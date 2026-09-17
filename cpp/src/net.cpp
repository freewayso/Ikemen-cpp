#include "net.hpp"
#include "log.hpp"
#include "ikcp.h"
#include "relay_proto.hpp"
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <chrono>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socklen_t = int;
#else
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define closesocket close
#endif

static bool wsaInited = false;
static void ensureWsa() {
#ifdef _WIN32
  if (!wsaInited) { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); wsaInited = true; }
#endif
}

enum { kSelR = 1, kSelW = 2, kSelE = 4 };

static bool sockWouldBlock() {
#ifdef _WIN32
  int e = WSAGetLastError();
  return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS;
#else
  return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS;
#endif
}

// Portable readiness: Windows/POSIX select(), timeout 0 = poll (never block the game loop).
static int selPoll(SOCKET s, bool rd, bool wr, int timeoutMs) {
  if (s == INVALID_SOCKET) return -1;
  fd_set r, w, e;
  FD_ZERO(&r);
  FD_ZERO(&w);
  FD_ZERO(&e);
  fd_set* pr = nullptr;
  fd_set* pw = nullptr;
  if (rd) {
    FD_SET(s, &r);
    pr = &r;
  }
  if (wr) {
    FD_SET(s, &w);
    pw = &w;
  }
  FD_SET(s, &e);
  timeval tv{};
  tv.tv_sec = timeoutMs / 1000;
  tv.tv_usec = (long)(timeoutMs % 1000) * 1000;
#ifdef _WIN32
  int nfds = 0;
#else
  int nfds = (int)s + 1;
#endif
  int n = select(nfds, pr, pw, &e, timeoutMs < 0 ? nullptr : &tv);
  if (n < 0) return -1;
  if (n == 0) return 0;
  int m = 0;
  if (rd && FD_ISSET(s, &r)) m |= kSelR;
  if (wr && FD_ISSET(s, &w)) m |= kSelW;
  if (FD_ISSET(s, &e)) m |= kSelE;
  return m ? m : 0;
}

