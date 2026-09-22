#pragma once
#include "types.hpp"
#include <string>
#include <unordered_map>

class Renderer {
 public:
  bool Init(int winW, int winH);
  void Resize(int winW, int winH);
  void Begin();
  void Begin(float vw, float vh);
  void BeginHud(float vw, float vh);
  void DrawSprite(const SpriteImage& spr, float x, float y, float facing, float scale);
  void DrawSprite(const SpriteImage& spr, float x, float y, float facing, float sx, float sy,
                  float r, float g, float b, float a);
  void DrawRect(float x, float y, float w, float h, float r, float g, float b, float a);
  void DrawDigits(float x, float y, int value, float scale, float r, float g, float b, float a);
  void DrawWord(float x, float y, const char* word, float scale, float r, float g, float b, float a);
  void End();
  void Shutdown();
  int VpX() const { return vpX_; }
  int VpY() const { return vpY_; }
  int VpW() const { return vpW_; }
  int VpH() const { return vpH_; }
  int VpTop() const { return winH_ - vpY_ - vpH_; }
  int WinW() const { return winW_; }
  int WinH() const { return winH_; }
 private:
  unsigned bindSpriteTex(const SpriteImage& spr);
  void drawQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1);
  unsigned vao_ = 0, vbo_ = 0, prog_ = 0, texFallback_ = 0;
  int uProj_ = -1, uModel_ = -1, uTint_ = -1, uUseTex_ = -1;
  int winW_ = 640, winH_ = 480;
  int vpX_ = 0, vpY_ = 0, vpW_ = 640, vpH_ = 480;
  std::unordered_map<const SpriteImage*, unsigned> texCache_;
};
