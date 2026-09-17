#include "engine.hpp"
#include "log.hpp"
#include "stage.hpp"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_syswm.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <imm.h>
#endif
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <fstream>
#include <array>

static std::string bitsStr(uint32_t b) {
  std::string s;
  if (b & kInputU) s += "U";
  if (b & kInputD) s += "D";
  if (b & kInputL) s += "L";
  if (b & kInputR) s += "R";
  if (b & kInputA) s += "a";
  if (b & kInputB) s += "b";
  if (b & kInputC) s += "c";
  if (b & kInputX) s += "x";
  if (b & kInputY) s += "y";
  if (b & kInputZ) s += "z";
  if (b & kInputS) s += "s";
  return s.empty() ? std::string("-") : s;
}

static SDL_Window* gWin = nullptr;
static SDL_GLContext gCtx = nullptr;

static bool dirExists(const std::string& p) {
#ifdef _WIN32
  DWORD a = GetFileAttributesA(p.c_str());
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
  return true;
#endif
}

static std::string repoRoot() {
#ifdef _WIN32
  char buf[MAX_PATH] = {};
  if (GetModuleFileNameA(nullptr, buf, MAX_PATH)) {
    std::string exe = buf;
    auto slash = exe.find_last_of("\\/");
    std::string dir = slash == std::string::npos ? std::string(".") : exe.substr(0, slash);
    std::string parent = dir;
    slash = parent.find_last_of("\\/");
    if (slash != std::string::npos) parent = parent.substr(0, slash);
    if (dirExists(parent + "\\chars") && dirExists(parent + "\\stages")) return parent;
    if (dirExists(dir + "\\chars") && dirExists(dir + "\\stages")) return dir;
  }
#endif
  if (dirExists("chars") && dirExists("stages")) return ".";
  if (dirExists("../chars") && dirExists("../stages")) return "..";
  return ".";
}

std::string Engine::contentFingerprint() const {
  // Go System.currentContentFingerprint currently returns "".
  return "";
}

bool Engine::Init(int argc, char** argv) {
  std::string relayHost;
  int relayPort = 9000;
  std::string room = "kfm1";
  bool wantLog = false;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--host")) mode_ = "host";
    else if (!std::strcmp(argv[i], "--connect") && i + 1 < argc) {
      mode_ = "connect";
      connectIp_ = argv[++i];
    } else if (!std::strcmp(argv[i], "--rbhost")) {
      mode_ = "rollback-host";
      if (i + 1 < argc && argv[i + 1][0] != '-') connectIp_ = argv[++i];
    } else if (!std::strcmp(argv[i], "--rollback") && i + 1 < argc) {
      mode_ = "rollback";
      connectIp_ = argv[++i];
    } else if (!std::strcmp(argv[i], "--relay") && i + 1 < argc) {
      std::string r = argv[++i];
      auto c = r.find(':');
      if (c != std::string::npos) {
        relayHost = r.substr(0, c);
        relayPort = std::atoi(r.c_str() + c + 1);
      } else relayHost = r;
    } else if (!std::strcmp(argv[i], "--room") && i + 1 < argc) {
      room = argv[++i];
    } else if (!std::strcmp(argv[i], "--autohit")) {
      autoHit_ = true;
    } else if (!std::strcmp(argv[i], "--log")) {
      wantLog = true;
    }
  }
  if (!relayHost.empty()) delayNet_.SetRelay(relayHost, relayPort, room);
  std::string root = repoRoot();
#ifdef _WIN32
  SetCurrentDirectoryA(root.c_str());
#endif
  std::srand((unsigned)std::time(nullptr));
  GameLog::Get().Open(root, mode_, wantLog);

  SDL_SetHint(SDL_HINT_IME_SHOW_UI, "0");
  SDL_SetMainReady();
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
    GameLog::Get().Error("SDL: %s", SDL_GetError());