static uint32_t nowMs() {
  using namespace std::chrono;
  return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static const char kHello[5] = {'I', 'K', 'U', '1', 1};
static const char kHelloAck[5] = {'I', 'K', 'U', '1', 2};
static const IUINT32 kKcpConv = 0x314B4350u;

static int kcpUdpOut(const char* buf, int len, ikcpcb*, void* user);

static void sockSetNonBlock(SOCKET sock) {
#ifdef _WIN32
  u_long nb = 1;
  ioctlsocket(sock, FIONBIO, &nb);
#else
  int fl = fcntl(sock, F_GETFL, 0);
  if (fl >= 0) fcntl(sock, F_SETFL, fl | O_NONBLOCK);
#endif
}

void DelayNet::setNonBlock(uintptr_t s) {
  sockSetNonBlock((SOCKET)s);
}

void DelayNet::resetBuf() {
  std::memset(loc_, 0, sizeof(loc_));
  std::memset(rem_, 0, sizeof(rem_));
  time_ = locInp_ = remInp_ = locSen_ = remSen_ = 0;
  exchSent_ = false;
}

void DelayNet::eat(int n) { rxOff_ += n; }
uint32_t DelayNet::peekU32() const {
  const unsigned char* b = rx_.data() + rxOff_;
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

bool DelayNet::queueBytes(const void* p, int n) {
  if (tx_.size() > 4096) {
    tx_.erase(0, txOff_);
    txOff_ = 0;
    if (tx_.size() > 4096) tx_.clear();
  }
  tx_.append((const char*)p, (size_t)n);
  return true;
}

bool DelayNet::sendRaw(const void* p, int n) {
  SOCKET s = (SOCKET)sock_;
  if (!s || !peerOk_) return false;
  selPoll(s, false, true, 0);
  if (useRelay_) {
    unsigned char pkt[2048];
    if (n + kRlHdr > (int)sizeof(pkt) || n < 0) return false;
    pkt[0] = 'I'; pkt[1] = 'K'; pkt[2] = 'R'; pkt[3] = 'L';
    pkt[4] = (unsigned char)kRlData;
    std::memcpy(pkt + 5, room_, 8);
    std::memcpy(pkt + kRlHdr, p, (size_t)n);
    int r = sendto(s, (char*)pkt, n + kRlHdr, 0, (sockaddr*)peer_, peerLen_);
    return r == n + kRlHdr || (r < 0 && sockWouldBlock());
  }
  int r = sendto(s, (const char*)p, n, 0, (sockaddr*)peer_, peerLen_);
  return r == n || (r < 0 && sockWouldBlock());
}

void DelayNet::sendReg() {
  if (!useRelay_ || !peerOk_ || !sock_) return;
  unsigned char pkt[kRlHdr];
  pkt[0] = 'I'; pkt[1] = 'K'; pkt[2] = 'R'; pkt[3] = 'L';
  pkt[4] = (unsigned char)(host_ ? kRlRegHost : kRlRegGuest);
  std::memcpy(pkt + 5, room_, 8);
  sendto((SOCKET)sock_, (char*)pkt, kRlHdr, 0, (sockaddr*)peer_, peerLen_);
  lastKeepMs_ = nowMs();
}

void DelayNet::sendKeep() {
  if (!useRelay_ || !peerOk_ || !sock_) return;
  uint32_t t = nowMs();
  if (t - lastKeepMs_ < 800) return;
  unsigned char pkt[kRlHdr];
  pkt[0] = 'I'; pkt[1] = 'K'; pkt[2] = 'R'; pkt[3] = 'L';
  pkt[4] = (unsigned char)kRlKeep;
  std::memcpy(pkt + 5, room_, 8);
  sendto((SOCKET)sock_, (char*)pkt, kRlHdr, 0, (sockaddr*)peer_, peerLen_);
  lastKeepMs_ = t;
}

void DelayNet::SetRelay(const std::string& ip, int port, const std::string& room) {
  ensureWsa();
  sockaddr_in dst{};
  dst.sin_family = AF_INET;
  dst.sin_port = htons((uint16_t)port);
  inet_pton(AF_INET, ip.c_str(), &dst.sin_addr);
  std::memcpy(relay_, &dst, sizeof(dst));
  relayLen_ = (int)sizeof(dst);
  useRelay_ = true;
  rlPadRoom(room_, room.c_str());
  applyRelayPeer();
}

void DelayNet::applyRelayPeer() {
  if (!useRelay_) return;
  std::memcpy(peer_, relay_, (size_t)relayLen_);
  peerLen_ = relayLen_;
  peerOk_ = true;
}

bool DelayNet::recvRaw(unsigned char* buf, int maxn, int& n, bool updatePeer) {
  SOCKET s = (SOCKET)sock_;
  n = 0;
  sockaddr_storage src{};
  socklen_t sl = sizeof(src);
  int r = recvfrom(s, (char*)buf, maxn, 0, (sockaddr*)&src, &sl);
  if (r < 0) {
    if (sockWouldBlock()) return true;
    return false;
  }
  if (useRelay_) {
    if (!rlIs(buf, r)) { n = 0; return true; }
    uint8_t cmd = buf[4];
    if (cmd != kRlData || r <= kRlHdr) { n = 0; return true; }
    int pay = r - kRlHdr;
    if (pay > maxn) pay = maxn;
    std::memmove(buf, buf + kRlHdr, (size_t)pay);
    n = pay;
    return true;
  }
  n = r;
  if (updatePeer || !peerOk_) {
    std::memcpy(peer_, &src, (size_t)sl);
    peerLen_ = (int)sl;
    peerOk_ = true;
  }
  return true;
}

bool DelayNet::createKcp() {
  if (kcp_) return true;
  kcp_ = ikcp_create(kKcpConv, this);
  if (!kcp_) return false;
  ikcp_setoutput(kcp_, kcpUdpOut);
  ikcp_nodelay(kcp_, 1, 10, 2, 1);
  ikcp_wndsize(kcp_, 128, 128);
  kcp_->stream = 1;
  return true;
}

static int kcpUdpOut(const char* buf, int len, ikcpcb*, void* user) {
  auto* n = (DelayNet*)user;
  return n->sendRaw(buf, len) ? 0 : -1;
}

static void tcpNoDelay(SOCKET s) {
  int n = 1;
  setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&n, sizeof(n));
}

bool DelayNet::pumpIo() {
  SOCKET s = (SOCKET)sock_;
  if (!s) return false;
  if (useRelay_) {
    sendKeep();
    int ready = selPoll(s, true, false, 0);
    if (ready < 0) { hs_ = HsFail; return false; }
    if (ready & kSelR) {
      unsigned char buf[2048];
      for (;;) {
        int n = 0;
        if (!recvRaw(buf, sizeof(buf), n, host_ && !peerOk_)) {
          hs_ = HsFail;
          return false;
        }
        if (n <= 0) break;
        if (n >= 5 && std::memcmp(buf, "IKU1", 4) == 0) {
          if (buf[4] == 1 && host_) sendRaw(kHelloAck, 5);
          continue;
        }
        if (kcp_) ikcp_input(kcp_, (const char*)buf, n);
      }
    }
    if (kcp_) {
      ikcp_update(kcp_, nowMs());
      if (txOff_ < tx_.size()) {
        int rest = (int)(tx_.size() - txOff_);
        if (ikcp_send(kcp_, tx_.data() + txOff_, rest) >= 0) {
          tx_.clear();
          txOff_ = 0;
        }
      }
      ikcp_flush(kcp_);
      char rec[2048];
      for (;;) {
        int n = ikcp_recv(kcp_, rec, sizeof(rec));
        if (n <= 0) break;
        rx_.insert(rx_.end(), rec, rec + n);
      }
    }
  } else {
    int wantW = txOff_ < tx_.size();
    int ready = selPoll(s, true, wantW, 0);
    if (ready < 0) { hs_ = HsFail; return false; }
    if ((ready & kSelW) && txOff_ < tx_.size()) {
      int rest = (int)(tx_.size() - txOff_);
      int n = send(s, tx_.data() + txOff_, rest, 0);
      if (n > 0) {
        txOff_ += (size_t)n;
        if (txOff_ >= tx_.size()) { tx_.clear(); txOff_ = 0; }
      } else if (n < 0 && !sockWouldBlock()) { hs_ = HsFail; return false; }
    }
    if (ready & kSelR) {
      unsigned char buf[2048];
      int n = recv(s, (char*)buf, sizeof(buf), 0);
      if (n == 0) { hs_ = HsFail; return false; }
      if (n > 0) rx_.insert(rx_.end(), buf, buf + n);
      else if (!sockWouldBlock()) { hs_ = HsFail; return false; }
    }
  }
  if (rxOff_ > 0 && rxOff_ > (int)rx_.size() / 2) {
    rx_.erase(rx_.begin(), rx_.begin() + rxOff_);
    rxOff_ = 0;
  }
  return true;
}

const char* DelayNet::WaitLabel() const {
  if (useRelay_) {
    if (hs_ == HsConnect) return "RELAY JOIN";
    if (listening_ && !active_) return "RELAY HOST ROOM";
    if (hs_ != HsIdle && hs_ != HsDone) return "RELAY KCP HANDSHAKE";
    return "RELAY";
  }
  if (hs_ == HsConnect) return "CONNECTING";
  if (listening_ && !active_) {
    if (sock_) return "HANDSHAKE";
    return "WAITING FOR JOIN";
  }
  if (hs_ != HsIdle && hs_ != HsDone) return "SYNCING";
  return "WAITING";
}

bool DelayNet::Listen(int port) {
  ensureWsa();
  bool relay = useRelay_;
  char roomSav[8];
  std::memcpy(roomSav, room_, 8);
  unsigned char relSav[128];
  int relLen = relayLen_;
  std::memcpy(relSav, relay_, sizeof(relSav));
  Close();
  if (relay) {
    useRelay_ = true;
    std::memcpy(room_, roomSav, 8);
    std::memcpy(relay_, relSav, sizeof(relSav));
    relayLen_ = relLen;
    applyRelayPeer();
  }
  host_ = true;
  if (useRelay_) {
    SOCKET ls = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ls == INVALID_SOCKET) return false;
    int opt = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    setNonBlock((uintptr_t)ls);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = INADDR_ANY;
    a.sin_port = htons(0);
    if (bind(ls, (sockaddr*)&a, sizeof(a)) < 0) { closesocket(ls); return false; }
    sock_ = (uintptr_t)ls;
    listening_ = true;
    hs_ = HsIdle;
    sendReg();
    GameLog::Get().Info("DelayNet UDP+KCP via relay room %.8s", room_);
    return true;
  }
  SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (ls == INVALID_SOCKET) return false;
  int opt = 1;
  setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
  setNonBlock((uintptr_t)ls);
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = INADDR_ANY;
  a.sin_port = htons((uint16_t)port);
  if (bind(ls, (sockaddr*)&a, sizeof(a)) < 0) { closesocket(ls); return false; }
  if (listen(ls, 1) < 0) { closesocket(ls); return false; }
  listenSock_ = (uintptr_t)ls;
  listening_ = true;
  hs_ = HsIdle;
  GameLog::Get().Info("DelayNet TCP listen %d (IKEMENGO)", port);
  return true;
}

