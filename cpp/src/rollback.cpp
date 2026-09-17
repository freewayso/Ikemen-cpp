#include "rollback.hpp"
#include "log.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <chrono>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socklen_t = int;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define closesocket close
#endif

static void ggpoWsa() {
#ifdef _WIN32
  static bool once = false;
  if (!once) { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); once = true; }
#endif
}
static uint32_t ggpoNow() {
  using namespace std::chrono;
  return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
static void ggpoNonblock(SOCKET s) {
#ifdef _WIN32
  u_long nb = 1;
  ioctlsocket(s, FIONBIO, &nb);
#else
  int fl = fcntl(s, F_GETFL, 0);
  if (fl >= 0) fcntl(s, F_SETFL, fl | O_NONBLOCK);
#endif
}
static void putBE16(uint8_t* b, uint16_t v) { b[0] = (uint8_t)(v >> 8); b[1] = (uint8_t)v; }
static void putBE32(uint8_t* b, uint32_t v) {
  b[0] = (uint8_t)(v >> 24); b[1] = (uint8_t)(v >> 16);
  b[2] = (uint8_t)(v >> 8); b[3] = (uint8_t)v;
}
static void putBE64(uint8_t* b, uint64_t v) {
  putBE32(b, (uint32_t)(v >> 32)); putBE32(b + 4, (uint32_t)v);
}
static uint16_t getBE16(const uint8_t* b) { return (uint16_t)((b[0] << 8) | b[1]); }
static uint32_t getBE32(const uint8_t* b) {
  return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}
static uint64_t getBE64(const uint8_t* b) {
  return ((uint64_t)getBE32(b) << 32) | getBE32(b + 4);
}

void RollbackStore::SaveSlot(int slot, int frame, const FighterSnapshot& a, const FighterSnapshot& b,
                             const FightWorld& world) {
  if (slot < 0 || slot >= kGgpoSlots) return;
  indexed_[slot].frame = frame;
  indexed_[slot].a = a;
  indexed_[slot].b = b;
  indexed_[slot].world = world;
}
bool RollbackStore::LoadSlot(int slot, int& frame, FighterSnapshot& a, FighterSnapshot& b, FightWorld& world) {
  if (slot < 0 || slot >= kGgpoSlots || indexed_[slot].frame < 0) return false;
  frame = indexed_[slot].frame;
  a = indexed_[slot].a;
  b = indexed_[slot].b;
  world = indexed_[slot].world;
  return true;
}
void RollbackStore::Save(int frame, const FighterSnapshot& a, const FighterSnapshot& b) {
  for (auto& s : slots_) {
    if (s.frame == frame) { s.a = a; s.b = b; return; }
  }
  slots_.push_back({frame, a, b, {}});
}
bool RollbackStore::Load(int frame, FighterSnapshot& a, FighterSnapshot& b) {
  for (auto& s : slots_) {
    if (s.frame == frame) { a = s.a; b = s.b; return true; }
  }
  return false;
}
uint32_t RollbackStore::Checksum(const FighterSnapshot& a, const FighterSnapshot& b) const {
  uint32_t h = 2166136261u;
  auto mix = [&](uint32_t v) { h ^= v; h *= 16777619u; };
  mix((uint32_t)a.state); mix((uint32_t)b.state);
  mix((uint32_t)a.life); mix((uint32_t)b.life);
  mix((uint32_t)(int)(a.pos.x * 1000)); mix((uint32_t)(int)(b.pos.x * 1000));
  mix(a.input); mix(b.input);
  return h;
}
void RollbackStore::Trim(int keepFrom) {
  slots_.erase(std::remove_if(slots_.begin(), slots_.end(),
                              [&](const Slot& s) { return s.frame < keepFrom; }),
               slots_.end());
}

static int prevHead(int h) { return h == 0 ? 127 : h - 1; }

bool GgpoPeer::InQueue::add(GameIn in) {
  if (!(lastUser == -1 || in.frame == lastUser + 1)) return false;
  lastUser = in.frame;
  int expected = first ? 0 : inputs[prevHead(head)].frame + 1;
  int frame = in.frame + delay;
  if (expected > frame) return true;
  while (expected < frame) {
    GameIn pad = first ? GameIn{} : inputs[prevHead(head)];
    pad.frame = expected;
    inputs[head] = pad;
    head = (head + 1) & 127;
    length++;
    lastAdded = expected;
    first = false;
    expected++;
  }
  in.frame = frame;
  inputs[head] = in;
  head = (head + 1) & 127;
  length++;
  first = false;
  lastAdded = frame;
  if (prediction.frame != -1) {
    if (firstIncorrect < 0 && std::memcmp(prediction.bits, in.bits, kGgpoInputSize) != 0)
      firstIncorrect = frame;
    if (prediction.frame == lastRequested && firstIncorrect < 0) prediction.frame = -1;
    else prediction.frame++;
  }
  return true;
}

bool GgpoPeer::InQueue::get(int requested, GameIn& out, bool& predicted) {
  lastRequested = requested;
  predicted = false;
  if (prediction.frame == -1) {
    if (!first && requested >= inputs[tail].frame) {
      int offset = requested - inputs[tail].frame;
      if (offset < length) {
        int idx = (tail + offset) & 127;
        if (inputs[idx].frame == requested) { out = inputs[idx]; return true; }
      }
    }
    if (lastAdded < 0 || requested == 0) {
      prediction = GameIn{};
    } else {
      prediction = inputs[prevHead(head)];
    }
    prediction.frame++;
  }
  predicted = true;
  out = prediction;
  out.frame = requested;
  return true;
}

void GgpoPeer::InQueue::resetPred(int) {
  prediction.frame = -1;
  firstIncorrect = -1;
  lastRequested = -1;
}

void GgpoPeer::InQueue::discard(int frame) {
  if (frame < 0) return;
  if (lastRequested != -1 && frame > lastRequested) frame = lastRequested;
  if (frame >= lastAdded) { tail = head; length = 0; return; }
  int offset = frame - inputs[tail].frame + 1;
  if (offset < 0) return;
  tail = (tail + offset) & 127;
  length -= offset;
  if (length < 0) length = 0;
}

bool GgpoPeer::Init(GgpoSession* cb, int localPort, int remotePort, const std::string& ip,
                    bool host, int frameDelay) {
  Close();
  ggpoWsa();
  cb_ = cb;
  host_ = host;
  local_ = host ? 0 : 1;
  delay_ = frameDelay < 0 ? 0 : (frameDelay > 8 ? 8 : frameDelay);
  q_[local_].setDelay(delay_);
  SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == INVALID_SOCKET) return false;
  int opt = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons((uint16_t)localPort);
  a.sin_addr.s_addr = INADDR_ANY;
  if (bind(s, (sockaddr*)&a, sizeof(a)) < 0) { closesocket(s); return false; }
  ggpoNonblock(s);
  sockaddr_in dst{};
  dst.sin_family = AF_INET;
  dst.sin_port = htons((uint16_t)remotePort);
  inet_pton(AF_INET, ip.empty() ? "127.0.0.1" : ip.c_str(), &dst.sin_addr);
  std::memcpy(peer_, &dst, sizeof(dst));
  peerLen_ = (int)sizeof(dst);
  sock_ = (uintptr_t)s;
  magic_ = (uint16_t)(ggpoNow() | 1);
  syncLeft_ = 5;
  lastSendMs_ = lastRecvMs_ = lastQualityMs_ = ggpoNow();
  sendSyncReq();
  GameLog::Get().Info("GGPO (ikemen binary) local %d remote %s:%d host=%d delay=%d",
               localPort, ip.empty() ? "127.0.0.1" : ip.c_str(), remotePort, (int)host, delay_);
  return true;
}

