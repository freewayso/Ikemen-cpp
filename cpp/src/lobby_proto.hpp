#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace lb {

enum Msg : int {
  Unspecified = 0,
  C2S_Register = 1,
  C2S_Login = 2,
  C2S_List = 3,
  C2S_Create = 4,
  C2S_Join = 5,
  C2S_Leave = 6,
  S2C_Hello = 10,
  S2C_Error = 11,
  S2C_LoginOk = 12,
  S2C_RegisterOk = 13,
  S2C_RoomList = 14,
  S2C_JoinOk = 15,
  S2C_Start = 16,
  S2C_LeaveOk = 17,
};

inline void putVarint(std::string& o, uint64_t v) {
  while (v >= 0x80) {
    o.push_back((char)((v & 0x7f) | 0x80));
    v >>= 7;
  }
  o.push_back((char)v);
}

inline void putKey(std::string& o, int field, int wt) { putVarint(o, ((uint64_t)field << 3) | (uint64_t)wt); }

inline void putInt32(std::string& o, int field, int32_t v) {
  if (v == 0) return;
  putKey(o, field, 0);
  putVarint(o, (uint32_t)v);
}

inline void putBytes(std::string& o, int field, const std::string& s) {
  if (s.empty()) return;
  putKey(o, field, 2);
  putVarint(o, s.size());
  o.append(s);
}

inline bool readVarint(const uint8_t*& p, const uint8_t* e, uint64_t& v) {
  v = 0;
  int s = 0;
  while (p < e) {
    uint8_t b = *p++;
    v |= (uint64_t)(b & 0x7f) << s;
    if ((b & 0x80) == 0) return true;
    s += 7;
    if (s > 63) return false;
  }
  return false;
}

inline bool skipField(const uint8_t*& p, const uint8_t* e, int wt) {
  uint64_t v = 0;
  if (wt == 0) return readVarint(p, e, v);
  if (wt == 1) {
    if (e - p < 8) return false;
    p += 8;
    return true;
  }
  if (wt == 5) {
    if (e - p < 4) return false;
    p += 4;
    return true;
  }
  if (wt == 2) {
    if (!readVarint(p, e, v) || (uint64_t)(e - p) < v) return false;
    p += (size_t)v;
    return true;
  }
  return false;
}

struct Cursor {
  const uint8_t* p = nullptr;
  const uint8_t* e = nullptr;
  bool next(int& field, int& wt, const uint8_t*& vp, size_t& vn, uint64_t& vi) {
    if (p >= e) return false;
    uint64_t key = 0;
    if (!readVarint(p, e, key)) return false;
    field = (int)(key >> 3);
    wt = (int)(key & 7);
    vp = p;
    vn = 0;
    vi = 0;
    if (wt == 0) {
      if (!readVarint(p, e, vi)) return false;
      return true;
    }
    if (wt == 2) {
      uint64_t n = 0;
      if (!readVarint(p, e, n) || (uint64_t)(e - p) < n) return false;
      vp = p;
      vn = (size_t)n;
      p += vn;
      return true;
    }
    return skipField(p, e, wt);
  }
};

inline std::string encodeEnvelope(int type, const std::string& body) {
  std::string o;
  putInt32(o, 1, type);
  putBytes(o, 2, body);
  return o;
}

inline bool decodeEnvelope(const uint8_t* d, size_t n, int& type, std::string& body) {
  type = 0;
  body.clear();
  Cursor c{d, d + n};
  int f, wt;
  const uint8_t* vp;
  size_t vn;
  uint64_t vi;
  while (c.next(f, wt, vp, vn, vi)) {
    if (f == 1 && wt == 0) type = (int)vi;
    else if (f == 2 && wt == 2) body.assign((const char*)vp, vn);
  }
  return type != 0;
}

inline std::string encodeAuth(const std::string& user, const std::string& pass) {
  std::string o;
  putBytes(o, 1, user);
  putBytes(o, 2, pass);
  return o;
}

inline bool decodeAuth(const uint8_t* d, size_t n, std::string& user, std::string& pass) {
  user.clear();
  pass.clear();
  Cursor c{d, d + n};
  int f, wt;
  const uint8_t* vp;
  size_t vn;
  uint64_t vi;
  while (c.next(f, wt, vp, vn, vi)) {
    if (f == 1 && wt == 2) user.assign((const char*)vp, vn);
    else if (f == 2 && wt == 2) pass.assign((const char*)vp, vn);
  }
  return !user.empty();
}

inline std::string encodeUser(const std::string& user) {
  std::string o;
  putBytes(o, 1, user);
  return o;
}

inline bool decodeUser(const uint8_t* d, size_t n, std::string& user) {
  user.clear();
  Cursor c{d, d + n};
  int f, wt;
  const uint8_t* vp;
  size_t vn;
  uint64_t vi;
  while (c.next(f, wt, vp, vn, vi)) {
    if (f == 1 && wt == 2) user.assign((const char*)vp, vn);
  }
  return !user.empty();
}

inline std::string encodeError(const std::string& code, const std::string& msg) {
  std::string o;
  putBytes(o, 1, code);
  putBytes(o, 2, msg);
  return o;
}

inline bool decodeError(const uint8_t* d, size_t n, std::string& code, std::string& msg) {
  code.clear();
  msg.clear();
  Cursor c{d, d + n};
  int f, wt;
  const uint8_t* vp;
  size_t vn;
  uint64_t vi;
  while (c.next(f, wt, vp, vn, vi)) {
    if (f == 1 && wt == 2) code.assign((const char*)vp, vn);
    else if (f == 2 && wt == 2) msg.assign((const char*)vp, vn);
  }
  return !code.empty();
}

