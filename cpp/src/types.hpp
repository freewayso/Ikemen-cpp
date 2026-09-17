#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <memory>
#include <cmath>

constexpr int kGameW = 320;
constexpr int kGameH = 240;
constexpr int kMaxPlayers = 2;
constexpr uint32_t kInputU = 1u << 0;
constexpr uint32_t kInputD = 1u << 1;
constexpr uint32_t kInputL = 1u << 2;
constexpr uint32_t kInputR = 1u << 3;
constexpr uint32_t kInputA = 1u << 4;
constexpr uint32_t kInputB = 1u << 5;
constexpr uint32_t kInputC = 1u << 6;
constexpr uint32_t kInputX = 1u << 7;
constexpr uint32_t kInputY = 1u << 8;
constexpr uint32_t kInputZ = 1u << 9;
constexpr uint32_t kInputS = 1u << 10;
constexpr uint32_t kInputD2 = 1u << 11;
constexpr uint32_t kInputW = 1u << 12;
constexpr uint32_t kInputM = 1u << 13;

struct Vec2 {
  float x = 0, y = 0;
};

struct Rect {
  float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};

struct SpriteKey {
  uint16_t group = 0, number = 0;
  bool operator==(const SpriteKey& o) const { return group == o.group && number == o.number; }
};

struct SpriteKeyHash {
  size_t operator()(const SpriteKey& k) const noexcept {
    return (size_t(k.group) << 16) ^ k.number;
  }
};

struct SpriteImage {
  int w = 0, h = 0;
  int ax = 0, ay = 0;
  uint16_t palidx = 0;
  std::vector<uint8_t> idx;
  std::vector<uint8_t> rgba;
  mutable unsigned glTex = 0;
};

struct AnimFrame {
  uint16_t group = 0, number = 0;
  int x = 0, y = 0, ticks = 1;
  int flipH = 0;
  std::vector<Rect> clsn1, clsn2;
};

struct Animation {
  int action = 0;
  int loopstart = 0;
  std::vector<AnimFrame> frames;
};

constexpr int HF_H = 1 << 0;
constexpr int HF_L = 1 << 1;
constexpr int HF_A = 1 << 2;
constexpr int HF_D = 1 << 3;
constexpr int HF_F = 1 << 4;
constexpr int HF_M = HF_H | HF_L;

struct HitDef {
  bool on = false;
  int damage = 40, guardDamage = 0;
  int hittime = 15, guardHittime = 12;
  int airHittime = 20;
  int slideTime = 0;
  float gvx = -4.f, gvy = 0, avx = -2.f, avy = -3.f;
  float guardvx = -4.f;
  float yaccel = 0.35f;
  int pause1 = 8, pause2 = 8;
  int fall = 0;
  int animType = 0;   // 0 light .. 5 diagup
  int groundType = 1; // 1 high, 2 low, 3 trip
  int airType = 1;
  int kill = 1;
  int guardKill = 1;
  int p2stateno = -1;
  int hitflag = HF_H | HF_L | HF_A | HF_F;
  int guardflag = 0;
};

struct FighterSnapshot {
  int state = 0;
  int time = 0;
  int anim = 0;
  int animElem = 0;
  int animTime = 0;
  int animTimeLeft = 1;
  int animEnded = 0;
  char stateType = 'S';
  char moveType = 'I';
  char physics = 'S';
  int life = 1000;
  int lifeMax = 1000;
  int power = 3000;
  int powerMax = 3000;
  int ctrl = 1;
  int facing = 1;
  int teamSide = 1;
  int hitstun = 0;
  int hitpause = 0;
  int moveContact = 0;
  int hitOnce = 0;
  int alive = 1;
  int roundState = 2;
  Vec2 pos, vel;
  uint32_t input = 0;
  uint32_t assertInput = 0;
  uint32_t asf = 0;
  int ghvDamage = 0;
  float ghvVelX = 0, ghvVelY = 0;
  int ghvFall = 0;
  int ghvSlideTime = 0;
  int ghvAnimType = 0;
  int ghvGroundType = 1;
  int ghvAirType = 1;
  float ghvYaccel = 0.35f;
  int hittmp = 0;
  int prevState = 0;
  int sysVar1 = 0;
  float attackMul = 1.f;
  HitDef hit;
  std::array<float, 64> var{};
  std::unordered_map<std::string, float> map;
};

enum ASF : uint32_t {
  ASF_autoguard = 1u << 0,
  ASF_noko = 1u << 1,
  ASF_noautoturn = 1u << 2,
  ASF_nowalk = 1u << 3,
  ASF_noaicheat = 1u << 4,
};