int DelayNet::TryAccept() {
  if (!listening_) return -1;
  if (useRelay_) {
    if (!sock_) return -1;
    sendKeep();
    int ready = selPoll((SOCKET)sock_, true, false, 0);
    if (ready < 0) return -1;
    if (!(ready & kSelR)) return 0;
    unsigned char buf[2048];
    int n = 0;
    if (!recvRaw(buf, sizeof(buf), n, true)) return -1;
    if (n < 5 || std::memcmp(buf, "IKU1", 4) != 0 || buf[4] != 1) return 0;
    if (!sendRaw(kHelloAck, 5)) return 0;
    if (!createKcp()) return -1;
    active_ = true;
    return 1;
  }
  if (sock_ && !active_) {
    if (!pumpIo()) return -1;
    if (!have(8)) return 0;
    if (std::memcmp(rx_.data() + rxOff_, "IKEMENGO", 8) != 0) return -1;
    eat(8);
    resetBuf();
    rx_.clear(); rxOff_ = 0;
    active_ = true;
    GameLog::Get().Info("TryAccept: peer IKEMENGO ok");
    return 1;
  }
  SOCKET ls = (SOCKET)listenSock_;
  if (!ls) return -1;
  int ready = selPoll(ls, true, false, 0);
  if (ready < 0) return -1;
  if (!(ready & kSelR)) return 0;
  sockaddr_in src{};
  socklen_t sl = sizeof(src);
  SOCKET c = accept(ls, (sockaddr*)&src, &sl);
  if (c == INVALID_SOCKET) {
    if (sockWouldBlock()) return 0;
    return -1;
  }
  tcpNoDelay(c);
  setNonBlock((uintptr_t)c);
  sock_ = (uintptr_t)c;
  peerOk_ = true;
  queueBytes("IKEMENGO", 8);
  if (!pumpIo()) return -1;
  GameLog::Get().Info("TryAccept: accepted, sent IKEMENGO");
  return 0;
}