#ifdef _WIN32
    MessageBoxA(nullptr, SDL_GetError(), "Ikemen SDL init failed", MB_OK);
#endif
    return false;
  }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  int wx = 40, wy = 40;
  const char* wtitle = "Ikemen GO";
  if (mode_ == "host" || mode_ == "rollback-host") {
    wx = 20;
    wy = 40;
    wtitle = "Ikemen GO [HOST]";
  } else if (mode_ == "connect" || mode_ == "rollback") {
    wx = 660;
    wy = 80;
    wtitle = "Ikemen GO [CLIENT]";
  }
  gWin = SDL_CreateWindow(wtitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  if (!gWin) {
#ifdef _WIN32
    MessageBoxA(nullptr, SDL_GetError(), "Ikemen window failed", MB_OK);
#endif
    return false;
  }
  gCtx = SDL_GL_CreateContext(gWin);
  if (!gCtx) {
#ifdef _WIN32
    MessageBoxA(nullptr, SDL_GetError(), "Ikemen OpenGL failed", MB_OK);
#endif
    return false;
  }
  SDL_GL_SetSwapInterval(0);
  if (!render_.Init(1280, 720)) {
#ifdef _WIN32
    MessageBoxA(nullptr, "OpenGL 3.3 shaders/buffers failed", "Ikemen render failed", MB_OK);
#endif
    return false;
  }
  SDL_StopTextInput();
  SDL_SetWindowPosition(gWin, wx, wy);
  SDL_RaiseWindow(gWin);
#ifdef _WIN32
  SDL_SysWMinfo wminfo;
  SDL_VERSION(&wminfo.version);
  if (SDL_GetWindowWMInfo(gWin, &wminfo))
    ImmAssociateContext(wminfo.info.win.window, nullptr);
#endif

  std::string sff = root + "/chars/kfm/kfm.sff";
  std::string air = root + "/chars/kfm/kfm.air";
  std::string cmd = root + "/chars/kfm/kfm.cmd";
  if (!sff_.Load(sff)) GameLog::Get().Warn("sff load failed %s", sff.c_str());
  if (!air_.Load(air)) GameLog::Get().Warn("air load failed %s", air.c_str());
  if (!stage_.Load(root + "/stages/kfm.def"))
    GameLog::Get().Warn("stage load failed");
  cam_.Setup(stage_.boundleft, stage_.boundright, stage_.tension);
  if (!hud_.Load(root + "/data/fight.sff"))
    GameLog::Get().Warn("fight.sff load failed");
  title_.Load(root + "/data/ikemen1");

  p1_.playerIndex = 0;
  p2_.playerIndex = 1;
  p1_.pal = 0;
  p2_.pal = 1;
  p1_.SetAssets(&sff_, &air_);
  p2_.SetAssets(&sff_, &air_);
  p1_.cmd.LoadFile(cmd);
  p2_.cmd.LoadFile(cmd);
  cns_.LoadFile(root + "/chars/kfm/kfm.cns");
  cns_.LoadFile(root + "/chars/kfm/kfm.cmd");
  cns_.LoadZss(root + "/data/common1.cns.zss");
  cns_.world = &world_;
  p1_.cns = p2_.cns = &cns_;
  p1_.Reset(1, stage_.p1startx, stage_.p1starty);
  p2_.Reset(2, stage_.p2startx, stage_.p2starty);
  p1_.snap.facing = stage_.p1facing;
  p2_.snap.facing = stage_.p2facing;
  cns_.Enter(p1_, 0, 1);
  cns_.Enter(p2_, 0, 1);

  lua_.Init();
  lua_.LoadFile((root + "/cpp/lua/chars/kfm.lua").c_str());
  lua_.LoadFile((root + "/cpp/lua/common/training.lua").c_str());

  if (mode_ == "host") {
    if (!delayNet_.Listen(7500)) return false;
    screen_ = kNetWait;
    gameMode_ = "versus";
  } else if (mode_ == "connect") {
    std::string ip = connectIp_.empty() ? "127.0.0.1" : connectIp_;
    if (!delayNet_.BeginConnect(ip, 7500)) return false;
    screen_ = kNetWait;
    gameMode_ = "versus";
  } else if (mode_ == "rollback") {
    std::string ip = connectIp_.empty() ? "127.0.0.1" : connectIp_;
    if (!ggpo_.Init(this, 7550, 7600, ip, false, 2)) return false;
    screen_ = kNetWait;
    gameMode_ = "versus";
  } else if (mode_ == "rollback-host") {
    std::string ip = connectIp_.empty() ? "127.0.0.1" : connectIp_;
    if (!ggpo_.Init(this, 7600, 7550, ip, true, 2)) return false;
    screen_ = kNetWait;
    gameMode_ = "versus";
  }
  if (autoHit_) {
    training_ = true;
    gameMode_ = "training";
    p1AiLevel_ = 0;
    p2AiLevel_ = 0;
    ResetMatch();
    screen_ = kFight;
  }
  return true;
}

