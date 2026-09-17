#include "lua_host.hpp"
#include "log.hpp"
#include "lua.hpp"
#include <cstring>

#include <cstdio>

static Fighter* checkF(lua_State* L, int idx = 1) {
  return *(Fighter**)luaL_checkudata(L, idx, "IkFighter");
}

static int f_life(lua_State* L) {
  auto* f = checkF(L);
  if (lua_gettop(L) >= 2) f->snap.life = (int)luaL_checkinteger(L, 2);
  lua_pushinteger(L, f->snap.life);
  return 1;
}
static int f_lifeMax(lua_State* L) { lua_pushinteger(L, checkF(L)->snap.lifeMax); return 1; }
static int f_lifeSet(lua_State* L) { checkF(L)->snap.life = (int)luaL_checkinteger(L, 2); return 0; }
static int f_powerSet(lua_State* L) { checkF(L)->snap.power = (int)luaL_checkinteger(L, 2); return 0; }
static int f_power(lua_State* L) { lua_pushinteger(L, checkF(L)->snap.power); return 1; }
static int f_ctrl(lua_State* L) { lua_pushboolean(L, checkF(L)->snap.ctrl); return 1; }
static int f_state(lua_State* L) { lua_pushinteger(L, checkF(L)->snap.state); return 1; }
static int f_facing(lua_State* L) { lua_pushinteger(L, checkF(L)->snap.facing); return 1; }
static int f_teamSide(lua_State* L) { lua_pushinteger(L, checkF(L)->snap.teamSide); return 1; }
static int f_posX(lua_State* L) { lua_pushnumber(L, checkF(L)->snap.pos.x); return 1; }
static int f_posY(lua_State* L) { lua_pushnumber(L, checkF(L)->snap.pos.y); return 1; }
static int f_velX(lua_State* L) {
  auto* f = checkF(L);
  if (lua_gettop(L) >= 2) f->snap.vel.x = (float)luaL_checknumber(L, 2);
  lua_pushnumber(L, f->snap.vel.x); return 1;
}
static int f_changeState(lua_State* L) { checkF(L)->ChangeState((int)luaL_checkinteger(L, 2)); return 0; }
static int f_setAnim(lua_State* L) { checkF(L)->SetAnim((int)luaL_checkinteger(L, 2)); return 0; }
static int f_command(lua_State* L) {
  lua_pushboolean(L, checkF(L)->cmd.Was(luaL_checkstring(L, 2)));
  return 1;
}
static int f_input(lua_State* L) {
  auto* f = checkF(L);
  const char* n = luaL_checkstring(L, 2);
  uint32_t b = f->snap.input | f->snap.assertInput;
  uint32_t m = 0;
  if (!std::strcmp(n, "U")) m = kInputU;
  else if (!std::strcmp(n, "D")) m = kInputD;
  else if (!std::strcmp(n, "L")) m = kInputL;
  else if (!std::strcmp(n, "R")) m = kInputR;
  else if (!std::strcmp(n, "F")) m = f->snap.facing >= 0 ? kInputR : kInputL;
  else if (!std::strcmp(n, "B")) m = f->snap.facing >= 0 ? kInputL : kInputR;
  else if (!std::strcmp(n, "a")) m = kInputA;
  else if (!std::strcmp(n, "b")) m = kInputB;
  else if (!std::strcmp(n, "c")) m = kInputC;
  lua_pushboolean(L, (b & m) != 0);
  return 1;
}
static int f_assertInput(lua_State* L) {
  auto* f = checkF(L);
  const char* n = luaL_checkstring(L, 2);
  if (!std::strcmp(n, "F")) f->snap.assertInput |= f->snap.facing >= 0 ? kInputR : kInputL;
  else if (!std::strcmp(n, "B")) f->snap.assertInput |= f->snap.facing >= 0 ? kInputL : kInputR;
  else if (!std::strcmp(n, "D")) f->snap.assertInput |= kInputD;
  else if (!std::strcmp(n, "U")) f->snap.assertInput |= kInputU;
  else if (!std::strcmp(n, "a")) f->snap.assertInput |= kInputA;
  return 0;
}
static int f_assertSpecial(lua_State* L) {
  auto* f = checkF(L);
  const char* n = luaL_checkstring(L, 2);
  if (!std::strcmp(n, "autoGuard")) f->snap.asf |= ASF_autoguard;
  if (!std::strcmp(n, "noKo")) f->snap.asf |= ASF_noko;
  return 0;
}
static int f_map(lua_State* L) {
  auto* f = checkF(L);
  const char* k = luaL_checkstring(L, 2);
  if (lua_gettop(L) >= 3) f->snap.map[k] = (float)luaL_checknumber(L, 3);
  lua_pushnumber(L, f->snap.map[k]);
  return 1;
}
static int f_moveTypeH(lua_State* L) {
  lua_pushboolean(L, checkF(L)->snap.moveType == 'H');
  return 1;
}

