#pragma once
#include <cstdint>
#include <cstring>

enum {
  kRlMagic0 = 'I', kRlMagic1 = 'K', kRlMagic2 = 'R', kRlMagic3 = 'L',
  kRlRegHost = 1,
  kRlRegGuest = 2,
  kRlData = 3,
  kRlKeep = 4
};

static const int kRlHdr = 13; // 4 magic + 1 cmd + 8 room

inline void rlPadRoom(char out[8], const char* room) {
  std::memset(out, 0, 8);
  if (!room) return;
  for (int i = 0; i < 8 && room[i]; i++) out[i] = room[i];
}

inline bool rlIs(const unsigned char* p, int n) {
  return n >= 5 && p[0] == 'I' && p[1] == 'K' && p[2] == 'R' && p[3] == 'L';
}