bool DelayNet::BeginConnect(const std::string& ip, int port) {
  ensureWsa();
  bool relay = useRelay_;
  char roomSav[8];
  std::memcpy(roomSav, room_, 8);
  unsigned char relSav[128];
  int relLen = relayLen_;
  std::memcpy(relSav, relay_, sizeof(relSav));
  Close();
  if (relay) {
    useRelay_ = true;
    std::memcpy(room_, roomSav, 8);
    std::memcpy(relay_, relSav, sizeof(relSav));
    relayLen_ = relLen;
  }
  host_ = false;
  if (useRelay_) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return false;
    setNonBlock((uintptr_t)s);
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = 0;
    bind(s, (sockaddr*)&local, sizeof(local));
    applyRelayPeer();
    sock_ = (uintptr_t)s;
    hs_ = HsConnect;
    lastHelloMs_ = 0;
    sendReg();
    sendRaw(kHello, 5);
    lastHelloMs_ = nowMs();
    return true;
  }
  SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET) return false;
  tcpNoDelay(s);
  setNonBlock((uintptr_t)s);
  sockaddr_in dst{};
  dst.sin_family = AF_INET;
  dst.sin_port = htons((uint16_t)port);
  inet_pton(AF_INET, ip.c_str(), &dst.sin_addr);
  int r = connect(s, (sockaddr*)&dst, sizeof(dst));
  if (r < 0 && !sockWouldBlock()) { closesocket(s); return false; }
  sock_ = (uintptr_t)s;
  peerOk_ = false;
  hs_ = HsConnect;
  lastHelloMs_ = nowMs();
  return true;
}