inline std::string encodeJoinReq(const std::string& id) {
  std::string o;
  putBytes(o, 1, id);
  return o;
}

inline bool decodeJoinReq(const uint8_t* d, size_t n, std::string& id) {
  id.clear();
  Cursor c{d, d + n};
  int f, wt;
  const uint8_t* vp;
  size_t vn;
  uint64_t vi;
  while (c.next(f, wt, vp, vn, vi)) {
    if (f == 1 && wt == 2) id.assign((const char*)vp, vn);
  }
  return !id.empty();
}

struct RoomInfo {
  std::string id, name, host;
  int32_t players = 0;
  int32_t status = 0;
  int32_t max_players = 2;
};

inline std::string encodeRoomInfo(const RoomInfo& r) {
  std::string o;
  putBytes(o, 1, r.id);
  putBytes(o, 2, r.name);
  putInt32(o, 3, r.players);
  putInt32(o, 4, r.status);
  putBytes(o, 5, r.host);
  putInt32(o, 6, r.max_players);
  return o;
}

inline bool decodeRoomInfo(const uint8_t* d, size_t n, RoomInfo& r) {
  r = {};
  r.max_players = 2;
  Cursor c{d, d + n};
  int f, wt;
  const uint8_t* vp;
  size_t vn;
  uint64_t vi;
  while (c.next(f, wt, vp, vn, vi)) {
    if (f == 1 && wt == 2) r.id.assign((const char*)vp, vn);
    else if (f == 2 && wt == 2) r.name.assign((const char*)vp, vn);
    else if (f == 3 && wt == 0) r.players = (int32_t)vi;
    else if (f == 4 && wt == 0) r.status = (int32_t)vi;
    else if (f == 5 && wt == 2) r.host.assign((const char*)vp, vn);
    else if (f == 6 && wt == 0) r.max_players = (int32_t)vi;
  }
  return !r.id.empty();
}

inline std::string encodeRoomList(const std::vector<RoomInfo>& rooms) {
  std::string o;
  for (auto& r : rooms) putBytes(o, 1, encodeRoomInfo(r));
  return o;
}

inline bool decodeRoomList(const uint8_t* d, size_t n, std::vector<RoomInfo>& rooms) {
  rooms.clear();
  Cursor c{d, d + n};
  int f, wt;
  const uint8_t* vp;
  size_t vn;
  uint64_t vi;
  while (c.next(f, wt, vp, vn, vi)) {
    if (f == 1 && wt == 2) {
      RoomInfo r;
      if (decodeRoomInfo(vp, vn, r)) rooms.push_back(r);
    }
  }
  return true;
}

inline std::string encodeJoinOk(const std::string& id, int32_t role) {
  std::string o;
  putBytes(o, 1, id);
  putInt32(o, 2, role);
  return o;
}

inline bool decodeJoinOk(const uint8_t* d, size_t n, std::string& id, int32_t& role) {
  id.clear();
  role = 0;
  Cursor c{d, d + n};
  int f, wt;
  const uint8_t* vp;
  size_t vn;
  uint64_t vi;
  while (c.next(f, wt, vp, vn, vi)) {
    if (f == 1 && wt == 2) id.assign((const char*)vp, vn);
    else if (f == 2 && wt == 0) role = (int32_t)vi;
  }
  return !id.empty();
}

inline std::string encodeHello(const std::string& name, int32_t proto) {
  std::string o;
  putBytes(o, 1, name);
  putInt32(o, 2, proto);
  return o;
}

struct Account {
  std::string user;
  std::string salt;
  std::string pass_hash;
};

inline std::string encodeAccount(const Account& a) {
  std::string o;
  putBytes(o, 1, a.user);
  putBytes(o, 2, a.salt);
  putBytes(o, 3, a.pass_hash);
  return o;
}

inline bool decodeAccount(const uint8_t* d, size_t n, Account& a) {
  a = {};
  Cursor c{d, d + n};
  int f, wt;
  const uint8_t* vp;
  size_t vn;
  uint64_t vi;
  while (c.next(f, wt, vp, vn, vi)) {
    if (f == 1 && wt == 2) a.user.assign((const char*)vp, vn);
    else if (f == 2 && wt == 2) a.salt.assign((const char*)vp, vn);
    else if (f == 3 && wt == 2) a.pass_hash.assign((const char*)vp, vn);
  }
  return !a.user.empty();
}

inline std::string encodeUserStore(uint32_t ver, const std::vector<Account>& users) {
  std::string o;
  putInt32(o, 1, (int32_t)ver);
  for (auto& a : users) putBytes(o, 2, encodeAccount(a));
  return o;
}

inline bool decodeUserStore(const uint8_t* d, size_t n, uint32_t& ver, std::vector<Account>& users) {
  ver = 0;
  users.clear();
  Cursor c{d, d + n};
  int f, wt;
  const uint8_t* vp;
  size_t vn;
  uint64_t vi;
  while (c.next(f, wt, vp, vn, vi)) {
    if (f == 1 && wt == 0) ver = (uint32_t)vi;
    else if (f == 2 && wt == 2) {
      Account a;
      if (decodeAccount(vp, vn, a)) users.push_back(a);
    }
  }
  return true;
}

inline std::string frame(const std::string& payload) {
  uint32_t n = (uint32_t)payload.size();
  std::string o(4 + payload.size(), '\0');
  o[0] = (char)(n & 255);
  o[1] = (char)((n >> 8) & 255);
  o[2] = (char)((n >> 16) & 255);
  o[3] = (char)((n >> 24) & 255);
  std::memcpy(&o[4], payload.data(), payload.size());
  return o;
}

}  // namespace lb
