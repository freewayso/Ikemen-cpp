#pragma once
#include "fighter.hpp"
#include "world.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

struct CnsParam {
  std::string key, val;
};

struct CnsCtrl {
  std::string type;
  std::vector<CnsParam> params;
  std::vector<std::string> triggerall;
  std::vector<std::vector<std::string>> triggers; // OR of AND-groups (1-based)
  std::vector<CnsCtrl> children;
  int persistent = 1;
  int ignorehitpause = 0;
};

struct CnsStateDef {
  int no = 0;
  char type = 'S', movetype = 'I', physics = 'S';
  int anim = -1, ctrl = -1;
  float velx = 1e9f, vely = 1e9f;
  int poweradd = 0;
  std::vector<CnsCtrl> ctrls;
};

class CnsBank {
 public:
  bool LoadFile(const std::string& path);
  bool LoadZss(const std::string& path);
  const CnsStateDef* Get(int no) const;
  void Enter(Fighter& f, int no, int ctrlOverride = -1);
  void ActionPrepare(Fighter& f); // Go Char.actionPrepare hardcoded keys
  void RunMinusOne(Fighter& f, Fighter& p2);
  void RunCurrent(Fighter& f, Fighter& p2);
  void CommonLoco(Fighter& f);
  void ApplyPhysics(Fighter& f, float left, float right);
  void GlobalCollision(Fighter& a, Fighter& b);
  void TickProjectiles(Fighter& p1, Fighter& p2, float left, float right);
  void OnHit(Fighter& atk, Fighter& def, int hitResult);
  void ApplyQueuedDamage(Fighter& f);
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
  std::unordered_map<std::string, std::vector<CnsCtrl>> funcs_;
  std::unordered_map<std::string, std::vector<std::string>> funcParams_;
  void runCtrlList(const std::vector<CnsCtrl>& ctrls, Fighter& f, Fighter& p2);
  bool evalTriggers(const CnsCtrl& c, Fighter& f, Fighter& p2) const;
  bool evalBool(const std::string& e, Fighter& f, Fighter& p2) const;
  float evalNum(const std::string& e, Fighter& f, Fighter& p2) const;
  void runCtrl(const CnsCtrl& c, int idx, Fighter& f, Fighter& p2);
  void parseHitDef(const CnsCtrl& c, Fighter& f);
};