void GgpoPeer::Close() {
  if (sock_) closesocket((SOCKET)sock_);
  sock_ = 0;
  running_ = synchronized_ = rolling_ = false;
  frameCount_ = 0;
  lastConfirmed_ = -1;
  pendingN_ = 0;
  lastRecvIn_ = -1;
  lastAcked_ = -1;
  saveHead_ = 0;
  syncLeft_ = 5;
  remoteMagic_ = 0;
  sendSeq_ = 0;
  std::memset(q_, 0, sizeof(q_));
  q_[0].prediction.frame = q_[1].prediction.frame = -1;
  q_[0].lastUser = q_[1].lastUser = -1;
  q_[0].lastAdded = q_[1].lastAdded = -1;
  q_[0].firstIncorrect = q_[1].firstIncorrect = -1;
  q_[0].first = q_[1].first = true;
  std::memset(saved_, 0, sizeof(saved_));
  for (int i = 0; i < kGgpoSlots; i++) saved_[i].frame = -1;
  std::memset(pendingCkOk_, 0, sizeof(pendingCkOk_));
  std::memset(remoteCkOk_, 0, sizeof(remoteCkOk_));
  for (int i = 0; i < 4; i++) localLast_[i] = peerLast_[i] = -1;
  cb_ = nullptr;
}

