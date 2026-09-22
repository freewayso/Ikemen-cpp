#include "input.hpp"
#include "types.hpp"
#include <SDL.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static uint32_t scanBits(SDL_Scancode sc) {
  switch (sc) {
    case SDL_SCANCODE_W:
    case SDL_SCANCODE_UP: return kInputU;
    case SDL_SCANCODE_S:
    case SDL_SCANCODE_DOWN: return kInputD;
    case SDL_SCANCODE_A:
    case SDL_SCANCODE_LEFT: return kInputL;
    case SDL_SCANCODE_D:
    case SDL_SCANCODE_RIGHT: return kInputR;
    case SDL_SCANCODE_J:
    case SDL_SCANCODE_U: return kInputA;
    case SDL_SCANCODE_K:
    case SDL_SCANCODE_I: return kInputB;
    case SDL_SCANCODE_Y: return kInputX;
    case SDL_SCANCODE_H: return kInputY;
    case SDL_SCANCODE_N: return kInputZ;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER: return kInputS;
    default: return 0;
  }
}

void InputSys::Poll() {
  clicked_ = false;
  backspace_ = false;
  tab_ = false;
  text_.clear();
  keyEvents_.clear();
  uint32_t down = 0;
  bool escDown = false;
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) quit_ = true;
    if (e.type == SDL_TEXTINPUT && e.text.text[0]) text_ += e.text.text;
    if (e.type == SDL_KEYDOWN) {
      uint32_t bits = scanBits(e.key.keysym.scancode);
      held_ |= bits;
      if (bits & (kInputL | kInputR)) lastLR_ = bits & (kInputL | kInputR);
      if (bits & (kInputU | kInputD)) lastUD_ = bits & (kInputU | kInputD);
      if (!e.key.repeat) {
        down |= scanBits(e.key.keysym.scancode);
        if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) escDown = true;
        if (e.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) backspace_ = true;
        if (e.key.keysym.scancode == SDL_SCANCODE_TAB) tab_ = true;
        const char* nm = SDL_GetScancodeName(e.key.keysym.scancode);
        keyEvents_.push_back(std::string("DOWN ") + (nm && *nm ? nm : "?"));
      }
    }
    if (e.type == SDL_KEYUP) {
      held_ &= ~scanBits(e.key.keysym.scancode);
      const char* nm = SDL_GetScancodeName(e.key.keysym.scancode);
      keyEvents_.push_back(std::string("UP ") + (nm && *nm ? nm : "?"));
    }
    if (e.type == SDL_WINDOWEVENT &&
        (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST || e.window.event == SDL_WINDOWEVENT_HIDDEN)) {
      held_ = 0;
      lastLR_ = lastUD_ = 0;
      if (!pad_) padBits_ = 0;
    }
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
      int ww = scrW_, hh = scrH_;
      SDL_Window* w = SDL_GetWindowFromID(e.button.windowID);
      if (w) SDL_GetWindowSize(w, &ww, &hh);
      if (ww < 1) ww = 1280;
      if (hh < 1) hh = 720;
      mapToLogical(e.button.x, e.button.y, clickX_, clickY_);
      clicked_ = true;
      if (pad_) touchPad(clickX_, clickY_, true, -2, false);
    }
    if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && pad_) {
      int mx = 0, my = 0;
      mapToLogical(e.button.x, e.button.y, mx, my);
      touchPad(mx, my, false, -2, false);
    }
    if (e.type == SDL_MOUSEMOTION && pad_ && (e.motion.state & SDL_BUTTON_LMASK)) {
      int mx = 0, my = 0;
      mapToLogical(e.motion.x, e.motion.y, mx, my);
      touchPad(mx, my, true, -2, true);
    }
    if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERMOTION || e.type == SDL_FINGERUP) {
      int px = (int)(e.tfinger.x * (float)scrW_);
      int py = (int)(e.tfinger.y * (float)scrH_);
      int mx = 0, my = 0;
      mapToLogical(px, py, mx, my);
      if (e.type == SDL_FINGERDOWN) {
        clickX_ = mx;
        clickY_ = my;
        clicked_ = true;
        touchPad(mx, my, true, (long long)e.tfinger.fingerId, false);
      } else if (e.type == SDL_FINGERMOTION) {
        touchPad(mx, my, true, (long long)e.tfinger.fingerId, true);
      } else {
        touchPad(mx, my, false, (long long)e.tfinger.fingerId, false);
      }
    }
  }
  const Uint8* k = SDL_GetKeyboardState(nullptr);
  auto bit = [&](int sc) { return k[sc] ? 1u : 0u; };
  uint32_t p1 = 0;
  if (bit(SDL_SCANCODE_W) || bit(SDL_SCANCODE_UP)) p1 |= kInputU;
  if (bit(SDL_SCANCODE_S) || bit(SDL_SCANCODE_DOWN)) p1 |= kInputD;
  if (bit(SDL_SCANCODE_A) || bit(SDL_SCANCODE_LEFT)) p1 |= kInputL;
  if (bit(SDL_SCANCODE_D) || bit(SDL_SCANCODE_RIGHT)) p1 |= kInputR;
  if (bit(SDL_SCANCODE_U) || bit(SDL_SCANCODE_J)) p1 |= kInputA;
  if (bit(SDL_SCANCODE_I) || bit(SDL_SCANCODE_K)) p1 |= kInputB;
  if (bit(SDL_SCANCODE_O) || bit(SDL_SCANCODE_L)) p1 |= kInputC;
  if (bit(SDL_SCANCODE_Y)) p1 |= kInputX;
  if (bit(SDL_SCANCODE_H)) p1 |= kInputY;
  if (bit(SDL_SCANCODE_N)) p1 |= kInputZ;
  if (bit(SDL_SCANCODE_RETURN) || bit(SDL_SCANCODE_KP_ENTER)) p1 |= kInputS;
  p1 |= held_;
  p1 |= padBits_;