NetPump DelayNet::PumpConnect() {
  if (hs_ != HsConnect) return hs_ == HsFail ? NetPump::Fail : NetPump::Ready;
  if (useRelay_) {
    uint32_t t = nowMs();
    if (t - lastHelloMs_ >= 200) {
      sendReg();
      sendRaw(kHello, 5);
      lastHelloMs_ = t;
    }
    int ready = selPoll((SOCKET)sock_, true, false, 0);
    if (ready < 0) { hs_ = HsFail; return NetPump::Fail; }
    if (ready & kSelR) {
      unsigned char buf[2048];
      int n = 0;
      if (!recvRaw(buf, sizeof(buf), n, false)) { hs_ = HsFail; return NetPump::Fail; }
      if (n >= 5 && std::memcmp(buf, "IKU1", 4) == 0 && buf[4] == 2) {
        if (!createKcp()) { hs_ = HsFail; return NetPump::Fail; }
        active_ = true;
        hs_ = HsIdle;
        return NetPump::Ready;
      }
    }
    return NetPump::Pending;
  }
  SOCKET s = (SOCKET)sock_;
  if (!peerOk_) {
    int ready = selPoll(s, false, true, 0);
    if (ready < 0) { hs_ = HsFail; return NetPump::Fail; }
    if (!(ready & kSelW) && !(ready & kSelE)) return NetPump::Pending;
    int err = 0;
    socklen_t el = sizeof(err);
    getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &el);
    if (err) { hs_ = HsFail; return NetPump::Fail; }
    peerOk_ = true;
  }
  if (!pumpIo()) return NetPump::Fail;
  if (!have(8)) return NetPump::Pending;
  if (std::memcmp(rx_.data() + rxOff_, "IKEMENGO", 8) != 0) { hs_ = HsFail; return NetPump::Fail; }
  eat(8);
  queueBytes("IKEMENGO", 8);
  for (int i = 0; i < 64 && txOff_ < tx_.size(); i++) {
    if (!pumpIo()) return NetPump::Fail;
  }
  active_ = true;
  hs_ = HsIdle;
  GameLog::Get().Info("PumpConnect: magic ok");
  return NetPump::Ready;
}

void DelayNet::BeginHandshake(const std::string& fingerprint) {
  fingerprint_ = fingerprint;
  for (int i = 0; i < 64 && txOff_ < tx_.size(); i++) pumpIo();
  tx_.clear(); txOff_ = 0;
  jsonLen_ = 0;
  dummyLeft_ = 0;
  if (host_) {
    rx_.clear();
    rxOff_ = 0;
    seed_ = (uint32_t)std::rand();
  }
  time_ = 0;
  hs_ = HsJsonRecvLen;
  if (host_) {
    std::string local = "{\"sync_version\":1,\"host\":[{\"path\":\"Netplay.Rollback.FrameDelay\",\"value\":\"2\"}]";
    if (!fingerprint_.empty())
      local += std::string(",\"content_fingerprint\":\"") + fingerprint_ + "\"";
    local += "}";
    int32_t n = (int32_t)local.size();
    unsigned char b[4] = {(unsigned char)n, (unsigned char)(n >> 8), (unsigned char)(n >> 16), (unsigned char)(n >> 24)};
    queueBytes(b, 4);
    queueBytes(local.data(), (int)local.size());
  }
  GameLog::Get().Info("BeginHandshake host=%d rx=%d", (int)host_, (int)rx_.size() - rxOff_);
}

static std::string jsonGet(const std::string& j, const char* key) {
  std::string k = std::string("\"") + key + "\":";
  auto p = j.find(k);
  if (p == std::string::npos) return "";
  p += k.size();
  while (p < j.size() && j[p] == ' ') p++;
  if (p < j.size() && j[p] == '"') {
    auto e = j.find('"', p + 1);
    return e == std::string::npos ? "" : j.substr(p + 1, e - p - 1);
  }
  auto e = j.find_first_of(",}", p);
  return e == std::string::npos ? j.substr(p) : j.substr(p, e - p);
}