const char* GgpoPeer::WaitLabel() const {
  if (running_) return "GGPO RUNNING";
  if (synchronized_) return "GGPO STARTING";
  return host_ ? "GGPO HOST UDP 7600" : "GGPO JOIN UDP 7550";
}

void GgpoPeer::sendRaw(const uint8_t* p, int n) {
  if (!sock_) return;
  sendto((SOCKET)sock_, (const char*)p, n, 0, (sockaddr*)peer_, peerLen_);
  lastSendMs_ = ggpoNow();
}

void GgpoPeer::setHdr(uint8_t* b, uint8_t type) {
  putBE16(b, magic_);
  putBE16(b + 2, sendSeq_++);
  b[4] = type;
}

void GgpoPeer::sendHdr(uint8_t type, const uint8_t* body, int bodyN) {
  uint8_t buf[512];
  setHdr(buf, type);
  if (bodyN > 0) std::memcpy(buf + 5, body, (size_t)bodyN);
  sendRaw(buf, 5 + bodyN);
}

void GgpoPeer::sendSyncReq() {
  syncRand_ = (uint32_t)(ggpoNow() & 0xFFFF);
  uint8_t body[8]{};
  putBE32(body, syncRand_);
  putBE16(body + 4, magic_);
  body[6] = (uint8_t)local_;
  body[7] = (uint8_t)delay_;
  sendHdr(TSyncReq, body, 8);
}

void GgpoPeer::sendSyncRep(uint32_t randv) {
  uint8_t body[4];
  putBE32(body, randv);
  sendHdr(TSyncRep, body, 4);
}

void GgpoPeer::sendInputAck(int32_t ack) {
  uint8_t body[4];
  putBE32(body, (uint32_t)ack);
  sendHdr(TInputAck, body, 4);
}

void GgpoPeer::sendQuality() {
  uint8_t body[9];
  body[0] = (uint8_t)(int8_t)(frameCount_ - lastRecvIn_);
  putBE64(body + 1, (uint64_t)ggpoNow());
  sendHdr(TQuality, body, 9);
  lastQualityMs_ = ggpoNow();
}

void GgpoPeer::sendPendingInput() {
  uint8_t buf[512];
  setHdr(buf, TInput);
  buf[5] = 4;
  int o = 6;
  for (int i = 0; i < 4; i++) {
    buf[o] = 0;
    putBE32(buf + o + 1, (uint32_t)localLast_[i]);
    o += 5;
  }
  int start = 0;
  uint8_t bits[64 * kGgpoInputSize];
  int nbits = 0;
  if (pendingN_ > 0) {
    start = pending_[0].frame;
    for (int i = 0; i < pendingN_; i++) {
      std::memcpy(bits + nbits, pending_[i].bits, kGgpoInputSize);
      nbits += kGgpoInputSize;
    }
  }
  putBE32(buf + o, (uint32_t)start); o += 4;
  buf[o++] = 0;
  putBE32(buf + o, (uint32_t)lastRecvIn_); o += 4;
  uint32_t ck = 0;
  if (pendingN_ > 0) ck = pending_[pendingN_ - 1].checksum;
  putBE32(buf + o, ck); o += 4;
  putBE16(buf + o, (uint16_t)kGgpoInputSize); o += 2;
  buf[o++] = (uint8_t)kGgpoInputSize;
  buf[o++] = (uint8_t)nbits;
  if (nbits) { std::memcpy(buf + o, bits, (size_t)nbits); o += nbits; }
  sendRaw(buf, o);
}

void GgpoPeer::saveCurrent() {
  saved_[saveHead_].frame = frameCount_;
  if (cb_) saved_[saveHead_].checksum = cb_->SaveGameState(saveHead_);
  saveHead_ = (saveHead_ + 1) % kGgpoSlots;
}

