#include "relay_proto.hpp"
#include "ini.hpp"
#include <cstdarg>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
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
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define closesocket close
#endif
extern "C" {
#include "ikcp.h"
}

struct Endpoint {
  sockaddr_in addr{};
  bool ok = false;
  uint32_t lastMs = 0;
};

struct RoomFwd {
  Endpoint host, guest;
};

struct FrameRec {
  int32_t frame = 0;
  uint32_t i0 = 0;
  uint32_t i1 = 0;
};

struct RoomPlay;
struct Sess {
  sockaddr_in addr{};
  IKCPCB* kcp = nullptr;
  SOCKET* sock = nullptr;
  char room[8]{};
  int role = -1;
  uint32_t lastMs = 0;
  bool alive = true;
  RoomPlay* play = nullptr;
};

struct RoomPlay {
  Sess* p[2]{};
  uint32_t seed = 1;
  int32_t confirmed = -1;
  char id[8]{};
  struct Slot {
    uint32_t bits[2]{};
    uint8_t got = 0;
    uint32_t readyMs = 0;
  };
  std::map<int32_t, Slot> pending;
  std::deque<FrameRec> hist;
};

static const int kMaxRelayRooms = 128;

static SOCKET gSock = INVALID_SOCKET;

static void logf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(stdout, fmt, ap);
  va_end(ap);
  std::fflush(stdout);
}

