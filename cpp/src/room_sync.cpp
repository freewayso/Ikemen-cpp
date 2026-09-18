#include "room_sync.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socklen_t = int;
#else
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define closesocket close
#endif
extern "C" {
#include "ikcp.h"
}

static uint32_t nowMs() {
  using namespace std::chrono;
  return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static void sockNb(SOCKET s) {
#ifdef _WIN32
  u_long n = 1;
  ioctlsocket(s, FIONBIO, &n);
#else
  fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
}

int RoomSync::UdpSend(const char* buf, int len) {
  SOCKET s = (SOCKET)sock_;
  if (!s || peerLen_ <= 0) return -1;
  int n = sendto(s, buf, len, 0, (sockaddr*)peer_, peerLen_);
  return n == len ? 0 : -1;
}

static int kcpOut(const char* buf, int len, ikcpcb*, void* user) {
  return ((RoomSync*)user)->UdpSend(buf, len);
}

bool RoomSync::createKcp() {
  if (kcp_) return true;
  uint32_t conv = host_ ? 0x4B465301u : 0x4B465302u;
  kcp_ = ikcp_create(conv, this);
  if (!kcp_) return false;
  ikcp_setoutput(kcp_, kcpOut);
  ikcp_nodelay(kcp_, 1, 10, 2, 1);
  ikcp_wndsize(kcp_, 256, 256);
  return true;
}

bool RoomSync::Start(bool host, const std::string& ip, int port, const std::string& room) {
  Close();
#ifdef _WIN32
  WSADATA w;
  WSAStartup(MAKEWORD(2, 2), &w);
#endif
  host_ = host;
  rlPadRoom(room_, room.c_str());
  SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == INVALID_SOCKET) return false;
  sockNb(s);
  sockaddr_in bindA{};
  bindA.sin_family = AF_INET;
  bindA.sin_addr.s_addr = INADDR_ANY;
  bindA.sin_port = 0;
  if (bind(s, (sockaddr*)&bindA, sizeof(bindA)) < 0) {
    closesocket(s);
    return false;
  }
  sockaddr_in dst{};
  dst.sin_family = AF_INET;
  dst.sin_port = htons((uint16_t)port);
  if (inet_pton(AF_INET, ip.empty() ? "127.0.0.1" : ip.c_str(), &dst.sin_addr) != 1) {
    closesocket(s);
    return false;
  }
  std::memcpy(peer_, &dst, sizeof(dst));
  peerLen_ = (int)sizeof(dst);
  sock_ = (uintptr_t)s;
  if (!createKcp()) {
    Close();
    return false;
  }
  active_ = true;
  lastJoinMs_ = nowMs();
  FsMsg j;
  j.cmd = kFsJoin;
  j.role = host_ ? 0 : 1;
  std::memcpy(j.room, room_, 8);
  sendMsg(j);
  fsPrintSpec(host_ ? "host" : "guest");
  std::fprintf(stderr, "room-sync join %s %s:%d room %.8s\n", host_ ? "host" : "guest",
               ip.empty() ? "127.0.0.1" : ip.c_str(), port, room_);
  return true;
}

void RoomSync::sendMsg(const FsMsg& m) {
  if (!kcp_) return;
  fsDump(host_ ? "host" : "guest", "tx", m);
  unsigned char buf[kFsHdr];
  fsWrite(buf, m);
  ikcp_send(kcp_, (const char*)buf, kFsHdr);
  ikcp_update(kcp_, nowMs());
  ikcp_flush(kcp_);
}

void RoomSync::requestCatchup(int32_t from) {
  uint32_t t = nowMs();
  if (t - lastCatchMs_ < 80) return;
  lastCatchMs_ = t;
  FsMsg m;
  m.cmd = kFsCatchup;
  m.role = host_ ? 0 : 1;
  std::memcpy(m.room, room_, 8);
  m.frame = from;
  sendMsg(m);
}