bool GgpoPeer::loadFrame(int frame) {
  int i = 0;
  for (; i < kGgpoSlots; i++) if (saved_[i].frame == frame) break;
  if (i == kGgpoSlots) return false;
  if (cb_) cb_->LoadGameState(i);
  frameCount_ = saved_[i].frame;
  saveHead_ = (i + 1) % kGgpoSlots;
  return true;
}

void GgpoPeer::onInputPkt(const uint8_t* p, int n) {
  if (n < 6) return;
  int nst = p[5];
  int o = 6;
  if (n < o + nst * 5 + 16) return;
  for (int i = 0; i < nst && i < 4; i++) {
    int32_t lf = (int32_t)getBE32(p + o + 1);
    if (lf > peerLast_[i]) peerLast_[i] = lf;
    o += 5;
  }
  uint32_t start = getBE32(p + o); o += 4;
  o++; // disconnect
  int32_t ack = (int32_t)getBE32(p + o); o += 4;
  uint32_t ck = getBE32(p + o); o += 4;
  o += 2; // numBits
  uint8_t inSize = p[o++];
  uint8_t nbits = p[o++];
  if (!inSize) inSize = kGgpoInputSize;
  if (o + nbits > n) return;
  int rem = local_ ^ 1;
  uint32_t cur = start;
  if (lastRecvIn_ < 0) lastRecvIn_ = (int)start - 1;
  int off = 0;
  while (off + inSize <= nbits) {
    if ((int)cur == lastRecvIn_ + 1) {
      GameIn gi;
      gi.frame = (int)cur;
      gi.size = inSize;
      gi.checksum = ck;
      std::memcpy(gi.bits, p + o + off, inSize > kGgpoInputSize ? kGgpoInputSize : inSize);
      q_[rem].add(gi);
      lastRecvIn_ = (int)cur;
      sendInputAck(lastRecvIn_);
      if (ck && gi.frame >= 16) {
        remoteCk_[(gi.frame - 16) & 127] = ck;
        remoteCkOk_[(gi.frame - 16) & 127] = true;
      }
    }
    off += inSize;
    cur++;
  }
  int w = 0;
  for (int i = 0; i < pendingN_; i++) {
    if (pending_[i].frame >= ack) pending_[w++] = pending_[i];
  }
  pendingN_ = w;
  lastAcked_ = ack;
}

void GgpoPeer::onPacket(const uint8_t* p, int n) {
  if (n < 5) return;
  uint16_t mag = getBE16(p);
  uint8_t t = p[4];
  if (running_ && remoteMagic_ && mag != remoteMagic_ && t != TSyncReq) return;
  if (t == TSyncReq) {
    uint32_t rnd = getBE32(p + 5);
    sendSyncRep(rnd);
    if (cb_ && !synchronized_) cb_->OnEvent({GgpoEvent::Connected});
  } else if (t == TSyncRep) {
    uint32_t echo = getBE32(p + 5);
    if (echo != syncRand_ || running_) return;
    if (!synchronized_ && cb_) cb_->OnEvent({GgpoEvent::Connected});
    syncLeft_--;
    if (cb_) cb_->OnEvent({GgpoEvent::Syncing, 5 - syncLeft_, 5, 0, 0});
    if (syncLeft_ <= 0) {
      synchronized_ = running_ = true;
      remoteMagic_ = mag;
      lastRecvIn_ = -1;
      if (cb_) {
        cb_->OnEvent({GgpoEvent::Synced, 5, 5, 0, 0});
        cb_->OnEvent({GgpoEvent::Running});
      }
    } else sendSyncReq();
  } else if (t == TInput) {
    if (!running_) {
      synchronized_ = running_ = true;
      remoteMagic_ = mag;
      if (cb_) cb_->OnEvent({GgpoEvent::Running});
    }
    onInputPkt(p, n);
  } else if (t == TInputAck) {
    int32_t ack = (int32_t)getBE32(p + 5);
    int w = 0;
    for (int i = 0; i < pendingN_; i++)
      if (pending_[i].frame >= ack) pending_[w++] = pending_[i];
    pendingN_ = w;
  } else if (t == TQuality) {
    uint8_t body[8];
    std::memcpy(body, p + 6, 8);
    sendHdr(TQualityRep, body, 8);
    remoteFrameAdv_ = (int8_t)p[5];
  } else if (t == TKeep) {
  }
}

