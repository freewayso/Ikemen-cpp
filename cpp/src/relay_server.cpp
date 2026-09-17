#include "relay_proto.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
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
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define closesocket close
#endif

struct Endpoint {
  sockaddr_in addr{};
  bool ok = false;
  uint32_t lastMs = 0;
};

struct Room {
  Endpoint host, guest;
};

static uint32_t nowMs() {
  using namespace std::chrono;
  return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static bool sameAddr(const sockaddr_in& a, const sockaddr_in& b) {
  return a.sin_port == b.sin_port && a.sin_addr.s_addr == b.sin_addr.s_addr;
}

int main(int argc, char** argv) {
  int port = 9000;
  if (argc > 1) port = std::atoi(argv[1]);
#ifdef _WIN32
  WSADATA w;
  WSAStartup(MAKEWORD(2, 2), &w);
#endif
  SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == INVALID_SOCKET) {
    std::fprintf(stderr, "socket failed\n");
    return 1;
  }
  int opt = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
  sockaddr_in bindAddr{};
  bindAddr.sin_family = AF_INET;
  bindAddr.sin_port = htons((uint16_t)port);
  bindAddr.sin_addr.s_addr = INADDR_ANY;
  if (bind(s, (sockaddr*)&bindAddr, sizeof(bindAddr)) < 0) {
    std::fprintf(stderr, "bind %d failed\n", port);
    return 1;
  }
  std::fprintf(stderr, "Ikemen UDP relay on %d (rooms via --room on clients)\n", port);

  std::map<std::string, Room> rooms;
  unsigned char buf[2048];
  for (;;) {
    fd_set r;
    FD_ZERO(&r);
    FD_SET(s, &r);
    timeval tv{1, 0};
#ifdef _WIN32
    int nfds = 0;
#else
    int nfds = (int)s + 1;
#endif
    if (select(nfds, &r, nullptr, nullptr, &tv) <= 0) {
      uint32_t t = nowMs();
      for (auto it = rooms.begin(); it != rooms.end();) {
        bool dead = (!it->second.host.ok || t - it->second.host.lastMs > 30000) &&
                    (!it->second.guest.ok || t - it->second.guest.lastMs > 30000);
        if (dead) it = rooms.erase(it);
        else ++it;
      }
      continue;
    }
    sockaddr_in src{};
    socklen_t sl = sizeof(src);
    int n = recvfrom(s, (char*)buf, sizeof(buf), 0, (sockaddr*)&src, &sl);
    if (n < kRlHdr || !rlIs(buf, n)) continue;
    char roomKey[9] = {};
    std::memcpy(roomKey, buf + 5, 8);
    Room& rm = rooms[std::string(roomKey, 8)];
    uint8_t cmd = buf[4];
    uint32_t t = nowMs();
    auto stamp = [&](Endpoint& e) {
      e.addr = src;
      e.ok = true;
      e.lastMs = t;
    };
    if (cmd == kRlRegHost) {
      stamp(rm.host);
      std::fprintf(stderr, "room %.8s host %s:%d\n", roomKey, inet_ntoa(src.sin_addr), ntohs(src.sin_port));
    } else if (cmd == kRlRegGuest) {
      stamp(rm.guest);
      std::fprintf(stderr, "room %.8s guest %s:%d\n", roomKey, inet_ntoa(src.sin_addr), ntohs(src.sin_port));
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
      } else if (rm.host.ok && !rm.guest.ok) {
        stamp(rm.guest);
        dst = &rm.host;
      } else if (rm.guest.ok && !rm.host.ok) {
        stamp(rm.host);
        dst = &rm.guest;
      }
      if (dst && dst->ok)
        sendto(s, (char*)buf, n, 0, (sockaddr*)&dst->addr, sizeof(dst->addr));
    }
  }
}
