#pragma once
#include "types.hpp"
#include <cstdint>
#include <vector>
#include <string>

struct SDL_Window;

namespace VPad {
constexpr float DCx = 200.f, DCy = 540.f, DR = 150.f, KR = 56.f;
constexpr float Jx = 1048.f, Jy = 560.f, JR = 78.f;
constexpr float Kx = 1188.f, Ky = 430.f, BR = 70.f;
constexpr float Cx = 1188.f, Cy = 590.f, CR = 52.f;
constexpr float ZoneR = 560.f;
}

struct AiInput {
  void Update(float level);
  uint32_t Bits() const;
  int dirt = 0, dir = 0;
  int at = 0, bt = 0, ct = 0, xt = 0, yt = 0, zt = 0, st = 0;
};

class InputSys {
 public:
  void Poll();
  uint32_t P1() const { return p1_; }
  uint32_t P2() const { return p2_; }
  uint32_t Pressed() const { return pressed_; }
  bool Quit() const { return quit_; }
  bool EscPressed() const { return escPressed_; }
  bool Clicked() const { return clicked_; }
  int ClickX() const { return clickX_; }
  int ClickY() const { return clickY_; }
  const std::vector<std::string>& KeyEvents() const { return keyEvents_; }
  const std::string& Text() const { return text_; }
  bool Backspace() const { return backspace_; }
  bool Tab() const { return tab_; }
  void BindWindow(SDL_Window* w) { win_ = w; }
  void SetVirtualPad(bool on) { pad_ = on; }
  bool VirtualPad() const { return pad_; }
  void SetCombatPad(bool on);
  bool CombatPad() const { return combat_; }
  void SetLetterbox(int vx, int vy, int vw, int vh, int sw, int sh);
  float StickOX() const { return stickOx_; }
  float StickOY() const { return stickOy_; }
  float StickKX() const { return stickKx_; }
  float StickKY() const { return stickKy_; }
  bool StickHeld() const { return dpadFinger_ >= 0 || dpadFinger_ == -2; }
  bool BtnJ() const { return (padBits_ & kInputA) != 0; }
  bool BtnK() const { return (padBits_ & kInputB) != 0; }
  bool BtnC() const { return (padBits_ & kInputC) != 0; }
 private:
  void touchPad(int x, int y, bool down, long long finger, bool motion);
  void applyStick(int px, int py);
  void resetStick();
  uint32_t p1_ = 0, p2_ = 0, pressed_ = 0;
  bool quit_ = false;
  bool escHeld_ = false;
  bool escPressed_ = false;
  bool clicked_ = false;
  int clickX_ = 0, clickY_ = 0;
  std::vector<std::string> keyEvents_;
  std::string text_;
  bool backspace_ = false;
  bool tab_ = false;
  SDL_Window* win_ = nullptr;
  uint32_t held_ = 0;
  uint32_t lastLR_ = 0;
  uint32_t lastUD_ = 0;
  bool pad_ = false;
  bool combat_ = false;
  uint32_t padBits_ = 0;
  long long dpadFinger_ = -1;
  long long jFinger_ = -1;
  long long kFinger_ = -1;
  long long cFinger_ = -1;
  float stickOx_ = VPad::DCx;
  float stickOy_ = VPad::DCy;
  float stickKx_ = VPad::DCx;
  float stickKy_ = VPad::DCy;
  int lbX_ = 0, lbY_ = 0, lbW_ = 1280, lbH_ = 720, scrW_ = 1280, scrH_ = 720;
  void mapToLogical(int px, int py, int& lx, int& ly) const;
};
