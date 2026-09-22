#pragma once
#include "fighter.hpp"
#include "world.hpp"
#include <string>
#include <unordered_map>

struct CnsStateDef {
  int no = 0;
  char type = +StateType::Stand, movetype = +MoveType::Idle, physics = +Physics::Stand;
  int anim = -1, ctrl = -1;
  float velx = 1e9f, vely = 1e9f;
  int poweradd = 0;
};

class CnsBank {
 public:
  bool LoadFile(const std::string& path);
  const CnsStateDef* Get(int no) const;
  void Enter(Fighter& f, int no, int ctrlOverride = -1);
  void Enter(Fighter& f, State no, int ctrlOverride = -1) { Enter(f, +no, ctrlOverride); }
  void ApplyPhysics(Fighter& f, float left, float right);
  void GlobalCollision(Fighter& a, Fighter& b);
  void TickProjectiles(Fighter& p1, Fighter& p2, float left, float right);
  void SpawnFireCannon(Fighter& f);
  void OnHit(Fighter& atk, Fighter& def, HitResult hitResult);
  void ApplyQueuedDamage(Fighter& f);
  void ActionFinish(Fighter& f);
  int ComputeDamage(const Fighter& def, const Fighter& atk, int raw, bool kill, bool bounds) const;
  FightWorld* world = nullptr;

  float walkFwd = 2.4f, walkBack = -2.2f;
  float jumpNeuX = 0, jumpNeuY = -8.4f, jumpFwd = 2.5f, jumpBack = -2.55f;
  float yaccel = 0.44f, standFric = 0.85f, crouchFric = 0.82f;
  int life = 1000, wFront = 16, wBack = 15;
  int attackBase = 100, defenceBase = 100;
  float runFwd = 4.6f, runBackX = -4.5f, runBackY = -3.8f;

 private:
  std::unordered_map<int, CnsStateDef> defs_;
};
