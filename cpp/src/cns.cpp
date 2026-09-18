#include "cns.hpp"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <vector>

static std::string trim(std::string s) {
  while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
  while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
  return s;
}
static std::string lower(std::string s) {
  for (char& c : s) c = (char)tolower((unsigned char)c);
  return s;
}
static bool parseSectionState(const std::string& sec, int& no, bool& isDef) {
  std::string s = lower(sec);
  isDef = false;
  if (s.rfind("[statedef", 0) == 0) {
    isDef = true;
    return sscanf(sec.c_str(), "%*[^0-9-]%d", &no) == 1 || sscanf(sec.c_str(), "[Statedef %d", &no) == 1
        || sscanf(lower(sec).c_str(), "[statedef %d", &no) == 1;
  }
  if (s.rfind("[state", 0) == 0) {
    return sscanf(lower(sec).c_str(), "[state %d", &no) == 1;
  }
  return false;
}

static float parsePairFirst(const std::string& v, float def = 0) {
  float a = def, b = 0;
  if (sscanf(v.c_str(), "%f , %f", &a, &b) >= 1 || sscanf(v.c_str(), "%f,%f", &a, &b) >= 1 || sscanf(v.c_str(), "%f", &a) == 1)
    return a;
  return def;
}
static float parsePairSecond(const std::string& v, float def = 0) {
  float a = 0, b = def;
  if (sscanf(v.c_str(), "%f , %f", &a, &b) == 2 || sscanf(v.c_str(), "%f,%f", &a, &b) == 2) return b;
  return def;
}

bool CnsBank::LoadFile(const std::string& path) {
  std::ifstream f(path);
  if (!f) return false;
  std::string line;
  CnsStateDef* def = nullptr;
  while (std::getline(f, line)) {
    auto c = line.find(';');
    if (c != std::string::npos) line = line.substr(0, c);
    line = trim(line);
    if (line.empty()) continue;
    if (line.front() == '[') {
      int no = 0;
      bool isDef = false;
      if (!parseSectionState(line, no, isDef)) {
        def = nullptr;
        continue;
      }
      if (isDef) {
        def = &defs_[no];
        def->no = no;
      } else {
        def = nullptr; // [State] controllers unused (Lua CharUpdate)
      }
      continue;
    }
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string k = lower(trim(line.substr(0, eq)));
    std::string v = trim(line.substr(eq + 1));
    if (def) {
      if (k == "type" && !v.empty()) def->type = (char)toupper((unsigned char)v[0]);
      else if (k == "movetype" && !v.empty()) def->movetype = (char)toupper((unsigned char)v[0]);
      else if (k == "physics" && !v.empty()) def->physics = (char)toupper((unsigned char)v[0]);
      else if (k == "anim") def->anim = atoi(v.c_str());
      else if (k == "ctrl") def->ctrl = atoi(v.c_str());
      else if (k == "poweradd") def->poweradd = atoi(v.c_str());
      else if (k == "velset") {
        def->velx = parsePairFirst(v, 0);
        def->vely = parsePairSecond(v, 0);
        if (v.find(',') == std::string::npos) def->vely = 1e9f;
      }
    } else {
      if (k == "life") life = atoi(v.c_str());
      else if (k == "attack") attackBase = atoi(v.c_str());
      else if (k == "defence") defenceBase = atoi(v.c_str());
      else if (k == "walk.fwd") walkFwd = (float)atof(v.c_str());
      else if (k == "walk.back") walkBack = (float)atof(v.c_str());
      else if (k == "run.fwd") { runFwd = parsePairFirst(v, 4.6f); }
      else if (k == "run.back") { runBackX = parsePairFirst(v, -4.5f); runBackY = parsePairSecond(v, -3.8f); }
      else if (k == "jump.fwd") jumpFwd = parsePairFirst(v, 2.5f);
      else if (k == "jump.back") jumpBack = parsePairFirst(v, -2.55f);
      else if (k == "yaccel") yaccel = (float)atof(v.c_str());
      else if (k == "stand.friction") standFric = (float)atof(v.c_str());
      else if (k == "crouch.friction") crouchFric = (float)atof(v.c_str());
      else if (k == "ground.front") wFront = atoi(v.c_str());
      else if (k == "ground.back") wBack = atoi(v.c_str());
    }
  }
  return true;
}

