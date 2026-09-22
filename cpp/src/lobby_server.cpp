// Login + 2-player rooms. Protocol: proto3 Envelope over TCP (see cpp/proto/lobby.proto).
// Accounts: protobuf file (IKUS + UserStore + sha256). Passwords stored as salted sha256, not plaintext.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socklen_t = int;
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define closesocket close
#endif
#include "lobby_proto.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

struct Client {
  SOCKET s = INVALID_SOCKET;
  std::string rx;
  std::string user;
  std::string room;
  int role = -1;
};

struct Room {
  std::string id;
  std::string name;
  std::string host;
  SOCKET seats[2] = {INVALID_SOCKET, INVALID_SOCKET};
  int status = 0;  // 0 wait, 1 fight
  int n() const { return (seats[0] != INVALID_SOCKET) + (seats[1] != INVALID_SOCKET); }
};

static const int kMaxLobbyRooms = 64;
static const int kMaxRoomPlayers = 2;

static std::string gUsersPath = "data/users.pb";
struct UserRec {
  std::string salt;
  std::string hash;
};
static std::map<std::string, UserRec> gUsers;
static std::vector<Client> gCli;
static std::vector<Room> gRooms;
static unsigned gNextRoom = 1;

static bool validName(const std::string& s) {
  if (s.empty() || s.size() > 16) return false;
  for (unsigned char c : s)
    if (!std::isalnum(c) && c != '_') return false;
  return true;
}

static std::string hashPass(const std::string& salt, const std::string& pass) { return sha256(salt + pass); }

static std::string newSalt() {
  std::string s(16, '\0');
  static uint32_t ctr = 1;
  uint32_t t = (uint32_t)std::chrono::steady_clock::now().time_since_epoch().count();
  ctr += t * 1664525u + 1013904223u;
  for (int i = 0; i < 16; i++) {
    ctr = ctr * 1103515245u + 12345u + (uint32_t)i;
    s[i] = (char)((ctr >> 16) ^ (t >> (i % 24)));
  }
  return s;
}

static bool writeUsersPb() {
  std::vector<lb::Account> acc;
  acc.reserve(gUsers.size());
  for (auto& kv : gUsers) acc.push_back({kv.first, kv.second.salt, kv.second.hash});
  std::string body = lb::encodeUserStore(1, acc);
  std::string sum = sha256(body);
  uint32_t n = (uint32_t)body.size();
  std::string file = "IKUS";
  file.push_back((char)(n & 255));
  file.push_back((char)((n >> 8) & 255));
  file.push_back((char)((n >> 16) & 255));
  file.push_back((char)((n >> 24) & 255));
  file += body;
  file += sum;
  std::ofstream out(gUsersPath, std::ios::binary | std::ios::trunc);
  if (!out) return false;
  out.write(file.data(), (std::streamsize)file.size());
  return true;
}

static bool readUsersPb() {
  std::ifstream in(gUsersPath, std::ios::binary);
  if (!in) return false;
  std::string file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (file.size() < 8 + 32 || file.compare(0, 4, "IKUS") != 0) return false;
  uint32_t n = (uint8_t)file[4] | ((uint32_t)(uint8_t)file[5] << 8) | ((uint32_t)(uint8_t)file[6] << 16) |
               ((uint32_t)(uint8_t)file[7] << 24);
  if (file.size() != 8 + n + 32) return false;
  std::string body = file.substr(8, n);
  if (sha256(body) != file.substr(8 + n, 32)) {
    std::fprintf(stderr, "users.pb checksum fail\n");
    return false;
  }
  uint32_t ver = 0;
  std::vector<lb::Account> acc;
  lb::decodeUserStore((const uint8_t*)body.data(), body.size(), ver, acc);
  gUsers.clear();
  for (auto& a : acc) {
    if (a.user.empty() || a.pass_hash.size() != 32) continue;
    gUsers[a.user] = {a.salt, a.pass_hash};
  }
  return true;
}

static void importUsersTxt(const std::string& path) {
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    auto sp = line.find(' ');
    std::string u = sp == std::string::npos ? line : line.substr(0, sp);
    std::string p = sp == std::string::npos ? "" : line.substr(sp + 1);
    if (u.empty() || gUsers.count(u)) continue;
    std::string salt = newSalt();
    gUsers[u] = {salt, hashPass(salt, p)};
  }
}

static void loadUsers() {
  gUsers.clear();
  if (readUsersPb()) {
    std::fprintf(stderr, "users.pb accounts=%d\n", (int)gUsers.size());
    return;
  }
  importUsersTxt("data/users.txt");
  writeUsersPb();
  std::fprintf(stderr, "users.pb created accounts=%d\n", (int)gUsers.size());
}

