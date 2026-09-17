#pragma once
#include "types.hpp"
#include <string>

class Renderer {
 public:
  bool Init(int winW, int winH);
  void Begin();
  void Begin(float vw, float vh);
  void DrawSprite(const SpriteImage& spr, float x, float y, float facing, float scale);
  void DrawSprite(const SpriteImage& spr, float x, float y, float facing, float sx, float sy,
                  float r, float g, float b, float a);
  void DrawRect(float x, float y, float w, float h, float r, float g, float b, float a);
  void DrawDigits(float x, float y, int value, float scale, float r, float g, float b, float a);
  void End();
  void Shutdown();
 private:
  unsigned bindSpriteTex(const SpriteImage& spr);
  void drawQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1);
  unsigned vao_ = 0, vbo_ = 0, prog_ = 0, texFallback_ = 0;
  int uProj_ = -1, uModel_ = -1, uTint_ = -1, uUseTex_ = -1;
  int winW_ = 640, winH_ = 480;
};