const CnsStateDef* CnsBank::Get(int no) const {
  auto it = defs_.find(no);
  return it == defs_.end() ? nullptr : &it->second;
}

void CnsBank::Enter(Fighter& f, int no, int ctrlOverride) {
  f.snap.prevState = f.snap.state;
  f.snap.state = no;
  f.snap.time = 0;
  f.snap.moveContact = 0;
  f.snap.hitOnce = 0;
  f.snap.hit.on = false;
  auto it = defs_.find(no);
  if (it != defs_.end()) {
    const auto& d = it->second;
    f.snap.stateType = d.type ? d.type : +StateType::Stand;
    f.snap.moveType = d.movetype ? d.movetype : +MoveType::Idle;
    f.snap.physics = d.physics ? d.physics : +Physics::Stand;
    if (d.anim >= 0) f.SetAnim(d.anim);
    else f.SetAnim(no);
    if (d.velx < 1e8f) f.snap.vel.x = d.velx;
    if (d.vely < 1e8f) f.snap.vel.y = d.vely;
    if (ctrlOverride >= 0) f.snap.ctrl = ctrlOverride;
    else if (d.ctrl >= 0) f.snap.ctrl = d.ctrl;
    else f.snap.ctrl = 0;
    f.snap.power = std::clamp(f.snap.power + d.poweradd, 0, f.snap.powerMax);
  } else {
    f.snap.ctrl = ctrlOverride >= 0 ? ctrlOverride : 0;
    if (no == State::Stand) { f.snap.stateType = +StateType::Stand; f.snap.moveType = +MoveType::Idle; f.snap.physics = +Physics::Stand; f.snap.ctrl = 1; f.SetAnim(+State::Stand); }
    else if (no == State::StandToCrouch || no == State::Crouch) { f.snap.stateType = +StateType::Crouch; f.snap.moveType = +MoveType::Idle; f.snap.physics = +Physics::Crouch; f.SetAnim(no); f.snap.ctrl = 1; }
    else if (no == State::CrouchToStand) { f.snap.stateType = +StateType::Stand; f.snap.physics = +Physics::Stand; f.SetAnim(+State::CrouchToStand); }
    else if (no == State::Walk) { f.snap.stateType = +StateType::Stand; f.snap.physics = +Physics::Stand; f.snap.ctrl = 1; f.SetAnim(+State::Walk); }
    else if (no == State::Run) {
      f.snap.stateType = +StateType::Stand; f.snap.moveType = +MoveType::Idle; f.snap.physics = +Physics::Stand;
      f.snap.ctrl = 1;
      f.SetAnim(+State::Run);
      f.snap.vel.x = runFwd;
    }
    else if (no == State::RunBack) {
      f.snap.stateType = +StateType::Air; f.snap.moveType = +MoveType::Idle; f.snap.physics = +Physics::Air;
      f.snap.ctrl = 0;
      f.SetAnim(+State::RunBack);
      f.snap.vel.x = runBackX;
      f.snap.vel.y = runBackY;
    }
    else if (no == State::RunBackLand) {
      f.snap.stateType = +StateType::Stand; f.snap.physics = +Physics::Stand; f.snap.ctrl = 0;
      f.SetAnim(+Anim::Land);
      f.snap.vel.y = 0;
    }
    else if (no == State::JumpStart) { f.snap.stateType = +StateType::Stand; f.snap.physics = +Physics::Stand; f.SetAnim(+State::JumpStart); f.snap.ctrl = 0; f.snap.sysVar1 = 0; }
    else if (no == State::Jump) { f.snap.stateType = +StateType::Air; f.snap.physics = +Physics::Air; f.SetAnim(+Anim::JumpAir); }
    else if (no == State::Land) { f.snap.stateType = +StateType::Stand; f.snap.physics = +Physics::Stand; f.SetAnim(+Anim::Land); f.snap.vel = {0, 0}; }
    else if (no == State::GuardStart || no == State::GuardStand || no == State::GuardCrouch) {
      f.snap.stateType = no == State::GuardCrouch ? +StateType::Crouch : +StateType::Stand;
      f.snap.physics = f.snap.stateType;
      f.SetAnim(no);
      f.snap.ctrl = 1;
    }
    else if (no == State::StandingPunch) { f.snap.stateType = +StateType::Stand; f.snap.moveType = +MoveType::Attack; f.snap.physics = +Physics::Stand; f.SetAnim(+State::StandingPunch); f.snap.ctrl = 0; }
    else if (no == State::StandGetHitShake) { f.snap.stateType = +StateType::Stand; f.snap.moveType = +MoveType::Hit; f.snap.physics = +Physics::None; f.SetAnim(+State::StandGetHitShake); f.snap.vel = {0, 0}; f.snap.ctrl = 0; }
    else if (no == State::StandGetHitSlide) { f.snap.stateType = +StateType::Stand; f.snap.moveType = +MoveType::Hit; f.snap.physics = +Physics::Stand; f.SetAnim(+State::StandGetHitSlide); f.snap.ctrl = 0; }
    else if (no == State::CrouchGetHitShake) { f.snap.stateType = +StateType::Crouch; f.snap.moveType = +MoveType::Hit; f.snap.physics = +Physics::None; f.SetAnim(+State::CrouchGetHitShake); f.snap.vel = {0, 0}; f.snap.ctrl = 0; }
    else if (no == State::CrouchGetHitSlide) { f.snap.stateType = +StateType::Crouch; f.snap.moveType = +MoveType::Hit; f.snap.physics = +Physics::Crouch; f.SetAnim(+State::CrouchGetHitSlide); f.snap.ctrl = 0; }
    else if (no == State::AirGetHitShake) { f.snap.stateType = +StateType::Air; f.snap.moveType = +MoveType::Hit; f.snap.physics = +Physics::None; f.SetAnim(+State::AirGetHitShake); f.snap.vel = {0, 0}; f.snap.ctrl = 0; }
    else if (no == State::AirGetHit) { f.snap.stateType = +StateType::Air; f.snap.moveType = +MoveType::Hit; f.snap.physics = +Physics::None; f.SetAnim(+State::AirGetHit); f.snap.ctrl = 0; }
    else if (no == State::HitFall) { f.snap.stateType = +StateType::Air; f.snap.moveType = +MoveType::Hit; f.snap.physics = +Physics::None; f.SetAnim(+State::HitFall); f.snap.ctrl = 0; }
    else if (no == State::HitBounce) { f.snap.stateType = +StateType::Lie; f.snap.moveType = +MoveType::Hit; f.snap.physics = +Physics::None; f.SetAnim(+State::HitBounce); f.snap.ctrl = 0; }
    else if (no == State::LieDown) { f.snap.stateType = +StateType::Lie; f.snap.moveType = +MoveType::Hit; f.snap.physics = +Physics::None; f.SetAnim(+State::LieDown); f.snap.vel = {0, 0}; }
    else if (no == State::LieDownKO) { f.snap.stateType = +StateType::Lie; f.snap.moveType = +MoveType::Hit; f.snap.physics = +Physics::None; f.SetAnim(+State::LieDown); f.snap.vel = {0, 0}; f.snap.ctrl = 0; }
    else if (no == State::GetUp) { f.snap.stateType = +StateType::Lie; f.snap.physics = +Physics::Stand; f.SetAnim(+State::GetUp); }
    else f.SetAnim(no);
  }
  if (no == State::StandingPunch) {
    f.snap.moveType = +MoveType::Attack;
    HitDef h;
    h.on = true;
    h.damage = 40;
    h.gvx = h.guardvx = -5.f;
    h.hittime = 15;
    h.pause1 = 8;
    h.pause2 = 8;
    f.snap.hit = h;
    f.snap.hitOnce = 0;
  }
}