NetPump DelayNet::PumpHandshake() {
  if (hs_ == HsDone) return NetPump::Ready;
  if (hs_ == HsFail || hs_ == HsIdle) return NetPump::Fail;
  if (!pumpIo()) return NetPump::Fail;

  auto jsonLocal = [&]() {
    std::string local = "{\"sync_version\":1,\"host\":[{\"path\":\"Netplay.Rollback.FrameDelay\",\"value\":\"2\"}]";
    if (!fingerprint_.empty())
      local += std::string(",\"content_fingerprint\":\"") + fingerprint_ + "\"";
    local += "}";
    int32_t n = (int32_t)local.size();
    unsigned char b[4] = {(unsigned char)n, (unsigned char)(n >> 8), (unsigned char)(n >> 16), (unsigned char)(n >> 24)};
    queueBytes(b, 4);
    queueBytes(local.data(), (int)local.size());
  };
  auto putI32 = [](unsigned char* b, int32_t v) {
    b[0] = (unsigned char)v; b[1] = (unsigned char)(v >> 8);
    b[2] = (unsigned char)(v >> 16); b[3] = (unsigned char)(v >> 24);
  };

  if (hs_ == HsJsonRecvLen) {
    if (!have(4)) return NetPump::Pending;
    jsonLen_ = (int32_t)peekU32();
    if (jsonLen_ < 0 || jsonLen_ > 1 << 20) { hs_ = HsFail; return NetPump::Fail; }
    eat(4);
    hs_ = HsJsonRecvBody;
  }
  if (hs_ == HsJsonRecvBody) {
    if (!have(jsonLen_)) return NetPump::Pending;
    std::string js((char*)rx_.data() + rxOff_, (size_t)jsonLen_);
    eat(jsonLen_);
    std::string ver = jsonGet(js, "sync_version");
    if (ver != "1") { hs_ = HsFail; return NetPump::Fail; }
    std::string fp = jsonGet(js, "content_fingerprint");
    if (!fingerprint_.empty() && !fp.empty() && fingerprint_ != fp) { hs_ = HsFail; return NetPump::Fail; }
    if (host_) {
      unsigned char b[12];
      putI32(b, (int32_t)seed_);
      putI32(b + 4, 0);
      putI32(b + 8, time_);
      queueBytes(b, 12);
      hs_ = HsTimeAck;
    } else {
      jsonLocal();
      hs_ = HsSeedRecv;
    }
  }
  if (hs_ == HsSeedRecv) {
    if (!have(8)) return NetPump::Pending;
    seed_ = peekU32();
    eat(4);
    eat(4);
    unsigned char t[4];
    putI32(t, time_);
    queueBytes(t, 4);
    hs_ = HsTimeAck;
  }
  if (hs_ == HsTimeAck) {
    if (!have(4)) return NetPump::Pending;
    int32_t remoteT = (int32_t)peekU32();
    eat(4);
    if (remoteT != time_) { hs_ = HsFail; return NetPump::Fail; }
    hs_ = HsDone;
    resetBuf();
    locInp_ = remInp_ = locSen_ = remSen_ = 0;
    time_ = 0;
    if (!pumpIo()) return NetPump::Fail;
    return NetPump::Ready;
  }
  if (!pumpIo()) return NetPump::Fail;
  return NetPump::Pending;
}

static void packGoInput(uint32_t bits, unsigned char out[8]) {
  uint16_t v = (uint16_t)(bits & 0x3FFF);
  out[0] = (unsigned char)v;
  out[1] = (unsigned char)(v >> 8);
  std::memset(out + 2, 0, 6);
}
static uint32_t unpackGoInput(const unsigned char in[8]) {
  return (uint32_t)in[0] | ((uint32_t)in[1] << 8);
}