void Engine::DetectHits() {
  cns_.GlobalCollision(p1_, p2_);
}

void Engine::ResetMatch() {
  world_.Clear();
  p1_.Reset(1, stage_.p1startx, stage_.p1starty);
  p2_.Reset(2, stage_.p2startx, stage_.p2starty);
  p1_.id = 0;
  p2_.id = 1;
  p1_.parentId = p2_.parentId = -1;
  p1_.helperIndex = p2_.helperIndex = 0;
  p1_.snap.facing = stage_.p1facing;
  p2_.snap.facing = stage_.p2facing;
  cns_.Enter(p1_, 0, 1);
  cns_.Enter(p2_, 0, 1);
  frame_ = 0;
}

void Engine::Tick() {
  input_.Poll();
  if (GameLog::Get().FightOn()) {
    for (auto& ev : input_.KeyEvents())
      GameLog::Get().Fight("f=%d key %s", frame_, ev.c_str());
    if (input_.Clicked())
      GameLog::Get().Fight("f=%d click %d,%d screen=%s", frame_, input_.ClickX(), input_.ClickY(),
                           screen_ == kTitle ? "title" : "fight");
  }
  if (input_.Quit()) running_ = false;
  if (screen_ == kNetWait) {
    if (input_.EscPressed()) {
      delayNet_.Close();
      ggpo_.Close();
      screen_ = kTitle;
      return;
    }
    if (ggpo_.Active() && (mode_ == "rollback" || mode_ == "rollback-host")) {
      ggpo_.Idle();
      if (ggpoAbort_) {
        ggpo_.Close();
        ggpoAbort_ = false;
        screen_ = kTitle;
        return;
      }
      if (ggpo_.Running()) {
        ResetMatch();
        SaveGameState(0);
        screen_ = kFight;
      }
      return;
    }
    auto failNet = [&]() {
#ifdef _WIN32
      MessageBoxA(nullptr, "Network failed (port 7500).", "Ikemen net", MB_OK);
#endif
      delayNet_.Close();
      screen_ = kTitle;
    };
    if (delayNet_.Connecting()) {
      NetPump c = delayNet_.PumpConnect();
      if (c == NetPump::Ready) delayNet_.BeginHandshake(contentFingerprint());
      else if (c == NetPump::Fail) { failNet(); return; }
    } else if (delayNet_.Listening() && !delayNet_.Active()) {
      int a = delayNet_.TryAccept();
      if (a > 0) delayNet_.BeginHandshake(contentFingerprint());
      else if (a < 0) { failNet(); return; }
    }
    if (delayNet_.Active() && !delayNet_.Synced()) {
      NetPump h = NetPump::Pending;
      for (int i = 0; i < 8; i++) {
        h = delayNet_.PumpHandshake();
        if (h != NetPump::Pending) break;
      }
      if (h == NetPump::Ready) {
        GameLog::Get().Info("delay handshake ready, entering fight");
        std::srand(delayNet_.Seed());
        ResetMatch();
        screen_ = kFight;
      } else if (h == NetPump::Fail) failNet();
    }
    return;
  }
  if (screen_ == kTitle) {
    auto act = title_.Tick(input_.Pressed(), input_.EscPressed(), 1.f,
                           input_.ClickX(), input_.ClickY(), input_.Clicked());
    if (act == TitleScreen::StartFight) {
      int m = title_.FightMenu();
      p1AiLevel_ = 0;
      p2AiLevel_ = 0;
      if (m == 2) gameMode_ = "versus";
      else if (m == 4) gameMode_ = "training";
      else if (m == 6) {
        gameMode_ = "watch";
        p1AiLevel_ = 4.f;
        p2AiLevel_ = 4.f;
      } else if (m == 31 || m == 32) {
        gameMode_ = "versus";
        if (m == 31) {
          if (!delayNet_.Listen(7500)) {
#ifdef _WIN32
            MessageBoxA(nullptr, "Cannot listen on port 7500.", "Ikemen net", MB_OK);
#endif
            return;
          }
          training_ = false;
          screen_ = kNetWait;
          return;
        }
        std::string ip = connectIp_.empty() ? "127.0.0.1" : connectIp_;
        if (!delayNet_.BeginConnect(ip, 7500)) {
#ifdef _WIN32
          MessageBoxA(nullptr, "Join failed (need a host on 7500).", "Ikemen net", MB_OK);
#endif
          return;
        }
        training_ = false;
        screen_ = kNetWait;
        return;
      } else {
        gameMode_ = "arcade";
        p2AiLevel_ = 4.f;
      }
      training_ = gameMode_ == "training";
      p1ai_ = AiInput{};
      p2ai_ = AiInput{};
      ResetMatch();
      screen_ = kFight;
    } else if (act == TitleScreen::Quit) {
      running_ = false;
    }
    return;
  }
  if (input_.EscPressed()) {
    delayNet_.Close();
    screen_ = kTitle;
    return;
  }
  uint32_t i1 = input_.P1();
  uint32_t i2 = input_.P2();
  if (autoHit_ && screen_ == kFight) {
    if (frame_ < 80) i1 = kInputR;
    else if ((frame_ % 50) < 8) i1 = kInputA;
    else i1 = 0;
    i2 = 0;
    if (frame_ >= 420) running_ = false;
  }
  bool netOn = delayNet_.Active() || ggpo_.Active();
  if (!netOn) {
    if (p1AiLevel_ > 0) {
      p1ai_.Update(p1AiLevel_);
      i1 = p1ai_.Bits();
    }
    if (p2AiLevel_ > 0) {
      p2ai_.Update(p2AiLevel_);
      i2 = p2ai_.Bits();
    }
  }
  if (delayNet_.Synced()) {
    uint32_t locPlay = i1, remPlay = 0;
    NetPump ex = delayNet_.TryExchange(i1, locPlay, remPlay);
    if (ex == NetPump::Pending) return;
    if (ex == NetPump::Fail) {
      delayNet_.Close();
      screen_ = kTitle;
      return;
    }
    if (delayNet_.IsHost()) { i1 = locPlay; i2 = remPlay; }
    else { i2 = locPlay; i1 = remPlay; }
  }
  if (ggpo_.Active()) {
    ggpo_.Idle();
    if (ggpoAbort_) {
      ggpo_.Close();
      ggpoAbort_ = false;
      screen_ = kTitle;
      return;
    }
    uint8_t buf[kGgpoInputSize]{};
    uint16_t bits = (uint16_t)((ggpo_.IsHost() ? i1 : i2) & 0x3FFF);
    buf[0] = (uint8_t)bits;
    buf[1] = (uint8_t)(bits >> 8);
    if (!ggpo_.AddLocalInput(buf, kGgpoInputSize)) return;
    uint8_t ins[kGgpoPlayers][kGgpoInputSize]{};
    ggpo_.SyncInput(ins);
    i1 = (uint32_t)ins[0][0] | ((uint32_t)ins[0][1] << 8);
    i2 = (uint32_t)ins[1][0] | ((uint32_t)ins[1][1] << 8);
  }

  p1_.snap.input = i1;
  p2_.snap.input = i2;

  auto autoTurn = [](Fighter& a, Fighter& b) {
    // Go commandUpdate autoTurn: ctrl (or roundState>2) and states 0/11/20/52
    if (a.snap.asf & ASF_noautoturn) return;
    int st = a.snap.state;
    bool ok = a.snap.ctrl && (st == 0 || st == 11 || st == 20 || (st == 52 && a.snap.animEnded));
    if (!ok) return;
    int want = a.snap.pos.x > b.snap.pos.x ? -1 : 1;
    if (want != a.snap.facing) a.snap.facing = want;
  };
  autoTurn(p1_, p2_);
  autoTurn(p2_, p1_);

  p1_.cmd.Push(i1 | p1_.snap.assertInput, p1_.snap.facing);
  p2_.cmd.Push(i2 | p2_.snap.assertInput, p2_.snap.facing);
  if (!netOn) {
    if (p1AiLevel_ > 0 && !(p1_.snap.asf & ASF_noaicheat)) p1_.cmd.Cheat(p1AiLevel_);
    if (p2AiLevel_ > 0 && !(p2_.snap.asf & ASF_noaicheat)) p2_.cmd.Cheat(p2AiLevel_);
  }
  cns_.ActionPrepare(p1_);
  cns_.ActionPrepare(p2_);

  i1hist_[(unsigned)frame_ & 255] = i1;
  i2hist_[(unsigned)frame_ & 255] = i2;

  SimulateFight();
  if (ggpo_.Active()) {
    ggpo_.AdvanceFrame(rbStore_.Checksum(p1_.snap, p2_.snap));
    frame_ = ggpo_.Frame();
  }
  if (GameLog::Get().FightOn()) {
    auto fired = p1_.cmd.Fired();
    uint32_t bits = p1_.snap.input;
    bool cmdchg = !fired.empty();
    if (bits != lastLogBits_ || p1_.snap.state != lastLogState_ || p2_.snap.state != lastLogState2_ ||
        cmdchg || !input_.KeyEvents().empty()) {
      char cmds[128];
      cmds[0] = 0;
      if (fired.empty()) std::snprintf(cmds, sizeof(cmds), "-");
      else {
        size_t off = 0;
        for (size_t i = 0; i < fired.size() && off + 16 < sizeof(cmds); i++) {
          int n = std::snprintf(cmds + off, sizeof(cmds) - off, "%s%s", i ? "," : "", fired[i].c_str());
          if (n > 0) off += (size_t)n;
        }
      }
      GameLog::Get().Fight(
          "f=%d P1 keys=%s cmds=%s st=%d ctrl=%d t=%c vel=%.2f,%.2f pos=%.1f,%.1f pause=%d | P2 st=%d vel=%.2f,%.2f pos=%.1f,%.1f pause=%d life=%d",
          frame_, bitsStr(bits).c_str(), cmds,
          p1_.snap.state, p1_.snap.ctrl, p1_.snap.stateType,
          p1_.snap.vel.x, p1_.snap.vel.y, p1_.snap.pos.x, p1_.snap.pos.y, p1_.snap.hitpause,
          p2_.snap.state, p2_.snap.vel.x, p2_.snap.vel.y, p2_.snap.pos.x, p2_.snap.pos.y,
          p2_.snap.hitpause, p2_.snap.life);
      lastLogBits_ = bits;
      lastLogState_ = p1_.snap.state;
      lastLogState2_ = p2_.snap.state;
    }
  }
  if (!ggpo_.Active()) frame_++;
}

