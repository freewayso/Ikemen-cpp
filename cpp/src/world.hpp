#pragma once
#include "types.hpp"
#include "fighter.hpp"
#include <vector>

struct Projectile {
  int active = 0;
  int owner = 0;
  int id = 0;
  int anim = 0;
  int hits = 1;
  int removetime = -1;
  int facing = 1;
  int hitpause = 0;
  int priority = 1;
  int priorityPoints = 1;
  Vec2 pos, vel, accel;
  Vec2 velmul{1.f, 1.f};
  HitDef hit;
};

struct DamagePopup {
  Vec2 pos;
  int amount = 0;
  int ttl = 0;
  int ttlMax = 50;
  int guarded = 0;
};

struct FightWorld {
  std::vector<Projectile> projs;
  std::vector<Fighter> helpers;
  std::vector<DamagePopup> popups;
  int nextCharId = 56;
  int helperMax = 56;
  int lastHitDmg[2] = {0, 0};
  int comboHits[2] = {0, 0};
  int comboDmg[2] = {0, 0};
  void Clear() {
    projs.clear();
    helpers.clear();
    popups.clear();
    nextCharId = helperMax;
    lastHitDmg[0] = lastHitDmg[1] = 0;
    comboHits[0] = comboHits[1] = 0;
    comboDmg[0] = comboDmg[1] = 0;
  }
};
