#include "camera.hpp"
#include "render.hpp"
#include <algorithm>
#include <cmath>

void Camera::Setup(float boundL, float boundR, float tension) {
  boundL_ = boundL;
  boundR_ = boundR;
  tension_ = tension;
  x_ = 0;
  scale_ = 1; // kfm.def zoomout = zoomin = 1
}

void Camera::Update(const Fighter& a, const Fighter& b) {
  float mid = 0.5f * (a.snap.pos.x + b.snap.pos.x);
  float half = kGameW * 0.5f;
  float minx = std::min(a.snap.pos.x, b.snap.pos.x);
  float maxx = std::max(a.snap.pos.x, b.snap.pos.x);
  float left = minx - tension_;
  float right = maxx + tension_;
  float camL = mid - half;
  float camR = mid + half;
  if (left < camL) mid -= (camL - left);
  if (right > camR) mid += (right - camR);
  x_ = std::clamp(mid, boundL_, boundR_);
  scale_ = 1.f;
}

bool Hud::Load(const std::string& fightSff) {
  return fight_.Load(fightSff);
}

void Hud::Draw(Renderer& r, const Fighter& a, const Fighter& b, const FightWorld& w) {
  // fight.def localcoord 1280x720; game space is 320x240 (scale 1/4), matching Go widthScale.
  const float s = 320.f / 1280.f;
  auto bar = [&](float posx, float posy, float facing, float life) {
    const SpriteImage* bg = fight_.Get(10, 0);
    const SpriteImage* front = fight_.Get(13, 0);
    if (!bg || !front) {
      float bw = 80.f * life;
      r.DrawRect(posx, posy, 90.f, 8.f, 0.2f, 0.05f, 0.05f, 0.9f);
      r.DrawRect(posx + 2, posy + 1, bw, 6.f, 0.9f, 0.15f, 0.1f, 1);
      return;
    }
    r.DrawSprite(*bg, posx, posy, facing, s);
    r.DrawSprite(*front, posx, posy, facing, s * life, s, 1, 1, 1, 1);
  };
  float p1x = 595.f * s, p1y = 40.f * s;
  float p2x = 684.f * s, p2y = 40.f * s;
  bar(p1x, p1y, 1.f, a.snap.life / (float)a.snap.lifeMax);
  bar(p2x, p2y, -1.f, b.snap.life / (float)b.snap.lifeMax);
  r.DrawDigits(8.f, 4.f, a.snap.life, 1.6f, 1, 0.85f, 0.2f, 1);
  r.DrawDigits(kGameW - 52.f, 4.f, b.snap.life, 1.6f, 1, 0.85f, 0.2f, 1);
  if (w.comboHits[0] > 0) {
    r.DrawDigits(8.f, 16.f, w.comboHits[0], 1.4f, 1, 1, 1, 1);
    r.DrawDigits(28.f, 16.f, w.comboDmg[0], 1.4f, 1, 0.35f, 0.2f, 1);
  }
  if (w.comboHits[1] > 0) {
    r.DrawDigits(kGameW - 70.f, 16.f, w.comboHits[1], 1.4f, 1, 1, 1, 1);
    r.DrawDigits(kGameW - 50.f, 16.f, w.comboDmg[1], 1.4f, 1, 0.35f, 0.2f, 1);
  }
  r.DrawDigits(140.f, 4.f, w.wins[0], 1.2f, 1, 1, 0.4f, 1);
  r.DrawDigits(172.f, 4.f, w.wins[1], 1.2f, 1, 1, 0.4f, 1);
  if (w.finishType != FinishType::NotYet) {
    if (w.intro > -w.overHitTime) {
      if (w.finishType == FinishType::DKO)
        r.DrawWord(118.f, 88.f, "DKO", 3.2f, 1, 0.85f, 0.2f, 1);
      else
        r.DrawWord(128.f, 88.f, "KO", 3.6f, 1, 0.2f, 0.15f, 1);
    } else {
      if (w.winTeam < 0)
        r.DrawWord(108.f, 88.f, "DRAW", 2.8f, 1, 1, 1, 1);
      else {
        r.DrawWord(96.f, 80.f, "WIN", 3.2f, 1, 0.85f, 0.15f, 1);
        char p[2] = { (char)('1' + w.winTeam), 0 };
        r.DrawWord(118.f, 108.f, p, 3.2f, 1, 0.9f, 0.3f, 1);
      }
    }
  }
}