void Engine::SimulateFight() {
  auto step = [&](Fighter& f, Fighter& o) {
    cns_.ApplyQueuedDamage(f);
    if (f.snap.hitpause > 0) f.snap.hitpause--;
    if (f.snap.hitpause <= 0) f.snap.time++;
    f.snap.asf = 0;
    cns_.RunMinusOne(f, o);
    cns_.RunCurrent(f, o);
  };
  step(p1_, p2_);
  step(p2_, p1_);
  size_t nhelp = world_.helpers.size();
  if (nhelp > (size_t)world_.helperMax) nhelp = (size_t)world_.helperMax;
  for (size_t i = 0; i < nhelp; i++) {
    Fighter& h = world_.helpers[i];
    if (h.helperIndex == 0) continue;
    Fighter& o = (h.playerIndex == 0) ? p2_ : p1_;
    step(h, o);
    cns_.ApplyPhysics(h, stage_.leftbound, stage_.rightbound);
    h.TickAnim();
  }
  if (gameMode_ == "training") {
    lua_.CallTraining(p1_, 2, "training");
    lua_.CallTraining(p2_, 2, "training");
  }
  cns_.ApplyPhysics(p1_, stage_.leftbound, stage_.rightbound);
  cns_.ApplyPhysics(p2_, stage_.leftbound, stage_.rightbound);
  DetectHits();
  for (auto& p : world_.popups) {
    p.ttl--;
    p.pos.y -= 0.7f;
  }
  world_.popups.erase(std::remove_if(world_.popups.begin(), world_.popups.end(),
                                     [](const DamagePopup& p) { return p.ttl <= 0; }),
                      world_.popups.end());
  if (p1_.snap.moveType != 'A' && p1_.snap.moveType != 'H' && p1_.snap.ctrl)
    world_.comboHits[0] = world_.comboDmg[0] = 0;
  if (p2_.snap.moveType != 'A' && p2_.snap.moveType != 'H' && p2_.snap.ctrl)
    world_.comboHits[1] = world_.comboDmg[1] = 0;
  cns_.TickProjectiles(p1_, p2_, stage_.leftbound, stage_.rightbound);
  p1_.TickAnim();
  p2_.TickAnim();
  cam_.Update(p1_, p2_);
}