static void saveUsers() { writeUsersPb(); }

static void sendMsg(SOCKET s, int type, const std::string& body) {
  if (s == INVALID_SOCKET) return;
  std::string f = lb::frame(lb::encodeEnvelope(type, body));
  send(s, f.data(), (int)f.size(), 0);
}

static void sendErr(SOCKET s, const std::string& code, const std::string& msg) {
  sendMsg(s, lb::S2C_Error, lb::encodeError(code, msg));
}

static Client* findSock(SOCKET s) {
  for (auto& c : gCli)
    if (c.s == s) return &c;
  return nullptr;
}

static Room* findRoom(const std::string& id) {
  for (auto& r : gRooms)
    if (r.id == id) return &r;
  return nullptr;
}

static void leaveRoom(Client& c) {
  if (c.room.empty()) return;
  std::string id = c.room;
  Room* r = findRoom(id);
  if (r) {
    if (c.role >= 0 && c.role < 2 && r->seats[c.role] == c.s) r->seats[c.role] = INVALID_SOCKET;
    if (r->n() == 0) {
      gRooms.erase(std::remove_if(gRooms.begin(), gRooms.end(), [&](const Room& x) { return x.id == id; }),
                   gRooms.end());
    } else {
      r->status = 0;
    }
  }
  c.room.clear();
  c.role = -1;
}

static std::vector<lb::RoomInfo> roomInfos() {
  std::vector<lb::RoomInfo> v;
  for (auto& r : gRooms)
    v.push_back({r.id, r.name, r.host.empty() ? r.name : r.host, r.n(), r.status, kMaxRoomPlayers});
  return v;
}

static void sendRooms(SOCKET s) { sendMsg(s, lb::S2C_RoomList, lb::encodeRoomList(roomInfos())); }

static void broadcastRooms() {
  std::string body = lb::encodeRoomList(roomInfos());
  for (auto& c : gCli) {
    if (!c.user.empty()) sendMsg(c.s, lb::S2C_RoomList, body);
  }
  std::fprintf(stderr, "lobby rooms=%d\n", (int)gRooms.size());
}

static void onMsg(Client& c, int type, const std::string& body) {
  const uint8_t* d = (const uint8_t*)body.data();
  size_t n = body.size();
  if (type == lb::C2S_Register) {
    std::string u, p;
    lb::decodeAuth(d, n, u, p);
    if (!validName(u) || p.size() > 32) {
      sendErr(c.s, "BAD", "name");
      return;
    }
    if (gUsers.count(u)) {
      sendErr(c.s, "EXISTS", u);
      return;
    }
    std::string salt = newSalt();
    gUsers[u] = {salt, hashPass(salt, p)};
    saveUsers();
    sendMsg(c.s, lb::S2C_RegisterOk, lb::encodeUser(u));
    return;
  }
  if (type == lb::C2S_Login) {
    std::string u, p;
    lb::decodeAuth(d, n, u, p);
    auto it = gUsers.find(u);
    if (it == gUsers.end() || it->second.hash != hashPass(it->second.salt, p)) {
      sendErr(c.s, "AUTH", "fail");
      return;
    }
    for (auto& o : gCli) {
      if (o.user == u && o.s != c.s) {
        sendErr(c.s, "ONLINE", u);
        return;
      }
    }
    leaveRoom(c);
    c.user = u;
    sendMsg(c.s, lb::S2C_LoginOk, lb::encodeUser(u));
    sendRooms(c.s);
    return;
  }
  if (c.user.empty()) {
    sendErr(c.s, "NEEDLOGIN", "");
    return;
  }
  if (type == lb::C2S_List) {
    sendRooms(c.s);
    return;
  }
  if (type == lb::C2S_Create) {
    leaveRoom(c);
    if ((int)gRooms.size() >= kMaxLobbyRooms) {
      sendErr(c.s, "MAX", "rooms");
      broadcastRooms();
      return;
    }
    Room r;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "r%07u", gNextRoom++);
    r.id = buf;
    r.name = c.user;
    r.host = c.user;
    r.seats[0] = c.s;
    r.status = 0;
    gRooms.push_back(r);
    c.room = r.id;
    c.role = 0;
    sendMsg(c.s, lb::S2C_JoinOk, lb::encodeJoinOk(r.id, 0));
    std::fprintf(stderr, "lobby create %s host=%s rooms=%d\n", r.id.c_str(), c.user.c_str(), (int)gRooms.size());
    broadcastRooms();
    return;
  }
  if (type == lb::C2S_Join) {
    std::string id;
    lb::decodeJoinReq(d, n, id);
    Room* r = findRoom(id);
    if (!r) {
      sendErr(c.s, "NOROOM", id);
      return;
    }
    if (r->status != 0) {
      sendErr(c.s, "FIGHT", id);
      return;
    }
    if (r->n() >= kMaxRoomPlayers) {
      sendErr(c.s, "FULL", id);
      return;
    }
    leaveRoom(c);
    r = findRoom(id);
    if (!r) {
      sendErr(c.s, "NOROOM", id);
      return;
    }
    int role = r->seats[0] == INVALID_SOCKET ? 0 : 1;
    r->seats[role] = c.s;
    c.room = r->id;
    c.role = role;
    sendMsg(c.s, lb::S2C_JoinOk, lb::encodeJoinOk(r->id, role));
    std::fprintf(stderr, "lobby join %s user=%s role=%d n=%d\n", r->id.c_str(), c.user.c_str(), role, r->n());
    if (r->n() == kMaxRoomPlayers) {
      r->status = 1;
      for (int i = 0; i < 2; i++) {
        if (r->seats[i] != INVALID_SOCKET) sendMsg(r->seats[i], lb::S2C_Start, lb::encodeJoinOk(r->id, i));
      }
      std::fprintf(stderr, "lobby start %s\n", r->id.c_str());
    }
    broadcastRooms();
    return;
  }
  if (type == lb::C2S_Leave) {
    leaveRoom(c);
    sendMsg(c.s, lb::S2C_LeaveOk, {});
    broadcastRooms();
    return;
  }
  sendErr(c.s, "CMD", std::to_string(type));
}