#ifdef _WIN32
  if (win_ && (SDL_GetWindowFlags(win_) & SDL_WINDOW_INPUT_FOCUS)) {
    auto vk = [](int v) { return (GetAsyncKeyState(v) & 0x8000) != 0; };
    if (vk('W') || vk(VK_UP)) p1 |= kInputU;
    if (vk('S') || vk(VK_DOWN)) p1 |= kInputD;
    if (vk('A') || vk(VK_LEFT)) p1 |= kInputL;
    if (vk('D') || vk(VK_RIGHT)) p1 |= kInputR;
    if (vk('J')) p1 |= kInputA;
    if (vk('K')) p1 |= kInputB;
    if (vk('L')) p1 |= kInputC;
  }
#endif
  if ((p1 & kInputL) && (p1 & kInputR)) {
    if (lastLR_ == kInputR) p1 &= ~kInputL;
    else p1 &= ~kInputR;
  }
  if ((p1 & kInputU) && (p1 & kInputD)) {
    if (lastUD_ == kInputD) p1 &= ~kInputU;
    else p1 &= ~kInputD;
  }
  pressed_ = down | (p1 & ~p1_);
  p1_ = p1;
  p2_ = 0;
  if (bit(SDL_SCANCODE_KP_8)) p2_ |= kInputU;
  if (bit(SDL_SCANCODE_KP_5) || bit(SDL_SCANCODE_KP_2)) p2_ |= kInputD;
  if (bit(SDL_SCANCODE_KP_4)) p2_ |= kInputL;
  if (bit(SDL_SCANCODE_KP_6)) p2_ |= kInputR;
  if (bit(SDL_SCANCODE_KP_7)) p2_ |= kInputA;
  if (bit(SDL_SCANCODE_KP_9)) p2_ |= kInputB;
  escPressed_ = escDown || (bit(SDL_SCANCODE_ESCAPE) && !escHeld_);
  escHeld_ = bit(SDL_SCANCODE_ESCAPE) != 0;
}

void InputSys::SetLetterbox(int vx, int vy, int vw, int vh, int sw, int sh) {
  lbX_ = vx;
  lbY_ = vy;
  lbW_ = vw > 0 ? vw : 1280;
  lbH_ = vh > 0 ? vh : 720;
  scrW_ = sw > 0 ? sw : 1280;
  scrH_ = sh > 0 ? sh : 720;
}

void InputSys::SetCombatPad(bool on) {
  combat_ = on;
  if (!on) resetStick();
}

void InputSys::mapToLogical(int px, int py, int& lx, int& ly) const {
  lx = (px - lbX_) * 1280 / lbW_;
  ly = (py - lbY_) * 720 / lbH_;
}

static bool inCircle(int x, int y, float cx, float cy, float r) {
  float dx = (float)x - cx, dy = (float)y - cy;
  return dx * dx + dy * dy <= r * r;
}

void InputSys::resetStick() {
  dpadFinger_ = -1;
  jFinger_ = kFinger_ = cFinger_ = -1;
  padBits_ &= ~(kInputU | kInputD | kInputL | kInputR | kInputA | kInputB | kInputC);
  stickOx_ = VPad::DCx;
  stickOy_ = VPad::DCy;
  stickKx_ = VPad::DCx;
  stickKy_ = VPad::DCy;
}

