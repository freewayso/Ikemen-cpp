#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>

enum {
  kRlMagic0 = 'I', kRlMagic1 = 'K', kRlMagic2 = 'R', kRlMagic3 = 'L',
  kRlRegHost = 1,
  kRlRegGuest = 2,
  kRlData = 3,
  kRlKeep = 4
};

static const int kRlHdr = 13; // 4 magic + 1 cmd + 8 room

enum {
  kFsJoin = 10,
  kFsJoinOk = 11,
  kFsInput = 12,
  kFsConfirm = 13,
  kFsCatchup = 14,
  kFsCatchupPack = 15,
  kFsDrop = 16
};

static const int kFsHdr = 32;
static const char kFsMagic[4] = {'I', 'K', 'F', 'S'};

struct FsMsg {
  uint8_t cmd = 0;
  uint8_t role = 0;
  char room[8]{};
  int32_t frame = 0;
  uint32_t a = 0;
  uint32_t b = 0;
  uint32_t seed = 0;
  uint16_t count = 0;
};

inline void rlPadRoom(char out[8], const char* room) {
  std::memset(out, 0, 8);
  if (!room) return;
  for (int i = 0; i < 8 && room[i]; i++) out[i] = room[i];
}

inline bool rlIs(const unsigned char* p, int n) {
  return n >= 5 && p[0] == 'I' && p[1] == 'K' && p[2] == 'R' && p[3] == 'L';
}

inline bool fsIs(const unsigned char* p, int n) {
  return n >= kFsHdr && p[0] == 'I' && p[1] == 'K' && p[2] == 'F' && p[3] == 'S';
}

inline void fsWrite(unsigned char out[kFsHdr], const FsMsg& m) {
  std::memcpy(out, kFsMagic, 4);
  out[4] = m.cmd;
  out[5] = m.role;
  std::memcpy(out + 6, m.room, 8);
  auto wu32 = [](unsigned char* d, uint32_t v) {
    d[0] = (unsigned char)v;
    d[1] = (unsigned char)(v >> 8);
    d[2] = (unsigned char)(v >> 16);
    d[3] = (unsigned char)(v >> 24);
  };
  wu32(out + 14, (uint32_t)m.frame);
  wu32(out + 18, m.a);
  wu32(out + 22, m.b);
  wu32(out + 26, m.seed);
  out[30] = (unsigned char)m.count;
  out[31] = (unsigned char)(m.count >> 8);
}

inline const char* fsCmdName(int cmd) {
  switch (cmd) {
    case kFsJoin: return "JOIN";
    case kFsJoinOk: return "JOIN_OK";
    case kFsInput: return "INPUT";
    case kFsConfirm: return "CONFIRM";
    case kFsCatchup: return "CATCHUP";
    case kFsCatchupPack: return "CATCHUP_PACK";
    case kFsDrop: return "DROP";
    default: return "UNK";
  }
}

inline void fsPrintSpec(const char* who) {
  std::fprintf(stderr,
               "[IKFS] %s  UDP+KCP  magic=IKFS  hdr=32 little-endian\n"
               "  +0 4 magic IKFS\n"
               "  +4 1 cmd JOIN=10 JOIN_OK=11 INPUT=12 CONFIRM=13 CATCHUP=14 CATCHUP_PACK=15 DROP=16\n"
               "  +5 1 role 0=host 1=guest\n"
               "  +6 8 room\n"
               "  +14 4 frame\n"
               "  +18 4 a   INPUT=localBits  CONFIRM=p0  JOIN_OK unused\n"
               "  +22 4 b   CONFIRM=p1\n"
               "  +26 4 seed\n"
               "  +30 2 count  CATCHUP_PACK extra n*(frame,i0,i1) u32le\n"
               "  KCP conv host=0x4B465301 guest=0x4B465302\n",
               who ? who : "");
  std::fflush(stderr);
}

inline bool fsShouldLog(const FsMsg& m) {
  if (m.cmd != kFsInput && m.cmd != kFsConfirm) return true;
  if (m.frame < 8) return true;
  return (m.frame % 30) == 0;
}

inline void fsDump(const char* who, const char* dir, const FsMsg& m, int nbytes = kFsHdr) {
  if (!fsShouldLog(m)) return;
  unsigned char raw[kFsHdr];
  fsWrite(raw, m);
  char hex[97];
  int n = 0;
  for (int i = 0; i < kFsHdr && n < 96; i++) n += std::sprintf(hex + n, "%02x", raw[i]);
  hex[n] = 0;
  std::fprintf(stderr,
               "[IKFS] %s %s %-12s role=%u room=%.8s frame=%d a=%u b=%u seed=%u count=%u n=%d hex=%s\n",
               who, dir, fsCmdName(m.cmd), (unsigned)m.role, m.room, (int)m.frame, m.a, m.b, m.seed,
               (unsigned)m.count, nbytes, hex);
  std::fflush(stderr);
}

inline FsMsg fsRead(const unsigned char* p) {
  FsMsg m;
  m.cmd = p[4];
  m.role = p[5];
  std::memcpy(m.room, p + 6, 8);
  auto ru32 = [](const unsigned char* d) -> uint32_t {
    return (uint32_t)d[0] | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16) | ((uint32_t)d[3] << 24);
  };
  m.frame = (int32_t)ru32(p + 14);
  m.a = ru32(p + 18);
  m.b = ru32(p + 22);
  m.seed = ru32(p + 26);
  m.count = (uint16_t)p[30] | ((uint16_t)p[31] << 8);
  return m;
}