void CnsBank::ApplyPhysics(Fighter& f, float left, float right) {
  // Go posUpdate: skipped during hitpause; friction after integrating vel
  if (f.snap.hitpause > 0) return;
  f.snap.pos.x += f.snap.vel.x * (float)f.snap.facing;
  f.snap.pos.y += f.snap.vel.y;
  char ph = f.snap.physics;
  const float originLs = 1.f; // localscl * (320 / gameWidth)
  if (ph == Physics::Stand) {
    f.snap.vel.x *= standFric;
    if (std::fabs(f.snap.vel.x) < 1.f / originLs) f.snap.vel.x = 0;
    if (f.snap.pos.y > 0) { f.snap.pos.y = 0; f.snap.vel.y = 0; }
  } else if (ph == Physics::Crouch) {
    f.snap.vel.x *= crouchFric;
    if (f.snap.pos.y > 0) { f.snap.pos.y = 0; f.snap.vel.y = 0; }
  } else if (ph == Physics::Air) {
    f.snap.vel.y += yaccel;
    if (f.snap.pos.y > 0) {
      f.snap.pos.y = 0;
      if (f.snap.state != State::Land && f.snap.moveType != MoveType::Hit) Enter(f, State::Land);
      else if (f.snap.moveType == MoveType::Hit && f.snap.state != State::LieDown && f.snap.state != State::LieDownKO)
        Enter(f, f.snap.alive ? State::LieDown : State::LieDownKO);
    }
  } else if (f.snap.state == State::HitFall || f.snap.state == State::AirGetHit) {
    f.snap.vel.y += f.snap.ghvYaccel != 0 ? f.snap.ghvYaccel : yaccel;
    if (f.snap.vel.y > 0 && f.snap.pos.y > 0) {
      f.snap.pos.y = 0;
      Enter(f, f.snap.alive ? State::LieDown : State::LieDownKO);
    }
  }
  f.snap.pos.x = std::clamp(f.snap.pos.x, left, right);
  if (f.snap.hitstun >= 0) f.snap.hitstun--;
}

