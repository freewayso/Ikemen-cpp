#pragma once
#include "types.hpp"
#include "air.hpp"
#include "sff.hpp"
#include "cmd.hpp"
#include <string>

class CnsBank;

class Fighter {
 public:
  void Reset(int side, float x, float y);
  void SetAssets(Sff* sff, AirBank* air);
  void ChangeState(int st, int ctrl = -1);
  void SetAnim(int act, int elem = 0);
  void TickAnim();
  const AnimFrame* CurrentFrame() const;
  const SpriteImage* CurrentSprite() const;
  int AnimElemTime(int elem) const;
  FighterSnapshot snap;
  CmdMatcher cmd;
  Sff* sff = nullptr;
  AirBank* air = nullptr;
  CnsBank* cns = nullptr;
  int playerIndex = 0;
  int helperIndex = 0;
  int id = -1;
  int parentId = -1;
  int helperId = 0;
  int pal = 0;
};
