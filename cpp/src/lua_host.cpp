#include "lua_host.hpp"
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
static int f_ctrl(lua_State* L) {
  auto* f = checkF(L);
  if (lua_gettop(L) >= 2) f->snap.ctrl = lua_toboolean(L, 2) ? 1 : 0;
  lua_pushboolean(L, f->snap.ctrl);
  return 1;
}
static int f_state(lua_State* L) { lua_pushinteger(L, checkF(L)->snap.state); return 1; }
static int f_time(lua_State* L) { lua_pushinteger(L, checkF(L)->snap.time); return 1; }
static int f_anim(lua_State* L) { lua_pushinteger(L, checkF(L)->snap.anim); return 1; }
static int f_animEnded(lua_State* L) { lua_pushboolean(L, checkF(L)->snap.animEnded != 0); return 1; }
static int f_alive(lua_State* L) { lua_pushboolean(L, checkF(L)->snap.alive != 0); return 1; }
static int f_hitpause(lua_State* L) { lua_pushinteger(L, checkF(L)->snap.hitpause); return 1; }
static int f_hitstun(lua_State* L) { lua_pushinteger(L, checkF(L)->snap.hitstun); return 1; }
static int f_hitShakeOver(lua_State* L) { lua_pushboolean(L, checkF(L)->snap.hitpause <= 0); return 1; }
static int f_hitOver(lua_State* L) { lua_pushboolean(L, checkF(L)->snap.hitstun < 0); return 1; }
static int f_moveType(lua_State* L) {
  char b[2] = { checkF(L)->snap.moveType, 0 };
  lua_pushstring(L, b);
  return 1;
}
static int f_facing(lua_State* L) { lua_pushinteger(L, checkF(L)->snap.facing); return 1; }
static int f_teamSide(lua_State* L) { lua_pushinteger(L, checkF(L)->snap.teamSide); return 1; }
static int f_posX(lua_State* L) { lua_pushnumber(L, checkF(L)->snap.pos.x); return 1; }
static int f_posY(lua_State* L) { lua_pushnumber(L, checkF(L)->snap.pos.y); return 1; }
static int f_velX(lua_State* L) {
  auto* f = checkF(L);
  if (lua_gettop(L) >= 2) f->snap.vel.x = (float)luaL_checknumber(L, 2);
  lua_pushnumber(L, f->snap.vel.x); return 1;
}
static int f_velY(lua_State* L) {
  auto* f = checkF(L);
  if (lua_gettop(L) >= 2) f->snap.vel.y = (float)luaL_checknumber(L, 2);
  lua_pushnumber(L, f->snap.vel.y); return 1;
}
static int f_changeState(lua_State* L) {
  int st = (int)luaL_checkinteger(L, 2);
  int ctrl = lua_gettop(L) >= 3 && !lua_isnil(L, 3) ? (int)luaL_checkinteger(L, 3) : -1;
  checkF(L)->ChangeState(st, ctrl);
  return 0;
}
static int f_hitVelSet(lua_State* L) {
  auto* f = checkF(L);
  int x = lua_gettop(L) < 2 || lua_toboolean(L, 2);
  int y = lua_gettop(L) >= 3 && lua_toboolean(L, 3);
  if (x) f->snap.vel.x = f->snap.ghvVelX * (float)f->snap.facing;
  if (y) f->snap.vel.y = f->snap.ghvVelY;
  return 0;
}
static int f_hitDef(lua_State* L) {
  auto* f = checkF(L);
  HitDef h;
  h.on = true;
  h.damage = lua_gettop(L) >= 2 ? (int)luaL_optinteger(L, 2, 40) : 40;
  h.gvx = lua_gettop(L) >= 3 ? (float)luaL_optnumber(L, 3, -4.0) : -4.f;
  h.guardvx = h.gvx;
  h.hittime = 15;
  h.pause1 = 8;
  h.pause2 = 8;
  bool keepOnce = f->snap.hit.on && f->snap.hitOnce;
  f->snap.hit = h;
  f->snap.hitOnce = keepOnce ? 1 : 0;
  return 0;
}
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
  lua_pushboolean(L, checkF(L)->snap.moveType == MoveType::Hit);
  return 1;
}

static const luaL_Reg kFighterMeta[] = {
  {"life", f_life}, {"lifeMax", f_lifeMax}, {"lifeSet", f_lifeSet},
  {"powerSet", f_powerSet}, {"power", f_power}, {"ctrl", f_ctrl},
  {"state", f_state}, {"time", f_time}, {"anim", f_anim}, {"animEnded", f_animEnded},
  {"alive", f_alive}, {"hitpause", f_hitpause}, {"hitstun", f_hitstun},
  {"hitShakeOver", f_hitShakeOver}, {"hitOver", f_hitOver}, {"moveType", f_moveType},
  {"facing", f_facing}, {"teamSide", f_teamSide},
  {"posX", f_posX}, {"posY", f_posY}, {"velX", f_velX}, {"velY", f_velY},
  {"changeState", f_changeState}, {"setAnim", f_setAnim},
  {"hitDef", f_hitDef}, {"hitVelSet", f_hitVelSet},
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
    std::fprintf(stderr, "lua: %s\n", lua_tostring(L_, -1));
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
    std::fprintf(stderr, "TrainingUpdate: %s\n", lua_tostring(L_, -1));
    lua_pop(L_, 1);
  }
}

void LuaHost::CallState(Fighter& f, Fighter* p2) {
  lua_getglobal(L_, "CharUpdate");
  if (!lua_isfunction(L_, -1)) { lua_pop(L_, 1); return; }
  PushFighter(L_, &f);
  if (p2) PushFighter(L_, p2); else lua_pushnil(L_);
  if (lua_pcall(L_, 2, 0, 0) != LUA_OK) {
    std::fprintf(stderr, "CharUpdate: %s\n", lua_tostring(L_, -1));
    lua_pop(L_, 1);
  }
}
