#include "input.hpp"
#include "types.hpp"
#include <SDL.h>
#include <cstdio>
#include <cstdlib>

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
  keyEvents_.clear();
  uint32_t down = 0;
  bool escDown = false;
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) quit_ = true;
    if (e.type == SDL_KEYDOWN && !e.key.repeat) {
      down |= scanBits(e.key.keysym.scancode);
      if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) escDown = true;
      const char* nm = SDL_GetScancodeName(e.key.keysym.scancode);
      keyEvents_.push_back(std::string("DOWN ") + (nm && *nm ? nm : "?"));
    }
    if (e.type == SDL_KEYUP) {
      const char* nm = SDL_GetScancodeName(e.key.keysym.scancode);
      keyEvents_.push_back(std::string("UP ") + (nm && *nm ? nm : "?"));
    }
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
      int ww = 1280, hh = 720;
      SDL_Window* w = SDL_GetWindowFromID(e.button.windowID);
      if (w) SDL_GetWindowSize(w, &ww, &hh);
      if (ww < 1) ww = 1280;
      if (hh < 1) hh = 720;
      clickX_ = e.button.x * 1280 / ww;
      clickY_ = e.button.y * 720 / hh;
      clicked_ = true;
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
  if (bit(SDL_SCANCODE_RETURN) || bit(SDL_SCANCODE_KP_ENTER))
    p1 |= kInputS;
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