int CnsBank::ComputeDamage(const Fighter& def, const Fighter& atk, int raw, bool kill, bool bounds) const {
  if (raw == 0) return 0;
  double damage = (double)raw;
  double atkmul = (double)atk.snap.attackMul * ((double)attackBase / 100.0);
  double finalDef = (double)defenceBase / 100.0;
  if (finalDef <= 0) finalDef = 1;
  damage *= atkmul / finalDef;
  if (damage > 0 && damage < 1) damage = 1;
  if (bounds && damage > (double)def.snap.life) damage = (double)def.snap.life;
  if (!kill && damage >= (double)def.snap.life && def.snap.life > 0)
    damage = (double)(def.snap.life - 1);
  return (int)std::lround(damage);
}

void CnsBank::ApplyQueuedDamage(Fighter& f) {
  // Go actionRun: lifeAdd(-ghv.damage, kill=true, absolute=true) if moveType H
  if (f.snap.ghvDamage != 0) {
    int dmg = f.snap.ghvDamage;
    if ((f.snap.asf & ASF_noko) && dmg >= f.snap.life && f.snap.life > 0)
      dmg = f.snap.life - 1;
    f.snap.life = std::max(0, f.snap.life - dmg);
    if (f.snap.life <= 0 && !(f.snap.asf & ASF_noko)) {
      f.snap.alive = 0;
      f.snap.ctrl = 0;
    }
  }
  f.snap.ghvDamage = 0;
}

void CnsBank::ActionFinish(Fighter& f) {
  // Go Char.actionFinish KO block
  if (f.snap.life <= 0 && !(f.snap.asf & ASF_noko)) {
    f.snap.alive = 0;
    f.snap.ctrl = 0;
  }
  if (f.snap.hitpause > 0) return;
  if (!f.snap.alive && f.helperIndex == 0 && f.snap.moveType != MoveType::Hit) {
    f.snap.ghvFall = 1;
    Enter(f, State::AirGetHit, 0);
  }
  if (f.snap.alive && f.snap.life > 0 && world && !world->RoundEnded()) {
    // keep fighting
  }
  if (f.snap.state == State::LieDownKO) f.snap.ctrl = 0;
}

