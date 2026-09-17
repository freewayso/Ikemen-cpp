#pragma once
#include "types.hpp"
#include "fighter.hpp"
#include "sff.hpp"
#include "world.hpp"

class Camera {
 public:
  void Setup(float boundL, float boundR, float tension);
  void Update(const Fighter& a, const Fighter& b);
  float X() const { return x_; }
  float Scale() const { return scale_; }
 private:
  float x_ = 0, scale_ = 1;
  float boundL_ = -150, boundR_ = 150, tension_ = 60;
};

class Hud {
 public:
  bool Load(const std::string& fightSff);
  void Draw(class Renderer& r, const Fighter& a, const Fighter& b, const FightWorld& w);
 private:
  Sff fight_;
};