void InputSys::applyStick(int px, int py) {
  float dx = (float)px - stickOx_;
  float dy = (float)py - stickOy_;
  float len = std::sqrt(dx * dx + dy * dy);
  const float maxr = VPad::DR;
  if (len > maxr && len > 0.001f) {
    dx *= maxr / len;
    dy *= maxr / len;
    len = maxr;
  }
  stickKx_ = stickOx_ + dx;
  stickKy_ = stickOy_ + dy;
  padBits_ &= ~(kInputU | kInputD | kInputL | kInputR);
  const float dead = maxr * 0.28f;
  if (len < dead) return;
  if (dx < -dead) {
    padBits_ |= kInputL;
    lastLR_ = kInputL;
  } else if (dx > dead) {
    padBits_ |= kInputR;
    lastLR_ = kInputR;
  }
  if (dy < -dead) {
    padBits_ |= kInputU;
    lastUD_ = kInputU;
  } else if (dy > dead) {
    padBits_ |= kInputD;
    lastUD_ = kInputD;
  }
}

void InputSys::touchPad(int x, int y, bool down, long long finger, bool motion) {
  if (!pad_ || !combat_) return;

  if (down && !motion) {
    if (inCircle(x, y, VPad::Jx, VPad::Jy, VPad::JR)) {
      jFinger_ = finger;
      padBits_ |= kInputA;
      return;
    }
    if (inCircle(x, y, VPad::Kx, VPad::Ky, VPad::BR)) {
      kFinger_ = finger;
      padBits_ |= kInputB;
      return;
    }
    if (inCircle(x, y, VPad::Cx, VPad::Cy, VPad::CR)) {
      cFinger_ = finger;
      padBits_ |= kInputC;
      return;
    }
    if (x < VPad::ZoneR && y > 160) {
      dpadFinger_ = finger;
      stickOx_ = (float)x;
      stickOy_ = (float)y;
      if (stickOx_ < VPad::DR + 20) stickOx_ = VPad::DR + 20;
      if (stickOx_ > VPad::ZoneR - 20) stickOx_ = VPad::ZoneR - 20;
      if (stickOy_ < VPad::DR + 80) stickOy_ = VPad::DR + 80;
      if (stickOy_ > 700.f - VPad::DR) stickOy_ = 700.f - VPad::DR;
      applyStick(x, y);
      return;
    }
  }
  if (motion) {
    if (finger == dpadFinger_) applyStick(x, y);
    return;
  }
  if (!down) {
    if (finger == dpadFinger_) {
      dpadFinger_ = -1;
      padBits_ &= ~(kInputU | kInputD | kInputL | kInputR);
      stickKx_ = stickOx_ = VPad::DCx;
      stickKy_ = stickOy_ = VPad::DCy;
    }
    if (finger == jFinger_) {
      jFinger_ = -1;
      padBits_ &= ~kInputA;
    }
    if (finger == kFinger_) {
      kFinger_ = -1;
      padBits_ &= ~kInputB;
    }
    if (finger == cFinger_) {
      cFinger_ = -1;
      padBits_ &= ~kInputC;
    }
  }
}

void AiInput::Update(float level) {
  // Port of src/input.go AiInput.Update (button jam)
  auto jam = [&](int* t, int chance, int time) {
    (*t)--;
    if (*t <= 0) {
      if ((rand() % chance) + 1 == 1) {
        *t = 1 + rand() % time;
        return true;
      }
      *t = 0;
    }
    return false;
  };
  if (jam(&dirt, 15, 60)) dir = rand() % 8;
  int chance = (int)((-11.25f * level + 165.f) * 7.f);
  if (chance < 1) chance = 1;
  jam(&at, chance, 30);
  jam(&bt, chance, 30);
  jam(&ct, chance, 30);
  jam(&xt, chance, 30);
  jam(&yt, chance, 30);
  jam(&zt, chance, 30);
  jam(&st, 3600, 30);
}

uint32_t AiInput::Bits() const {
  uint32_t b = 0;
  if (dirt != 0) {
    if (dir == 7 || dir == 0 || dir == 1) b |= kInputU;
    if (dir == 3 || dir == 4 || dir == 5) b |= kInputD;
    if (dir == 5 || dir == 6 || dir == 7) b |= kInputL;
    if (dir == 1 || dir == 2 || dir == 3) b |= kInputR;
  }
  if (at) b |= kInputA;
  if (bt) b |= kInputB;
  if (ct) b |= kInputC;
  if (xt) b |= kInputX;
  if (yt) b |= kInputY;
  if (zt) b |= kInputZ;
  if (st) b |= kInputS;
  return b;
}