void CnsBank::OnHit(Fighter& atk, Fighter& def, HitResult hitResult) {
  const HitDef& h = atk.snap.hit;
  if (!h.on || atk.snap.hitOnce) return;
  atk.snap.hitOnce = 1;
  atk.snap.moveContact = 1;
  atk.snap.hitpause = h.pause1;
  def.snap.hitpause = h.pause2;
  bool guarded = hitResult == HitResult::Guard;
  int raw = guarded ? h.guardDamage : h.damage;
  bool kill = guarded ? h.guardKill != 0 : h.kill != 0;
  int dmg = ComputeDamage(def, atk, raw, kill, true);
  def.snap.ghvDamage += dmg;
  if (world) {
    DamagePopup pop;
    pop.pos.x = def.snap.pos.x;
    pop.pos.y = def.snap.pos.y - 72.f;
    pop.amount = dmg;
    pop.ttl = pop.ttlMax = 48;
    pop.guarded = guarded ? 1 : 0;
    world->popups.push_back(pop);
    int atkSide = atk.playerIndex == 0 ? 0 : 1;
    world->lastHitDmg[atkSide] = dmg;
    world->comboHits[atkSide]++;
    world->comboDmg[atkSide] += dmg;
  }
  def.snap.hitstun = guarded ? h.guardHittime : h.hittime;
  // Go: ghv.xvel = ground_velocity * -attacker.facing; HitVelSet applies * getter.facing
  float byf = (float)atk.snap.facing;
  if (guarded) {
    def.snap.ghvVelX = h.guardvx * -byf;
    def.snap.ghvVelY = 0;
    def.snap.ghvFall = 0;
  } else if (def.snap.stateType == StateType::Air) {
    def.snap.ghvVelX = h.avx * -byf;
    def.snap.ghvVelY = h.avy;
    def.snap.ghvFall = h.fall ? 1 : 0;
    def.snap.hitstun = h.airHittime;
  } else {
    def.snap.ghvVelX = h.gvx * -byf;
    def.snap.ghvVelY = h.gvy;
    def.snap.ghvFall = h.fall ? 1 : 0;
    if (def.snap.ghvFall && def.snap.ghvVelY == 0) def.snap.ghvVelY = -0.001f;
    if (def.snap.ghvVelY != 0) def.snap.hitstun = h.airHittime;
  }
  def.snap.ghvSlideTime = h.slideTime;
  def.snap.ghvAnimType = h.animType;
  def.snap.ghvGroundType = h.groundType;
  def.snap.ghvAirType = h.airType;
  def.snap.ghvYaccel = h.yaccel;
  def.snap.moveType = +MoveType::Hit;
  def.snap.ctrl = 0;
  def.snap.hittmp = def.snap.ghvFall ? +HitTmp::Fall : +HitTmp::Hit;
  if (guarded) {
    Enter(def, def.snap.stateType == StateType::Crouch ? State::GuardHitCrouch : (def.snap.stateType == StateType::Air ? State::GuardHitAir : State::GuardHitStand));
    return;
  }
  if (h.p2stateno >= 0) {
    Enter(def, h.p2stateno);
    return;
  }
  if (def.snap.stateType == StateType::Crouch) Enter(def, State::CrouchGetHitShake);
  else if (def.snap.stateType == StateType::Air) Enter(def, State::AirGetHitShake);
  else Enter(def, State::StandGetHitShake);
}

