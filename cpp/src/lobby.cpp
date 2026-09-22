#include "lobby.hpp"
#include "lobby_proto.hpp"
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
#include <netinet/tcp.h>
#include <sys/socket.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define closesocket close
#endif

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

bool LobbyClient::Connect(const std::string& ip, int port) {
  Close();
#ifdef _WIN32
  WSADATA w;
  WSAStartup(MAKEWORD(2, 2), &w);
#endif
  SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET) return false;
  sockNb(s);
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons((uint16_t)port);
  if (inet_pton(AF_INET, ip.empty() ? "127.0.0.1" : ip.c_str(), &a.sin_addr) != 1) {
    closesocket(s);
    return false;
  }
  connect(s, (sockaddr*)&a, sizeof(a));
  int yes = 1;
  setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&yes, sizeof(yes));
  sock_ = (uintptr_t)s;
  ready_ = false;
  status_ = "CONNECTING";
  return true;
}

void LobbyClient::pollConnect() {
  if (!sock_ || ready_) return;
  SOCKET s = (SOCKET)sock_;
  fd_set w, e;
  FD_ZERO(&w);
  FD_ZERO(&e);
  FD_SET(s, &w);
  FD_SET(s, &e);
  timeval tv{0, 0};
#ifdef _WIN32
  int n = select(0, nullptr, &w, &e, &tv);
#else
  int n = select((int)s + 1, nullptr, &w, &e, &tv);
#endif
  if (n <= 0) return;
  int err = 0;
  socklen_t el = sizeof(err);
  getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &el);
  if (err != 0) {
    status_ = "CONN FAIL";
    Close();
    return;
  }
  ready_ = true;
  status_ = "HELLO WAIT";
  flushOut();
}

void LobbyClient::flushOut() {
  if (!sock_ || !ready_) return;
  for (auto& f : outq_) send((SOCKET)sock_, f.data(), (int)f.size(), 0);
  outq_.clear();
}

void LobbyClient::sendMsg(int type, const std::string& body) {
  if (!sock_) return;
  std::string f = lb::frame(lb::encodeEnvelope(type, body));
  if (!ready_) {
    outq_.push_back(f);
    return;
  }
  send((SOCKET)sock_, f.data(), (int)f.size(), 0);
}

void LobbyClient::SendLogin(const std::string& user, const std::string& pass) {
  pendingUser_ = user;
  pendingPass_ = pass;
  sendMsg(lb::C2S_Login, lb::encodeAuth(user, pass));
  status_ = "LOGIN";
}
void LobbyClient::SendRegister(const std::string& user, const std::string& pass) {
  pendingUser_ = user;
  pendingPass_ = pass;
  autoLogin_ = true;
  sendMsg(lb::C2S_Register, lb::encodeAuth(user, pass));
  status_ = "REGISTER";
}
void LobbyClient::SendList() { sendMsg(lb::C2S_List, {}); }
void LobbyClient::SendCreate() {
  sendMsg(lb::C2S_Create, {});
  status_ = "CREATE";
}
void LobbyClient::SendJoin(const std::string& id) {
  sendMsg(lb::C2S_Join, lb::encodeJoinReq(id));
  status_ = "JOIN";
}
void LobbyClient::SendLeave() {
  sendMsg(lb::C2S_Leave, {});
  roomId_.clear();
}

void LobbyClient::onMsg(int type, const std::string& body) {
  const uint8_t* d = (const uint8_t*)body.data();
  size_t n = body.size();
  std::fprintf(stderr, "lobby rx type=%d n=%zu\n", type, n);
  if (type == lb::S2C_Hello) {
    status_ = "HELLO";
  } else if (type == lb::S2C_LoginOk) {
    lb::decodeUser(d, n, user_);
    logged_ = true;
    status_ = "HI " + user_;
    SendList();
  } else if (type == lb::S2C_RegisterOk) {
    status_ = "REG OK";
    if (autoLogin_ && !pendingUser_.empty()) {
      autoLogin_ = false;
      sendMsg(lb::C2S_Login, lb::encodeAuth(pendingUser_, pendingPass_));
      status_ = "REG OK LOGIN";
    }
  } else if (type == lb::S2C_Error) {
    std::string code, msg;
    lb::decodeError(d, n, code, msg);
    if (code == "EXISTS" && autoLogin_ && !pendingUser_.empty()) {
      autoLogin_ = false;
      sendMsg(lb::C2S_Login, lb::encodeAuth(pendingUser_, pendingPass_));
      status_ = "EXISTS LOGIN";
    } else {
      autoLogin_ = false;
      status_ = code + (msg.empty() ? "" : " " + msg);
    }
  } else if (type == lb::S2C_RoomList) {
    std::vector<lb::RoomInfo> rs;
    lb::decodeRoomList(d, n, rs);
    rooms_.clear();
    for (auto& x : rs) {
      LobbyRoomInfo r;
      r.id = x.id;
      r.name = x.name;
      r.host = x.host.empty() ? x.name : x.host;
      r.n = x.players;
      r.status = x.status;
      r.maxn = x.max_players > 0 ? x.max_players : 2;
      rooms_.push_back(r);
    }
  } else if (type == lb::S2C_JoinOk) {
    int32_t role = 0;
    lb::decodeJoinOk(d, n, roomId_, role);
    hostRole_ = role == 0;
    status_ = hostRole_ ? "HOST WAIT" : "JOINED WAIT";
  } else if (type == lb::S2C_Start) {
    int32_t role = 0;
    lb::decodeJoinOk(d, n, roomId_, role);
    hostRole_ = role == 0;
    match_ = true;
    status_ = "START";
  } else if (type == lb::S2C_LeaveOk) {
    roomId_.clear();
    status_ = "LEFT";
  }
}

void LobbyClient::Pump() {
  if (!sock_) return;
  pollConnect();
  if (!sock_) return;
  char buf[2048];
  for (;;) {
    int n = recv((SOCKET)sock_, buf, (int)sizeof(buf), 0);
    if (n <= 0) break;
    rx_.append(buf, buf + n);
  }
  for (;;) {
    if (rx_.size() < 4) break;
    uint32_t n = (uint8_t)rx_[0] | ((uint32_t)(uint8_t)rx_[1] << 8) | ((uint32_t)(uint8_t)rx_[2] << 16) |
                 ((uint32_t)(uint8_t)rx_[3] << 24);
    if (n > 65536) {
      Close();
      status_ = "BAD FRAME";
      return;
    }
    if (rx_.size() < 4 + n) break;
    int type = 0;
    std::string body;
    lb::decodeEnvelope((const uint8_t*)rx_.data() + 4, n, type, body);
    rx_.erase(0, 4 + n);
    if (type) onMsg(type, body);
  }
  uint32_t t = nowMs();
  if (logged_ && t - lastListMs_ > 1500) {
    lastListMs_ = t;
    if (roomId_.empty()) SendList();
  }
}

void LobbyClient::Close() {
  if (sock_) closesocket((SOCKET)sock_);
  sock_ = 0;
  ready_ = false;
  logged_ = false;
  match_ = false;
  autoLogin_ = false;
  pendingUser_.clear();
  pendingPass_.clear();
  rx_.clear();
  outq_.clear();
  rooms_.clear();
  roomId_.clear();
  status_ = "OFF";
}