void RoomSync::onPayload(const unsigned char* p, int n) {
  if (!fsIs(p, n)) return;
  FsMsg m = fsRead(p);
  fsDump(host_ ? "host" : "guest", "rx", m, n);
  if (m.cmd == kFsJoinOk) {
    if (joined_) return;
    seed_ = m.seed;
    serverLatest_ = m.frame;
    joined_ = true;
    want_ = 0;
    ready_.clear();
    std::fprintf(stderr, "room-sync joined seed=%u latest=%d\n", seed_, (int)serverLatest_);
    if (serverLatest_ >= 0) requestCatchup(0);
    return;
  }
  if (m.cmd == kFsDrop) {
    dropped_ = true;
    return;
  }
  auto push = [&](int32_t fr, uint32_t a, uint32_t b) {
    if (fr < want_) return;
    ready_[fr] = {a, b};
    if (fr > serverLatest_) serverLatest_ = fr;
  };
  if (m.cmd == kFsConfirm) {
    push(m.frame, m.a, m.b);
    return;
  }
  if (m.cmd == kFsCatchupPack) {
    int off = kFsHdr;
    int cnt = m.count;
    for (int i = 0; i < cnt; i++) {
      if (off + 12 > n) break;
      auto ru32 = [](const unsigned char* d) -> uint32_t {
        return (uint32_t)d[0] | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16) | ((uint32_t)d[3] << 24);
      };
      int32_t fr = (int32_t)ru32(p + off);
      uint32_t a = ru32(p + off + 4);
      uint32_t b = ru32(p + off + 8);
      push(fr, a, b);
      off += 12;
    }
  }
}

void RoomSync::Pump() {
  if (!active_ || !kcp_) return;
  SOCKET s = (SOCKET)sock_;
  unsigned char buf[2048];
  for (;;) {
    sockaddr_storage src{};
    socklen_t sl = sizeof(src);
    int n = recvfrom(s, (char*)buf, sizeof(buf), 0, (sockaddr*)&src, &sl);
    if (n <= 0) break;
    ikcp_input(kcp_, (const char*)buf, n);
  }
  ikcp_update(kcp_, nowMs());
  ikcp_flush(kcp_);
  for (;;) {
    int n = ikcp_recv(kcp_, (char*)buf, sizeof(buf));
    if (n <= 0) break;
    onPayload(buf, n);
  }
  if (!joined_) {
    uint32_t t = nowMs();
    if (t - lastJoinMs_ > 400) {
      lastJoinMs_ = t;
      FsMsg j;
      j.cmd = kFsJoin;
      j.role = host_ ? 0 : 1;
      std::memcpy(j.room, room_, 8);
      sendMsg(j);
    }
  } else if (serverLatest_ >= want_ && want_ >= 0 && ready_.find(want_) == ready_.end()) {
    requestCatchup(want_);
  }
}

void RoomSync::SendInput(int32_t frame, uint32_t bits) {
  if (!joined_ || !kcp_) return;
  if (frame == lastSentFrame_ && bits == lastSentBits_) {
    uint32_t t = nowMs();
    if (t - lastInputMs_ < 40) return;
  }
  lastInputMs_ = nowMs();
  lastSentFrame_ = frame;
  lastSentBits_ = bits;
  FsMsg m;
  m.cmd = kFsInput;
  m.role = host_ ? 0 : 1;
  std::memcpy(m.room, room_, 8);
  m.frame = frame;
  m.a = bits;
  sendMsg(m);
}

bool RoomSync::NextConfirm(uint32_t& p0, uint32_t& p1) {
  auto it = ready_.find(want_);
  if (it == ready_.end()) return false;
  p0 = it->second.first;
  p1 = it->second.second;
  ready_.erase(it);
  want_++;
  return true;
}

const char* RoomSync::WaitLabel() const {
  if (!active_) return "ROOM";
  if (!joined_) return host_ ? "ROOM HOST  START RELAY :9000" : "ROOM JOIN  WAIT HOST";
  return "ROOM FRAME SYNC";
}

void RoomSync::Close() {
  if (kcp_) {
    ikcp_release(kcp_);
    kcp_ = nullptr;
  }
  if (sock_) closesocket((SOCKET)sock_);
  sock_ = 0;
  active_ = false;
  joined_ = false;
  dropped_ = false;
  want_ = 0;
  lastSentFrame_ = -1;
  serverLatest_ = -1;
  ready_.clear();
  peerLen_ = 0;
}