void Engine::DrawFight() {
  render_.Begin();
  render_.DrawRect(0, 0, (float)kGameW, (float)kGameH, 0, 0, 0, 1);
  stage_.Draw(render_, cam_.X(), 0, 0);
  auto world = [&](const Fighter& f, float& x, float& y, float& facing, float& scl) {
    const AnimFrame* fr = f.CurrentFrame();
    scl = cam_.Scale();
    x = kGameW * 0.5f + (f.snap.pos.x - cam_.X()) * scl;
    y = stage_.zoffset + f.snap.pos.y * scl;
    facing = (float)f.snap.facing;
    if (fr && fr->flipH) facing = -facing;
    if (fr) {
      x += (float)fr->x * facing * scl;
      y += (float)fr->y * scl;
    }
  };
  auto drawF = [&](Fighter& f, bool shadow) {
    const SpriteImage* s = f.CurrentSprite();
    float x, y, facing, scl;
    world(f, x, y, facing, scl);
    if (!s) {
      if (!shadow) render_.DrawRect(x - 8, y - 40, 16, 40, 0.8f, 0.8f, 0.2f, 1);
      return;
    }
    if (shadow) {
      float sy = stage_.shadowYScale * scl;
      if (sy > 0) sy = -sy;
      render_.DrawSprite(*s, x, y, facing, scl, sy, 0, 0, 0, stage_.shadowAlpha);
    } else {
      render_.DrawSprite(*s, x, y, facing, scl);
    }
  };
  drawF(p1_, true);
  drawF(p2_, true);
  drawF(p1_, false);
  drawF(p2_, false);
  for (auto& h : world_.helpers) drawF(h, false);
  for (auto& p : world_.projs) {
    if (!p.active) continue;
    float scl = cam_.Scale();
    float x = kGameW * 0.5f + (p.pos.x - cam_.X()) * scl;
    float y = stage_.zoffset + p.pos.y * scl;
    render_.DrawRect(x - 6, y - 6, 12, 12, 1, 0.8f, 0.2f, 1);
  }
  stage_.Draw(render_, cam_.X(), 0, 1);
  for (auto& p : world_.popups) {
    float scl = cam_.Scale();
    float x = kGameW * 0.5f + (p.pos.x - cam_.X()) * scl;
    float y = stage_.zoffset + p.pos.y * scl;
    float a = (float)p.ttl / (float)p.ttlMax;
    if (p.guarded)
      render_.DrawDigits(x - 8.f, y, p.amount, 1.8f, 0.45f, 0.75f, 1, a);
    else
      render_.DrawDigits(x - 8.f, y, p.amount, 2.2f, 1, 0.2f, 0.15f, a);
  }
  hud_.Draw(render_, p1_, p2_, world_);
  render_.End();
}

