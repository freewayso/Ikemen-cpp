#pragma once
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

struct NetCfg {
  std::string relay = "127.0.0.1";
  int port = 9000;
  std::string room = "kfm1";
  std::string lobby = "127.0.0.1";
  int lobbyPort = 8080;
};

inline std::string iniTrim(std::string s) {
  size_t a = 0;
  while (a < s.size() && std::isspace((unsigned char)s[a])) a++;
  size_t b = s.size();
  while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
  return s.substr(a, b - a);
}

inline std::string iniLower(std::string s) {
  for (char& c : s) c = (char)std::tolower((unsigned char)c);
  return s;
}

inline bool iniLoadFile(const std::string& path, NetCfg& out) {
  std::ifstream in(path);
  if (!in) return false;
  std::string section = "net";
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::string t = iniTrim(line);
    if (t.empty() || t[0] == ';' || t[0] == '#') continue;
    if (t[0] == '[') {
      auto e = t.find(']');
      section = iniLower(iniTrim(t.substr(1, e == std::string::npos ? t.size() - 1 : e - 1)));
      continue;
    }
    if (section != "net" && section != "relay") continue;
    auto eq = t.find('=');
    if (eq == std::string::npos) continue;
    std::string k = iniLower(iniTrim(t.substr(0, eq)));
    std::string v = iniTrim(t.substr(eq + 1));
    if (!v.empty() && (v.front() == '"' || v.front() == '\'')) {
      char q = v.front();
      if (v.size() >= 2 && v.back() == q) v = v.substr(1, v.size() - 2);
    }
    if (k == "relay" || k == "host" || k == "ip" || k == "server") {
      auto c = v.find(':');
      if (c != std::string::npos) {
        out.relay = v.substr(0, c);
        int p = std::atoi(v.c_str() + c + 1);
        if (p > 0) out.port = p;
      } else if (!v.empty()) {
        out.relay = v;
      }
      if (out.lobby == "127.0.0.1" && !out.relay.empty()) out.lobby = out.relay;
    } else if (k == "port") {
      int p = std::atoi(v.c_str());
      if (p > 0) out.port = p;
    } else if (k == "room") {
      if (!v.empty()) out.room = v;
    } else if (k == "lobby") {
      auto c = v.find(':');
      if (c != std::string::npos) {
        out.lobby = v.substr(0, c);
        int p = std::atoi(v.c_str() + c + 1);
        if (p > 0) out.lobbyPort = p;
      } else if (!v.empty()) {
        out.lobby = v;
      }
    } else if (k == "lobbyport") {
      int p = std::atoi(v.c_str());
      if (p > 0) out.lobbyPort = p;
    }
  }
  return true;
}

inline NetCfg LoadNetIni() {
  NetCfg c;
  const char* paths[] = {"data/net.ini", "net.ini", "cpp/net.ini", "../data/net.ini"};
  for (const char* p : paths) {
    if (iniLoadFile(p, c)) {
      std::fprintf(stderr, "net.ini %s relay=%s:%d lobby=%s:%d room=%s\n", p, c.relay.c_str(), c.port,
                   c.lobby.c_str(), c.lobbyPort, c.room.c_str());
      break;
    }
  }
  return c;
}

inline bool SaveNetIni(const NetCfg& c) {
  const char* paths[] = {"data/net.ini", "net.ini"};
  for (const char* p : paths) {
    FILE* f = std::fopen(p, "wb");
    if (!f) continue;
    std::fprintf(f,
                 "; KCP fight relay (UDP) + proto3 lobby (TCP).\n"
                 "[Net]\n"
                 "Relay=%s\n"
                 "Port=%d\n"
                 "Room=%s\n"
                 "Lobby=%s\n"
                 "LobbyPort=%d\n",
                 c.relay.c_str(), c.port, c.room.c_str(), c.lobby.c_str(), c.lobbyPort);
    std::fclose(f);
    std::fprintf(stderr, "saved net.ini %s lobby=%s:%d relay=%s:%d\n", p, c.lobby.c_str(), c.lobbyPort,
                 c.relay.c_str(), c.port);
    return true;
  }
  return false;
}

inline void ParseHostPort(const std::string& s, std::string& host, int& port) {
  std::string t = iniTrim(s);
  if (t.empty()) return;
  auto c = t.rfind(':');
  if (c != std::string::npos && c > 0 && c + 1 < t.size() && std::isdigit((unsigned char)t[c + 1])) {
    host = t.substr(0, c);
    int p = std::atoi(t.c_str() + c + 1);
    if (p > 0 && p < 65536) port = p;
  } else {
    host = t;
  }
}

inline std::string FormatHostPort(const std::string& host, int port) {
  return host + ":" + std::to_string(port);
}