static const luaL_Reg kFighterMeta[] = {
  {"life", f_life}, {"lifeMax", f_lifeMax}, {"lifeSet", f_lifeSet},
  {"powerSet", f_powerSet}, {"power", f_power}, {"ctrl", f_ctrl},
  {"state", f_state}, {"facing", f_facing}, {"teamSide", f_teamSide},
  {"posX", f_posX}, {"posY", f_posY}, {"velX", f_velX},
  {"changeState", f_changeState}, {"setAnim", f_setAnim},
  {"command", f_command}, {"input", f_input},
  {"assertInput", f_assertInput}, {"assertSpecial", f_assertSpecial},
  {"map", f_map}, {"moveTypeH", f_moveTypeH},
  {nullptr, nullptr}
};

void RegisterFighterApi(lua_State* L) {
  luaL_newmetatable(L, "IkFighter");
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, kFighterMeta, 0);
  lua_pop(L, 1);
}

void PushFighter(lua_State* L, Fighter* f) {
  auto** ud = (Fighter**)lua_newuserdata(L, sizeof(Fighter*));
  *ud = f;
  luaL_setmetatable(L, "IkFighter");
}

bool LuaHost::Init() {
  L_ = luaL_newstate();
  luaL_openlibs(L_);
  RegisterFighterApi(L_);
  return true;
}

void LuaHost::Shutdown() {
  if (L_) lua_close(L_);
  L_ = nullptr;
}

bool LuaHost::LoadFile(const std::string& path) {
  if (luaL_dofile(L_, path.c_str()) != LUA_OK) {
    GameLog::Get().Warn("lua: %s", lua_tostring(L_, -1));
    lua_pop(L_, 1);
    return false;
  }
  return true;
}

void LuaHost::CallTraining(Fighter& f, int roundState, const char* gameMode) {
  lua_getglobal(L_, "TrainingUpdate");
  if (!lua_isfunction(L_, -1)) { lua_pop(L_, 1); return; }
  PushFighter(L_, &f);
  lua_pushinteger(L_, roundState);
  lua_pushstring(L_, gameMode);
  if (lua_pcall(L_, 3, 0, 0) != LUA_OK) {
    GameLog::Get().Warn("TrainingUpdate: %s", lua_tostring(L_, -1));
    lua_pop(L_, 1);
  }
}

void LuaHost::CallState(Fighter& f, Fighter* p2) {
  lua_getglobal(L_, "CharUpdate");
  if (!lua_isfunction(L_, -1)) { lua_pop(L_, 1); return; }
  PushFighter(L_, &f);
  if (p2) PushFighter(L_, p2); else lua_pushnil(L_);
  if (lua_pcall(L_, 2, 0, 0) != LUA_OK) {
    GameLog::Get().Warn("CharUpdate: %s", lua_tostring(L_, -1));
    lua_pop(L_, 1);
  }
}