void Engine::Run() {
  const uint32_t frameMs = 16;
  uint32_t next = SDL_GetTicks();
  while (running_) {
    uint32_t now = SDL_GetTicks();
    int32_t wait = (int32_t)(next - now);
    if (wait > 0) {
      SDL_Delay((uint32_t)wait);
      continue;
    }
    if ((int32_t)(now - next) > 80) next = now;
    Tick();
    if (screen_ == kTitle) {
      render_.Begin(1280.f, 720.f);
      title_.Draw(render_);
      render_.End();
    } else if (screen_ == kNetWait) {
      render_.Begin(1280.f, 720.f);
      title_.DrawWait(render_, ggpo_.Active() ? ggpo_.WaitLabel() : delayNet_.WaitLabel());
      render_.End();
    } else {
      DrawFight();
    }
    SDL_GL_SwapWindow(gWin);
    next += frameMs;
  }
}

void Engine::Shutdown() {
  delayNet_.Close();
  ggpo_.Close();
  lua_.Shutdown();
  render_.Shutdown();
  if (gCtx) SDL_GL_DeleteContext(gCtx);
  if (gWin) SDL_DestroyWindow(gWin);
  GameLog::Get().Close();
  SDL_Quit();
}

int Engine::SaveGameState(int slot) {
  rbStore_.SaveSlot(slot, ggpo_.Frame(), p1_.snap, p2_.snap, world_);
  return (int)rbStore_.Checksum(p1_.snap, p2_.snap);
}

