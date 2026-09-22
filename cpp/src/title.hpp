#pragma once
#include "render.hpp"
#include "types.hpp"
#include <string>
#include <vector>

class TitleScreen {
 public:
  enum Action { None, StartFight, Quit };
  enum Kind { Sub, Fight, Back, ExitGame, Ignore, HostNet, JoinNet, Online };
  struct Item {
    const char* label;
    Kind kind;
    int sub = -1;
  };

  bool Load(const std::string& motifDir);
  Action Tick(uint32_t pressed, bool escPressed, float dt, int mx, int my, bool click);
  void Draw(Renderer& r, const char* user = nullptr);
  void DrawWait(Renderer& r, const char* msg, const char* sub = nullptr);
  void DrawLogin(Renderer& r, const std::string& user, const std::string& pass, const std::string& lobby,
                 const std::string& relay, int field, const char* status, bool pad);
  int HitLogin(int mx, int my, bool pad) const;
  void DrawRooms(Renderer& r, const std::string& user, const std::vector<std::string>& rows, int cursor,
                 const char* status);
  int FightMenu() const { return fightMenu_; }

 private:
  void drawText(Renderer& r, float x, float y, const char* s, float cr, float cg, float cb, int align);
  float textWidth(const char* s) const;
  const std::vector<Item>& items() const;

  int menu_ = +TitleMenu::Root;
  int fightMenu_ = +TitleMenu::Arcade;
  int cursor_ = 0;
  int scroll_ = 0;
  int frame_ = 0;
};
