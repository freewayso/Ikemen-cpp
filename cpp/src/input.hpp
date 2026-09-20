#pragma once
#include <cstdint>
#include <vector>
#include <string>

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
 private:
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
};