void CnsBank::GlobalCollision(Fighter& a, Fighter& b) {
  auto overlap = [](const Fighter& fa, const Rect& ra, const Fighter& fb, const Rect& rb) {
    auto box = [](const Fighter& f, const Rect& r, float& x0, float& y0, float& x1, float& y1) {
      x0 = f.snap.pos.x + r.x0 * f.snap.facing;
      x1 = f.snap.pos.x + r.x1 * f.snap.facing;
      if (x0 > x1) std::swap(x0, x1);
      y0 = f.snap.pos.y + r.y0;
      y1 = f.snap.pos.y + r.y1;
      if (y0 > y1) std::swap(y0, y1);
    };
    float ax0, ay0, ax1, ay1, bx0, by0, bx1, by1;
    box(fa, ra, ax0, ay0, ax1, ay1);
    box(fb, rb, bx0, by0, bx1, by1);
    return ax0 < bx1 && ax1 > bx0 && ay0 < by1 && ay1 > by0;
  };
  auto hittable = [](const Fighter& getter, const HitDef& hd) {
    char t = getter.snap.stateType;
    if ((hd.hitflag & HF_H) == 0 && t == StateType::Stand) return false;
    if ((hd.hitflag & HF_L) == 0 && t == StateType::Crouch) return false;
    if ((hd.hitflag & HF_A) == 0 && t == StateType::Air) return false;
    if ((hd.hitflag & HF_D) == 0 && t == StateType::Lie) return false;
    if ((hd.hitflag & HF_F) == 0 && getter.snap.hittmp >= +HitTmp::Fall) return false;
    return true;
  };
  auto hitResultCheck = [](Fighter& getter, const HitDef& hd) -> HitResult {
    uint32_t in = getter.snap.input | getter.snap.assertInput;
    bool back = (getter.snap.facing >= 0) ? (in & kInputL) != 0 : (in & kInputR) != 0;
    bool canguard = (getter.snap.asf & ASF_autoguard) || (back && (getter.snap.ctrl || getter.snap.state == State::GuardStart ||
                     getter.snap.state == State::GuardStand || getter.snap.state == State::GuardCrouch));
    HitResult result = HitResult::Hit;
    if (canguard) {
      char t = getter.snap.stateType;
      if ((hd.guardflag & HF_H) && t == StateType::Stand) result = HitResult::Guard;
      else if ((hd.guardflag & HF_L) && t == StateType::Crouch) result = HitResult::Guard;
      else if ((hd.guardflag & HF_A) && t == StateType::Air) result = HitResult::Guard;
      else if ((hd.guardflag & HF_M) && (t == StateType::Stand || t == StateType::Crouch)) result = HitResult::Guard;
    }
    return result;
  };
  auto playerHit = [&](Fighter& atk, Fighter& def) {
    if (!atk.snap.hit.on || atk.snap.hitOnce) return;
    if (!hittable(def, atk.snap.hit)) return;
    auto* fa = atk.CurrentFrame();
    auto* fb = def.CurrentFrame();
    std::vector<Rect> atkBoxes, defBoxes;
    if (fa && !fa->clsn1.empty()) atkBoxes = fa->clsn1;
    else atkBoxes.push_back(Rect{-8.f, -80.f, 48.f, -8.f});
    if (fb && !fb->clsn2.empty()) defBoxes = fb->clsn2;
    else defBoxes.push_back(Rect{-(float)wBack, -90.f, (float)wFront, 0.f});
    for (auto c1 : atkBoxes)
      for (auto c2 : defBoxes)
        if (overlap(atk, c1, def, c2)) {
          OnHit(atk, def, hitResultCheck(def, atk.snap.hit));
          return;
        }
  };
  playerHit(a, b);
  playerHit(b, a);
  float d = b.snap.pos.x - a.snap.pos.x;
  float need = (float)(wFront + wBack);
  if (std::fabs(d) < need && std::fabs(a.snap.pos.y - b.snap.pos.y) < 80) {
    float push = (need - std::fabs(d)) * 0.5f;
    if (d >= 0) { a.snap.pos.x -= push; b.snap.pos.x += push; }
    else { a.snap.pos.x += push; b.snap.pos.x -= push; }
  }
}