void Engine::LoadGameState(int slot) {
  int fr = 0;
  if (!rbStore_.LoadSlot(slot, fr, p1_.snap, p2_.snap, world_)) return;
  frame_ = ggpo_.Frame();
}

void Engine::AdvanceFrame(int) {
  uint8_t ins[kGgpoPlayers][kGgpoInputSize]{};
  ggpo_.SyncInput(ins);
  uint32_t i1 = (uint32_t)ins[0][0] | ((uint32_t)ins[0][1] << 8);
  uint32_t i2 = (uint32_t)ins[1][0] | ((uint32_t)ins[1][1] << 8);
  p1_.snap.input = i1;
  p2_.snap.input = i2;
  p1_.cmd.Push(i1 | p1_.snap.assertInput, p1_.snap.facing);
  p2_.cmd.Push(i2 | p2_.snap.assertInput, p2_.snap.facing);
  cns_.ActionPrepare(p1_);
  cns_.ActionPrepare(p2_);
  SimulateFight();
  ggpo_.AdvanceFrame(rbStore_.Checksum(p1_.snap, p2_.snap));
  frame_ = ggpo_.Frame();
}

void Engine::OnEvent(const GgpoEvent& ev) {
  if (ev.code == GgpoEvent::Desync || ev.code == GgpoEvent::Disconnect) ggpoAbort_ = true;
}
