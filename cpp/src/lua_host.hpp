#pragma once
#include "fighter.hpp"
struct lua_State;

class LuaHost {
 public:
  bool Init();
  void Shutdown();
  bool LoadFile(const std::string& path);
  void CallTraining(Fighter& f, int roundState, const char* gameMode);
  void CallState(Fighter& f, Fighter* p2);
  lua_State* L() { return L_; }
 private:
  lua_State* L_ = nullptr;
};

void RegisterFighterApi(lua_State* L);
void PushFighter(lua_State* L, Fighter* f);
