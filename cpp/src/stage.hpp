#pragma once
#include "sff.hpp"
#include "render.hpp"
#include <string>
#include <vector>

struct StageBG {
  int group = 0, number = 0;
  int layer = 0;
  float startx = 0, starty = 0;
  float deltax = 1, deltay = 1;
  int tilex = 0, tiley = 0;
  float spacex = 0, spacey = 0;
  float alpha = 1;
};

struct Stage {
  bool Load(const std::string& defPath);
  void Draw(Renderer& r, float camx, float camy, int layer) const;

  float p1startx = -70, p1starty = 0;
  float p2startx = 70, p2starty = 0;
  int p1facing = 1, p2facing = -1;
  float boundleft = -150, boundright = 150;
  float tension = 60;
  float zoffset = 200;
  float shadowYScale = -0.1f;
  float shadowAlpha = 64.f / 256.f;
  float leftbound = -1000, rightbound = 1000;
  Sff sff;
  std::vector<StageBG> bgs;
};