void GgpoPeer::recvAll() {
  if (!sock_) return;
  for (;;) {
    uint8_t buf[512];
    sockaddr_storage src{};
    socklen_t sl = sizeof(src);
    int n = recvfrom((SOCKET)sock_, (char*)buf, sizeof(buf), 0, (sockaddr*)&src, &sl);
    if (n < 5) break;
    lastRecvMs_ = ggpoNow();
    std::memcpy(peer_, &src, (size_t)sl);
    peerLen_ = (int)sl;
    onPacket(buf, n);
  }
}

void GgpoPeer::checkRollback() {
  if (!running_ || rolling_ || !cb_) return;
  int seek = -1;
  for (int p = 0; p < kGgpoPlayers; p++) {
    int bad = q_[p].firstBad();
    if (bad >= 0 && (seek < 0 || bad < seek)) seek = bad;
  }
  if (seek < 0) return;
  int old = frameCount_;
  int count = frameCount_ - seek;
  rolling_ = true;
  if (!loadFrame(seek)) { rolling_ = false; return; }
  q_[0].resetPred(seek);
  q_[1].resetPred(seek);
  for (int i = 0; i < count; i++) cb_->AdvanceFrame(0);
  rolling_ = false;
  (void)old;
  int f = lastConfirmed_;
  if (f > 0) { q_[0].discard(f - 1); q_[1].discard(f - 1); }
}

void GgpoPeer::Idle() {
  if (!sock_) return;
  recvAll();
  uint32_t t = ggpoNow();
  if (!running_ && t - lastSendMs_ > 500) sendSyncReq();
  if (running_) {
    checkRollback();
    if (t - lastQualityMs_ > 1000) sendQuality();
    if (t - lastSendMs_ > 200) {
      if (pendingN_) sendPendingInput();
      else sendHdr(TKeep, nullptr, 0);
    }
    int minC = localLast_[local_];
    if (peerLast_[local_ ^ 1] < minC) minC = peerLast_[local_ ^ 1];
    if (minC >= 0) {
      lastConfirmed_ = minC;
      if (lastConfirmed_ > 0) {
        q_[0].discard(lastConfirmed_ - 1);
        q_[1].discard(lastConfirmed_ - 1);
      }
    }
    int cf = lastConfirmed_;
    if (cf >= 0 && remoteCkOk_[cf & 127] && pendingCkOk_[cf & 127] &&
        remoteCk_[cf & 127] != pendingCk_[cf & 127]) {
      if (cb_) cb_->OnEvent({GgpoEvent::Desync});
    }
    if (lastRecvMs_ && t - lastRecvMs_ > 5000 && cb_)
      cb_->OnEvent({GgpoEvent::Disconnect, 0, 0, local_ ^ 1, 0});
  }
}

bool GgpoPeer::AddLocalInput(const uint8_t* bytes, int n) {
  if (!running_ || rolling_) return false;
  recvAll();
  if (frameCount_ >= kGgpoMaxPred && frameCount_ - lastConfirmed_ >= kGgpoMaxPred) return false;
  if (frameCount_ == 0) saveCurrent();
  GameIn in;
  in.frame = frameCount_;
  int cpy = n < kGgpoInputSize ? n : kGgpoInputSize;
  if (bytes) std::memcpy(in.bits, bytes, (size_t)cpy);
  if (!q_[local_].add(in)) return false;
  int delayed = q_[local_].lastAdded;
  if (delayed < 0) return true;
  int ckFrame = delayed - 16;
  if (ckFrame >= 0 && pendingCkOk_[ckFrame & 127]) in.checksum = pendingCk_[ckFrame & 127];
  in.frame = delayed;
  localLast_[local_] = delayed;
  if (pendingN_ < 64) pending_[pendingN_++] = in;
  sendPendingInput();
  return true;
}

bool GgpoPeer::SyncInput(uint8_t out[kGgpoPlayers][kGgpoInputSize]) {
  recvAll();
  for (int p = 0; p < kGgpoPlayers; p++) {
    GameIn g;
    bool pred = false;
    q_[p].get(frameCount_, g, pred);
    std::memcpy(out[p], g.bits, kGgpoInputSize);
  }
  return true;
}

void GgpoPeer::AdvanceFrame(uint32_t checksum) {
  pendingCk_[frameCount_ & 127] = checksum;
  pendingCkOk_[frameCount_ & 127] = true;
  frameCount_++;
  saveCurrent();
  if (!rolling_) Idle();
}