void CnsBank::TickProjectiles(Fighter& p1, Fighter& p2, float left, float right) {
  if (!world) return;
  auto clsnAt = [](Fighter& owner, int anim, Vec2 pos, int facing, bool clsn1) -> std::vector<Rect> {
    std::vector<Rect> out;
    if (!owner.air) return out;
    const Animation* a = owner.air->Get(anim);
    if (!a || a->frames.empty()) return out;
    const auto& fr = a->frames[0];
    const auto& src = clsn1 ? fr.clsn1 : fr.clsn2;
    for (auto r : src) {
      float x0 = pos.x + r.x0 * facing, x1 = pos.x + r.x1 * facing;
      if (x0 > x1) std::swap(x0, x1);
      float y0 = pos.y + r.y0, y1 = pos.y + r.y1;
      if (y0 > y1) std::swap(y0, y1);
      out.push_back({x0, y0, x1, y1});
    }
    return out;
  };
  auto boxesHit = [](const std::vector<Rect>& a, const std::vector<Rect>& b) {
    for (auto ra : a) for (auto rb : b)
      if (ra.x0 < rb.x1 && ra.x1 > rb.x0 && ra.y0 < rb.y1 && ra.y1 > rb.y0) return true;
    return false;
  };
  auto cancelHits = [](Projectile& p, Projectile& opp) {
    if (p.priorityPoints > opp.priorityPoints) p.priorityPoints--;
    else p.hits--;
    if (p.hits <= 0) p.active = 0;
    else p.hitpause = p.hit.pause1;
  };

  for (size_t i = 0; i < world->projs.size(); i++) {
    auto& p = world->projs[i];
    if (!p.active) continue;
    for (size_t j = i + 1; j < world->projs.size(); j++) {
      auto& q = world->projs[j];
      if (!q.active || p.owner == q.owner) continue;
      Fighter& po = p.owner == 0 ? p1 : p2;
      Fighter& qo = q.owner == 0 ? p1 : p2;
      auto ca = clsnAt(po, p.anim, p.pos, p.facing, false);
      auto cb = clsnAt(qo, q.anim, q.pos, q.facing, false);
      if (ca.empty() || cb.empty()) continue;
      if (boxesHit(ca, cb)) {
        cancelHits(p, q);
        cancelHits(q, p);
      }
    }
  }

  for (auto& p : world->projs) {
    if (!p.active) continue;
    if (p.hitpause > 0) { p.hitpause--; continue; }
    p.pos.x += p.vel.x * (float)p.facing;
    p.pos.y += p.vel.y;
    p.vel.x += p.accel.x;
    p.vel.y += p.accel.y;
    p.vel.x *= p.velmul.x;
    p.vel.y *= p.velmul.y;
    if (p.removetime > 0) p.removetime--;
    if (p.removetime == 0 || p.pos.x < left - 40 || p.pos.x > right + 40 || p.pos.y > 40) {
      p.active = 0;
      continue;
    }
    Fighter& def = (p.owner == 0) ? p2 : p1;
    Fighter& own = (p.owner == 0) ? p1 : p2;
    if (!p.hit.on || p.hits <= 0) continue;
    auto atkBox = clsnAt(own, p.anim, p.pos, p.facing, true);
    if (atkBox.empty()) atkBox = clsnAt(own, p.anim, p.pos, p.facing, false);
    auto* fb = def.CurrentFrame();
    if (!fb || fb->clsn2.empty()) continue;
    std::vector<Rect> defBox;
    for (auto c2 : fb->clsn2) {
      float x0 = def.snap.pos.x + c2.x0 * def.snap.facing;
      float x1 = def.snap.pos.x + c2.x1 * def.snap.facing;
      if (x0 > x1) std::swap(x0, x1);
      float y0 = def.snap.pos.y + c2.y0, y1 = def.snap.pos.y + c2.y1;
      if (y0 > y1) std::swap(y0, y1);
      defBox.push_back({x0, y0, x1, y1});
    }
    if (!boxesHit(atkBox, defBox)) continue;
    Fighter fake;
    fake.snap.hit = p.hit;
    fake.snap.hitOnce = 0;
    fake.snap.attackMul = 1;
    OnHit(fake, def, HitResult::Hit);
    p.hits--;
    p.hitpause = p.hit.pause1;
    if (p.hits <= 0) p.active = 0;
  }
}