static uint32_t nowMs() {
  using namespace std::chrono;
  return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static bool sameAddr(const sockaddr_in& a, const sockaddr_in& b) {
  return a.sin_port == b.sin_port && a.sin_addr.s_addr == b.sin_addr.s_addr;
}

static std::string addrKey(const sockaddr_in& a) {
  char ip[64];
  inet_ntop(AF_INET, &a.sin_addr, ip, sizeof(ip));
  return std::string(ip) + ":" + std::to_string(ntohs(a.sin_port));
}

static int kcpOut(const char* buf, int len, ikcpcb*, void* user) {
  auto* s = (Sess*)user;
  if (!s || gSock == INVALID_SOCKET) return -1;
  int n = sendto(gSock, buf, len, 0, (sockaddr*)&s->addr, sizeof(s->addr));
  return n == len ? 0 : -1;
}

static void kcpSetup(Sess* s, uint32_t conv) {
  if (s->kcp) {
    ikcp_release(s->kcp);
    s->kcp = nullptr;
  }
  s->kcp = ikcp_create(conv, s);
  ikcp_setoutput(s->kcp, kcpOut);
  ikcp_nodelay(s->kcp, 1, 10, 2, 1);
  ikcp_wndsize(s->kcp, 256, 256);
}

static void sendFs(Sess* s, const FsMsg& m, const unsigned char* extra = nullptr, int extraN = 0) {
  if (!s || !s->kcp) return;
  fsDump("relay", "tx", m, kFsHdr + extraN);
  unsigned char buf[kFsHdr + 512];
  fsWrite(buf, m);
  if (extra && extraN > 0) std::memcpy(buf + kFsHdr, extra, (size_t)extraN);
  ikcp_send(s->kcp, (const char*)buf, kFsHdr + extraN);
  ikcp_update(s->kcp, nowMs());
  ikcp_flush(s->kcp);
}

static void sendCatchup(Sess* s, RoomPlay* rm, int32_t from) {
  if (!rm) return;
  unsigned char extra[480];
  int nrec = 0;
  int off = 0;
  auto wu32 = [](unsigned char* d, uint32_t v) {
    d[0] = (unsigned char)v;
    d[1] = (unsigned char)(v >> 8);
    d[2] = (unsigned char)(v >> 16);
    d[3] = (unsigned char)(v >> 24);
  };
  for (const auto& rec : rm->hist) {
    if (rec.frame < from) continue;
    if (nrec >= 40) break;
    wu32(extra + off, (uint32_t)rec.frame);
    wu32(extra + off + 4, rec.i0);
    wu32(extra + off + 8, rec.i1);
    off += 12;
    nrec++;
  }
  FsMsg m;
  m.cmd = kFsCatchupPack;
  m.frame = rm->confirmed;
  m.seed = rm->seed;
  m.count = (uint16_t)nrec;
  std::memcpy(m.room, s->room, 8);
  sendFs(s, m, extra, off);
}

static void sendConfirm(RoomPlay* rm, const FrameRec& rec) {
  FsMsg m;
  m.cmd = kFsConfirm;
  m.frame = rec.frame;
  m.a = rec.i0;
  m.b = rec.i1;
  m.seed = rm->seed;
  for (int i = 0; i < 2; i++) {
    if (rm->p[i]) {
      std::memcpy(m.room, rm->p[i]->room, 8);
      sendFs(rm->p[i], m);
    }
  }
}

static void tryConfirm(RoomPlay* rm, uint32_t t) {
  for (;;) {
    int32_t need = rm->confirmed + 1;
    auto it = rm->pending.find(need);
    if (it == rm->pending.end() || it->second.got != 3) break;
    FrameRec rec{need, it->second.bits[0], it->second.bits[1]};
    rm->hist.push_back(rec);
    while ((int)rm->hist.size() > 3600) rm->hist.pop_front();
    rm->confirmed = need;
    rm->pending.erase(it);
    if (rec.frame < 8 || rec.frame % 60 == 0 || rec.i0 || rec.i1) {
      logf("confirm room frame=%d p0=%u p1=%u hist=%d\n", rec.frame, rec.i0, rec.i1,
           (int)rm->hist.size());
    }
    sendConfirm(rm, rec);
  }
}

int main(int argc, char** argv) {
  NetCfg net = LoadNetIni();
  int port = net.port;
  if (argc > 1) port = std::atoi(argv[1]);
#ifdef _WIN32
  WSADATA w;
  WSAStartup(MAKEWORD(2, 2), &w);
#endif
  gSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (gSock == INVALID_SOCKET) {
    std::fprintf(stderr, "socket failed\n");
    return 1;
  }
  int opt = 1;
  setsockopt(gSock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
  sockaddr_in bindAddr{};
  bindAddr.sin_family = AF_INET;
  bindAddr.sin_port = htons((uint16_t)port);
  bindAddr.sin_addr.s_addr = INADDR_ANY;
  if (bind(gSock, (sockaddr*)&bindAddr, sizeof(bindAddr)) < 0) {
    std::fprintf(stderr, "bind %d failed\n", port);
    return 1;
  }
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
  logf("Ikemen KCP multi-room relay UDP %d  max_rooms=%d\n", port, kMaxRelayRooms);
  logf("each 8-char room id is an isolated 1v1 session\n");
  fsPrintSpec("relay");

  std::map<std::string, RoomFwd> fwd;
  std::map<std::string, Sess*> sessByAddr;
  std::map<std::string, RoomPlay*> plays;

  unsigned char buf[2048];
  for (;;) {
    fd_set r;
    FD_ZERO(&r);
    FD_SET(gSock, &r);
    timeval tv{0, 10000};
#ifdef _WIN32
    int nfds = 0;
#else
    int nfds = (int)gSock + 1;
#endif
    int sel = select(nfds, &r, nullptr, nullptr, &tv);
    uint32_t t = nowMs();
    if (sel > 0 && FD_ISSET(gSock, &r)) {
      sockaddr_in src{};
      socklen_t sl = sizeof(src);
      int n = recvfrom(gSock, (char*)buf, sizeof(buf), 0, (sockaddr*)&src, &sl);
      if (n >= kRlHdr && rlIs(buf, n)) {
        char roomKey[9] = {};
        std::memcpy(roomKey, buf + 5, 8);
        RoomFwd& rm = fwd[std::string(roomKey, 8)];
        uint8_t cmd = buf[4];
        auto stamp = [&](Endpoint& e) {
          e.addr = src;
          e.ok = true;
          e.lastMs = t;
        };
        if (cmd == kRlRegHost) {
          stamp(rm.host);
          std::fprintf(stderr, "fwd room %.8s host\n", roomKey);
        } else if (cmd == kRlRegGuest) {
          stamp(rm.guest);
        } else if (cmd == kRlKeep) {
          if (rm.host.ok && sameAddr(rm.host.addr, src)) rm.host.lastMs = t;
          if (rm.guest.ok && sameAddr(rm.guest.addr, src)) rm.guest.lastMs = t;
        } else if (cmd == kRlData && n > kRlHdr) {
          Endpoint* dst = nullptr;
          if (rm.host.ok && sameAddr(rm.host.addr, src)) {
            rm.host.lastMs = t;
            if (rm.guest.ok) dst = &rm.guest;
          } else if (rm.guest.ok && sameAddr(rm.guest.addr, src)) {
            rm.guest.lastMs = t;
            if (rm.host.ok) dst = &rm.host;
          }
          if (dst && dst->ok)
            sendto(gSock, (char*)buf, n, 0, (sockaddr*)&dst->addr, sizeof(dst->addr));
        }
      } else if (n >= 24) {
        std::string key = addrKey(src);
        Sess* s = sessByAddr[key];
        if (!s) {
          uint32_t conv = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) |
                          ((uint32_t)buf[3] << 24);
          s = new Sess();
          s->addr = src;
          kcpSetup(s, conv);
          sessByAddr[key] = s;
        }
        s->addr = src;
        s->lastMs = t;
        ikcp_input(s->kcp, (const char*)buf, n);
        ikcp_update(s->kcp, t);
        for (;;) {
          int got = ikcp_recv(s->kcp, (char*)buf, sizeof(buf));
          if (got <= 0) break;
          if (!fsIs(buf, got)) continue;
          FsMsg m = fsRead(buf);
          fsDump("relay", "rx", m, got);
          std::string rk(m.room, m.room + 8);
          if (m.cmd == kFsJoin) {
            int role = m.role ? 1 : 0;
            RoomPlay* rp = nullptr;
            auto pit = plays.find(rk);
            if (pit == plays.end()) {
              if ((int)plays.size() >= kMaxRelayRooms) {
                logf("join reject %.8s rooms full\n", m.room);
                continue;
              }
              rp = new RoomPlay();
              rp->seed = t ^ 0x9E3779B9u;
              std::memcpy(rp->id, m.room, 8);
              plays[rk] = rp;
            } else {
              rp = pit->second;
            }
            if (!rp) continue;
            bool wasBoth = rp->p[0] && rp->p[1];
            Sess* prev = rp->p[role];
            if (prev && prev != s) prev->alive = false;
            s->role = role;
            std::memcpy(s->room, m.room, 8);
            s->play = rp;
            rp->p[role] = s;
            bool reconnect = prev && prev != s;
            if (!prev || reconnect) {
              logf("join room %.8s role=%s %s%s rooms=%d\n", m.room, role ? "guest" : "host", key.c_str(),
                   reconnect ? " reconnect" : "", (int)plays.size());
            }
            bool nowBoth = rp->p[0] && rp->p[1];
            if (nowBoth && (!wasBoth || reconnect)) {
              logf("ready room %.8s latest=%d seed=%u fighting=%d\n", m.room, (int)rp->confirmed, rp->seed,
                   (int)plays.size());
              FsMsg ok;
              ok.cmd = kFsJoinOk;
              ok.seed = rp->seed;
              ok.frame = rp->confirmed;
              std::memcpy(ok.room, m.room, 8);
              sendFs(rp->p[0], ok);
              sendFs(rp->p[1], ok);
            }
          } else if (m.cmd == kFsInput && s->play && s->role >= 0) {
            RoomPlay* rp = s->play;
            int32_t fr = m.frame;
            if (fr < 0) continue;
            if (fr <= rp->confirmed) continue;
            auto& slot = rp->pending[fr];
            uint8_t prev = slot.got;
            slot.bits[s->role] = m.a;
            slot.got |= (uint8_t)(1u << s->role);
            if (slot.got == 3 && prev != 3) slot.readyMs = t;
            tryConfirm(rp, t);
          } else if (m.cmd == kFsCatchup && s->play) {
            if (m.frame == 0 || m.frame % 120 == 0) {
              logf("catchup room %.8s from=%d confirmed=%d\n", m.room, (int)m.frame,
                   (int)s->play->confirmed);
            }
            sendCatchup(s, s->play, m.frame);
          }
        }
        ikcp_flush(s->kcp);
      }
    }
    for (auto& kv : plays) {
      if (kv.second) tryConfirm(kv.second, t);
    }
    for (auto& kv : sessByAddr) {
      Sess* s = kv.second;
      if (!s || !s->kcp) continue;
      ikcp_update(s->kcp, t);
      ikcp_flush(s->kcp);
      if (t - s->lastMs > 90000) s->alive = false;
    }
    for (auto it = sessByAddr.begin(); it != sessByAddr.end();) {
      Sess* s = it->second;
      if (s && !s->alive) {
        if (s->play && s->role >= 0 && s->play->p[s->role] == s) {
          s->play->p[s->role] = nullptr;
          int other = s->role ^ 1;
          if (s->play->p[other]) {
            logf("drop room %.8s role=%d timeout\n", s->room, s->role);
            FsMsg d;
            d.cmd = kFsDrop;
            std::memcpy(d.room, s->room, 8);
            sendFs(s->play->p[other], d);
          }
        }
        if (s->kcp) ikcp_release(s->kcp);
        delete s;
        it = sessByAddr.erase(it);
      } else {
        ++it;
      }
    }
    for (auto it = plays.begin(); it != plays.end();) {
      RoomPlay* rp = it->second;
      if (rp && !rp->p[0] && !rp->p[1]) {
        logf("close room %.8s hist=%d left=%d\n", it->first.c_str(), (int)rp->hist.size(),
             (int)plays.size() - 1);
        delete rp;
        it = plays.erase(it);
      } else {
        ++it;
      }
    }
    for (auto it = fwd.begin(); it != fwd.end();) {
      RoomFwd& rm = it->second;
      if (rm.host.ok && t - rm.host.lastMs > 90000) rm.host.ok = false;
      if (rm.guest.ok && t - rm.guest.lastMs > 90000) rm.guest.ok = false;
      if (!rm.host.ok && !rm.guest.ok) it = fwd.erase(it);
      else ++it;
    }
    static uint32_t lastStat = 0;
    if (t - lastStat > 5000) {
      lastStat = t;
      int fight = 0;
      for (auto& kv : plays)
        if (kv.second && kv.second->p[0] && kv.second->p[1]) fight++;
      logf("relay rooms=%d fighting=%d sess=%d\n", (int)plays.size(), fight, (int)sessByAddr.size());
    }
  }
}