int main(int argc, char** argv) {
#ifdef _WIN32
  WSADATA w;
  WSAStartup(MAKEWORD(2, 2), &w);
#endif
  int port = 8080;
  if (argc > 1) port = std::atoi(argv[1]);
  if (argc > 2) gUsersPath = argv[2];
  loadUsers();
  SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  int yes = 1;
  setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons((uint16_t)port);
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(ls, (sockaddr*)&a, sizeof(a)) != 0) {
    std::fprintf(stderr, "lobby bind %d fail\n", port);
    return 1;
  }
  listen(ls, 32);
  std::fprintf(stderr, "ikemen_lobby proto3 tcp %d users=%s max_rooms=%d max_per_room=%d\n", port,
               gUsersPath.c_str(), kMaxLobbyRooms, kMaxRoomPlayers);
  for (;;) {
    fd_set rf;
    FD_ZERO(&rf);
    FD_SET(ls, &rf);
    SOCKET mx = ls;
    for (auto& c : gCli) {
      FD_SET(c.s, &rf);
      if (c.s > mx) mx = c.s;
    }
    timeval tv{1, 0};
    int n = select((int)mx + 1, &rf, nullptr, nullptr, &tv);
    if (n < 0) continue;
    if (FD_ISSET(ls, &rf)) {
      sockaddr_in pa{};
      socklen_t pl = sizeof(pa);
      SOCKET ns = accept(ls, (sockaddr*)&pa, &pl);
      if (ns != INVALID_SOCKET) {
        Client c;
        c.s = ns;
        gCli.push_back(c);
        sendMsg(ns, lb::S2C_Hello, lb::encodeHello("IKLOBBY", 3));
      }
    }
    std::vector<SOCKET> dead;
    for (auto& c : gCli) {
      if (!FD_ISSET(c.s, &rf)) continue;
      char buf[2048];
      int got = recv(c.s, buf, (int)sizeof(buf), 0);
      if (got <= 0) {
        dead.push_back(c.s);
        continue;
      }
      c.rx.append(buf, buf + got);
      for (;;) {
        if (c.rx.size() < 4) break;
        uint32_t sz = (uint8_t)c.rx[0] | ((uint32_t)(uint8_t)c.rx[1] << 8) | ((uint32_t)(uint8_t)c.rx[2] << 16) |
                      ((uint32_t)(uint8_t)c.rx[3] << 24);
        if (sz > 65536) {
          dead.push_back(c.s);
          break;
        }
        if (c.rx.size() < 4 + sz) break;
        int type = 0;
        std::string body;
        lb::decodeEnvelope((const uint8_t*)c.rx.data() + 4, sz, type, body);
        c.rx.erase(0, 4 + sz);
        if (type) {
          std::fprintf(stderr, "lobby cmd user=%s type=%d\n", c.user.c_str(), type);
          onMsg(c, type, body);
        }
      }
    }
    for (SOCKET s : dead) {
      Client* c = findSock(s);
      if (c) {
        std::fprintf(stderr, "lobby drop %s\n", c->user.c_str());
        leaveRoom(*c);
        broadcastRooms();
      }
      closesocket(s);
      gCli.erase(std::remove_if(gCli.begin(), gCli.end(), [s](const Client& x) { return x.s == s; }), gCli.end());
    }
  }
}
