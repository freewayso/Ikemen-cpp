#include "fighter.hpp"
#include "cns.hpp"
#include <algorithm>

void Fighter::Reset(int side, float x, float y) {
  snap = FighterSnapshot{};
  snap.teamSide = side;
  snap.facing = side == 1 ? 1 : -1;
  snap.pos = {x, y};
  snap.life = snap.lifeMax = 1000;
  snap.power = snap.powerMax = 3000;
  snap.rage = 0;
  snap.rageMax = 1;
  snap.ctrl = 1;
  snap.alive = 1;
  snap.state = +State::Stand;
  snap.stateType = +StateType::Stand;
  snap.moveType = +MoveType::Idle;
  snap.physics = +Physics::Stand;
  SetAnim(+State::Stand);
}

void Fighter::SetAssets(Sff* s, AirBank* a) { sff = s; air = a; }

void Fighter::ChangeState(int st, int ctrl) {
  if (cns) {
    cns->Enter(*this, st, ctrl);
    return;
  }
  snap.state = st;
  snap.time = 0;
  snap.ctrl = (ctrl >= 0) ? ctrl : (StateGivesCtrl(st) ? 1 : 0);
  SetAnim(st == State::KfmAirSpecial ? +State::AirGetHit : st);
}

void Fighter::SetAnim(int act, int elem) {
  snap.anim = act;
  snap.animElem = std::max(0, elem);
  snap.animTime = 0;
  snap.animEnded = 0;
  const Animation* a = air ? air->Get(snap.anim) : nullptr;
  if (a && snap.animElem >= (int)a->frames.size()) snap.animElem = 0;
}

int Fighter::AnimElemTime(int elem) const {
  // ticks into this 1-based elem; negative if not reached yet
  int e = elem - 1;
  if (snap.animElem < e) return -999;
  if (snap.animElem > e) return 999;
  return snap.animTime;
}

void Fighter::TickAnim() {
  if (snap.hitpause > 0) return;
  const Animation* a = air ? air->Get(snap.anim) : nullptr;
  if (!a || a->frames.empty()) {
    snap.animTimeLeft = 0;
    snap.animEnded = 1;
    return;
  }
  int n = (int)a->frames.size();
  snap.animElem = std::clamp(snap.animElem, 0, n - 1);
  snap.animTime++;
  int remain = a->frames[snap.animElem].ticks - snap.animTime;
  for (int i = snap.animElem + 1; i < n; i++) remain += a->frames[i].ticks;
  snap.animTimeLeft = remain;
  const auto& fr = a->frames[snap.animElem];
  if (snap.animTime >= fr.ticks) {
    if (snap.animElem + 1 >= n) {
      bool looping = a->loopstart > 0 && a->loopstart < n;
      if (looping) {
        snap.animElem = a->loopstart;
        snap.animTime = 0;
        snap.animEnded = 0;
      } else {
        snap.animTime = fr.ticks;
        snap.animEnded = 1;
        snap.animTimeLeft = 0;
      }
    } else {
      snap.animElem++;
      snap.animTime = 0;
    }
  }
}

const AnimFrame* Fighter::CurrentFrame() const {
  const Animation* a = air ? air->Get(snap.anim) : nullptr;
  if (!a || a->frames.empty()) return nullptr;
  return &a->frames[std::clamp(snap.animElem, 0, (int)a->frames.size() - 1)];
}

const SpriteImage* Fighter::CurrentSprite() const {
  auto* fr = CurrentFrame();
  if (!fr || !sff) return nullptr;
  return sff->Get(fr->group, fr->number, pal);
}