NetPump DelayNet::TryExchange(uint32_t localNow, uint32_t& localPlay, uint32_t& remotePlay) {
  if (hs_ != HsDone) return NetPump::Fail;
  // Queue a few in-flight inputs (Go delay buffer). Never wait on a single unsent packet.
  if (locInp_ - time_ < 8) {
    loc_[locInp_ & 31] = localNow;
    unsigned char pkt[8];
    packGoInput(localNow, pkt);
    queueBytes(pkt, 8);
    locInp_++;
    locSen_ = locInp_;
    int32_t tmp = remInp_ + (delay_ >> 3) - locInp_;
    if (tmp >= 0) { if (delay_ > 0) delay_--; }
    else if (tmp < -1) { delay_ += 4; if (delay_ > 16) delay_ = 16; }
  }
  if (!pumpIo()) return NetPump::Fail;
  if (!pumpIo()) return NetPump::Fail;
  while (have(8)) {
    unsigned char in[8];
    std::memcpy(in, rx_.data() + rxOff_, 8);
    eat(8);
    rem_[remInp_ & 31] = unpackGoInput(in);
    remInp_++;
    remSen_ = remInp_;
  }
  if (time_ >= locSen_ || time_ >= remSen_) {
    stall_++;
    if (stall_ >= 8) {
      if (time_ >= remSen_) {
        uint32_t pred = remSen_ > 0 ? rem_[(remSen_ - 1) & 31] : 0;
        rem_[remInp_ & 31] = pred;
        remInp_++;
        remSen_ = remInp_;
      }
      if (time_ >= locSen_) {
        loc_[locInp_ & 31] = localNow;
        locInp_++;
        locSen_ = locInp_;
      }
    } else {
      return NetPump::Pending;
    }
  }
  stall_ = 0;
  localPlay = loc_[time_ & 31];
  remotePlay = rem_[time_ & 31];
  time_++;
  return NetPump::Ready;
}

void DelayNet::Close() {
  if (kcp_) { ikcp_release(kcp_); kcp_ = nullptr; }
  if (sock_) closesocket((SOCKET)sock_);
  if (listenSock_) closesocket((SOCKET)listenSock_);
  sock_ = 0;
  listenSock_ = 0;
  listening_ = false;
  active_ = false;
  if (!useRelay_) peerOk_ = false;
  hs_ = HsIdle;
  tx_.clear(); txOff_ = 0;
  rx_.clear(); rxOff_ = 0;
  exchSent_ = false;
}
void RollbackNet::Init(int localPort, int remotePort, const std::string& ip, bool host, int delay) {
  ensureWsa();
  delay_ = delay;
  SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons((uint16_t)localPort);
  a.sin_addr.s_addr = INADDR_ANY;
  bind(s, (sockaddr*)&a, sizeof(a));
  sockSetNonBlock(s);
  sockaddr_in dst{};
  dst.sin_family = AF_INET;
  dst.sin_port = htons((uint16_t)remotePort);
  inet_pton(AF_INET, ip.empty() ? "127.0.0.1" : ip.c_str(), &dst.sin_addr);
  connect(s, (sockaddr*)&dst, sizeof(dst));
  sock_ = (uintptr_t)s;
  active_ = true;
  (void)host;
}

void RollbackNet::SendFrame(int32_t frame, uint32_t input, uint32_t cksum) {
  SOCKET s = (SOCKET)sock_;
  if (selPoll(s, false, true, 0) < 0) return;
  RbPkt p{};
  p.frame = frame;
  p.cksum = cksum;
  p.bits = (int16_t)(input & 0x3FFF);
  std::memset(p.axes, 0, sizeof(p.axes));
  send(s, (char*)&p, sizeof(p), 0);
}

int RollbackNet::Poll(RbPkt* out, int maxn) {
  SOCKET s = (SOCKET)sock_;
  int n = 0;
  for (; n < maxn; n++) {
    int ready = selPoll(s, true, false, 0);
    if (ready <= 0 || !(ready & kSelR)) break;
    RbPkt p{};
    if (recv(s, (char*)&p, sizeof(p), 0) != (int)sizeof(p)) break;
    out[n] = p;
    lastRemote_ = (uint32_t)(uint16_t)p.bits;
  }
  return n;
}

bool RollbackNet::ExchangePredicted(uint32_t local, uint32_t& remoteOut, bool& confirmed) {
  SendFrame(0, local, 0);
  RbPkt pkts[8];
  int n = Poll(pkts, 8);
  confirmed = n > 0;
  remoteOut = lastRemote_;
  if (n > 0) remoteOut = (uint32_t)(uint16_t)pkts[n - 1].bits;
  return true;
}

void RollbackNet::Close() {
  if (sock_) closesocket((SOCKET)sock_);
  sock_ = 0;
  active_ = false;
}
