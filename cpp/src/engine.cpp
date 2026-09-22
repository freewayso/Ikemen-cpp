#include "engine.hpp"
#include "stage.hpp"
#include "ini.hpp"
#ifndef __ANDROID__
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#ifdef _WIN32
#include <SDL_syswm.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <imm.h>
#endif
#ifdef __ANDROID__
#include "android_fs.hpp"
#endif
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <string>
#include <fstream>
#include <array>
#include <vector>

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
  struct stat st {};
  return ::stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

static std::string repoRoot() {
#ifdef __ANDROID__
  std::string root;
  if (AndroidPrepareDataRoot(root) && dirExists(root + "/chars") && dirExists(root + "/stages"))
    return root;
  const char* base = SDL_AndroidGetInternalStoragePath();
  if (base && base[0]) return std::string(base) + "/ikemen";
  return ".";
#endif
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
  int relayPort = 0;
  std::string room;
  bool roomArg = false;
  std::string lobbyHostArg;
  int lobbyPortArg = 0;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--host")) mode_ = "host";
    else if (!std::strcmp(argv[i], "--connect")) {
      mode_ = "connect";
      if (i + 1 < argc && argv[i + 1][0] != '-') connectIp_ = argv[++i];
    } else if (!std::strcmp(argv[i], "--rbhost")) {
      mode_ = "rollback-host";
      if (i + 1 < argc && argv[i + 1][0] != '-') connectIp_ = argv[++i];
    } else if (!std::strcmp(argv[i], "--rollback") && i + 1 < argc) {
      mode_ = "rollback";
      connectIp_ = argv[++i];
    } else if (!std::strcmp(argv[i], "--lobby") && i + 1 < argc) {
      std::string r = argv[++i];
      auto c = r.find(':');
      if (c != std::string::npos) {
        lobbyHostArg = r.substr(0, c);
        lobbyPortArg = std::atoi(r.c_str() + c + 1);
      } else lobbyHostArg = r;
    } else if (!std::strcmp(argv[i], "--relay") && i + 1 < argc) {
      std::string r = argv[++i];
      auto c = r.find(':');
      if (c != std::string::npos) {
        relayHost = r.substr(0, c);
        relayPort = std::atoi(r.c_str() + c + 1);
      } else relayHost = r;
    } else if (!std::strcmp(argv[i], "--room") && i + 1 < argc) {
      room = argv[++i];
      roomArg = true;
    } else if (!std::strcmp(argv[i], "--user") && i + 1 < argc) {
      autoUser_ = argv[++i];
    } else if (!std::strcmp(argv[i], "--create")) {
      autoCreate_ = true;
    } else if (!std::strcmp(argv[i], "--join")) {
      autoJoin_ = true;
    }
  }

  SDL_SetHint(SDL_HINT_IME_SHOW_UI, "0");
#ifndef __ANDROID__
  SDL_SetMainReady();
#endif
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
    std::fprintf(stderr, "SDL: %s\n", SDL_GetError());
#ifdef _WIN32
    MessageBoxA(nullptr, SDL_GetError(), "Ikemen SDL init failed", MB_OK);
#endif
    return false;
  }

  std::string root = repoRoot();
#ifdef _WIN32
  SetCurrentDirectoryA(root.c_str());
#else
  chdir(root.c_str());
#endif
  NetCfg net = LoadNetIni();
  relayHost_ = net.relay;
  relayPort_ = net.port;
  roomName_ = net.room;
  lobbyHost_ = net.lobby.empty() ? net.relay : net.lobby;
  lobbyPort_ = net.lobbyPort;
  if (!relayHost.empty()) relayHost_ = relayHost;
  if (relayPort > 0) relayPort_ = relayPort;
  if (roomArg) roomName_ = room;
  if (!lobbyHostArg.empty()) lobbyHost_ = lobbyHostArg;
  if (lobbyPortArg > 0) lobbyPort_ = lobbyPortArg;
  std::srand((unsigned)std::time(nullptr));
  inputLog_ = std::fopen("input.log", "w");
  if (inputLog_) {
    std::fprintf(inputLog_, "# key/button log  P1: WASD move  Y=x H=y J=a K=b  Space=start\n");
    std::fflush(inputLog_);
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
#ifdef __ANDROID__
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
  int wx = 40, wy = 40;
  const char* wtitle = "Ikemen GO";
  if (mode_ == "host" || mode_ == "rollback-host" || autoCreate_) {
    wx = 20;
    wy = 40;
    wtitle = "Ikemen GO [HOST]";
  } else if (mode_ == "connect" || mode_ == "rollback" || autoJoin_) {
    wx = 660;
    wy = 80;
    wtitle = "Ikemen GO [CLIENT]";
  }
  Uint32 wflags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
#ifdef __ANDROID__
  wflags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_ALLOW_HIGHDPI;
#endif
  gWin = SDL_CreateWindow(wtitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, wflags);
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
  int dw = 1280, dh = 720;
  SDL_GL_GetDrawableSize(gWin, &dw, &dh);
  if (!render_.Init(dw, dh)) {
#ifdef _WIN32
    MessageBoxA(nullptr, "OpenGL 3.3 shaders/buffers failed", "Ikemen render failed", MB_OK);
#endif
    return false;
  }
  SDL_StopTextInput();
#ifndef __ANDROID__
  SDL_SetWindowPosition(gWin, wx, wy);
  SDL_RaiseWindow(gWin);
#endif
  input_.BindWindow(gWin);
  input_.SetLetterbox(render_.VpX(), render_.VpTop(), render_.VpW(), render_.VpH(), render_.WinW(),
                      render_.WinH());
#ifdef __ANDROID__
  input_.SetVirtualPad(true);
#endif
#ifdef _WIN32
  SDL_SysWMinfo wminfo;
  SDL_VERSION(&wminfo.version);
  if (SDL_GetWindowWMInfo(gWin, &wminfo))
    ImmAssociateContext(wminfo.info.win.window, nullptr);
#endif

  std::string sff = root + "/chars/kfm/kfm.sff";
  std::string air = root + "/chars/kfm/kfm.air";
  std::string cmd = root + "/chars/kfm/kfm.cmd";
  if (!sff_.Load(sff)) std::fprintf(stderr, "warn: sff load failed %s\n", sff.c_str());
  if (!air_.Load(air)) std::fprintf(stderr, "warn: air load failed %s\n", air.c_str());
  if (!stage_.Load(root + "/stages/kfm.def"))
    std::fprintf(stderr, "warn: stage load failed\n");
  cam_.Setup(stage_.boundleft, stage_.boundright, stage_.tension);
  if (!hud_.Load(root + "/data/fight.sff"))
    std::fprintf(stderr, "warn: fight.sff load failed\n");
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
  cns_.world = &world_;
  p1_.cns = p2_.cns = &cns_;
  p1_.Reset(1, stage_.p1startx, stage_.p1starty);
  p2_.Reset(2, stage_.p2startx, stage_.p2starty);
  p1_.snap.facing = stage_.p1facing;
  p2_.snap.facing = stage_.p2facing;
  cns_.Enter(p1_, State::Stand, 1);
  cns_.Enter(p2_, State::Stand, 1);

  lua_.Init();
  lua_.LoadFile((root + "/cpp/lua/chars/kfm.lua").c_str());
  lua_.LoadFile((root + "/cpp/lua/common/training.lua").c_str());

  if (mode_ == "host") {
    if (!beginRoom(true)) return false;
  } else if (mode_ == "connect") {
    if (!beginRoom(false)) return false;
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
  } else {
    beginLobby();
  }
  return true;
}

bool Engine::beginRoom(bool host) {
  std::string ip = relayHost_;
  if (!host && !connectIp_.empty()) ip = connectIp_;
  int port = relayPort_;
  if (!room_.Start(host, ip, port, roomName_)) return false;
  screen_ = kNetWait;
  gameMode_ = "versus";
  training_ = false;
  SDL_StopTextInput();
  SDL_RaiseWindow(gWin);
  std::fprintf(stderr, "room %s %s:%d %.8s (need ikemen_relay)\n", host ? "host" : "guest", ip.c_str(),
               relayPort_, roomName_.c_str());
  return true;
}

bool Engine::beginLobby() {
  openLogin(false);
  return true;
}

void Engine::openLogin(bool connectNow) {
  loginPass_.clear();
#ifdef __ANDROID__
  loginField_ = 2;
#else
  loginField_ = 0;
#endif
  loginNote_.clear();
  lobbyEdit_ = FormatHostPort(lobbyHost_, lobbyPort_);
  relayEdit_ = FormatHostPort(relayHost_, relayPort_);
  if (connectNow) {
    if (!lobby_.Connect(lobbyHost_, lobbyPort_)) loginNote_ = "CONN FAIL";
  }
  SDL_StartTextInput();
  screen_ = kLogin;
}

bool Engine::applyIpSettings(bool saveFile) {
  ParseHostPort(lobbyEdit_, lobbyHost_, lobbyPort_);
  ParseHostPort(relayEdit_, relayHost_, relayPort_);
  if (lobbyHost_.empty()) lobbyHost_ = "127.0.0.1";
  if (relayHost_.empty()) relayHost_ = lobbyHost_;
  if (lobbyPort_ <= 0) lobbyPort_ = 8080;
  if (relayPort_ <= 0) relayPort_ = 9000;
  lobbyEdit_ = FormatHostPort(lobbyHost_, lobbyPort_);
  relayEdit_ = FormatHostPort(relayHost_, relayPort_);
  if (saveFile) {
    NetCfg c;
    c.relay = relayHost_;
    c.port = relayPort_;
    c.room = roomName_;
    c.lobby = lobbyHost_;
    c.lobbyPort = lobbyPort_;
    if (!SaveNetIni(c)) {
      loginNote_ = "SAVE FAIL";
      return false;
    }
    loginNote_ = std::string("SAVED ") + lobbyEdit_;
  }
  return true;
}

void Engine::leaveLobbyUi() {
  SDL_StopTextInput();
  lobby_.Close();
  screen_ = kTitle;
}

void Engine::backToLobby() {
  delayNet_.Close();
  room_.Close();
  ggpo_.Close();
  if (lobby_.LoggedIn()) {
    lobby_.SendLeave();
    screen_ = kRooms;
  } else {
    screen_ = kTitle;
  }
}

void Engine::DetectHits() {
  if (world_.RoundNoDamage()) return;
  if (world_.finishType != FinishType::NotYet && world_.intro < -world_.overHitTime) return;
  cns_.GlobalCollision(p1_, p2_);
}

void Engine::StepFightWithInputs(uint32_t i1, uint32_t i2, bool netOn) {
  p1_.snap.input = i1;
  p2_.snap.input = i2;

  auto autoTurn = [](Fighter& a, Fighter& b) {
    if (a.snap.asf & ASF_noautoturn) return;
    int st = a.snap.state;
    bool ok = a.snap.ctrl && StateAllowsAutoTurn(st, a.snap.animEnded != 0);
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

  i1hist_[(unsigned)frame_ & 255] = i1;
  i2hist_[(unsigned)frame_ & 255] = i2;

  SimulateFight();
  if (ggpo_.Active()) {
    ggpo_.AdvanceFrame(rbStore_.Checksum(p1_.snap, p2_.snap));
    frame_ = ggpo_.Frame();
  }
  if (inputLog_ && !room_.Active()) {
    auto fired = p1_.cmd.Fired();
    uint32_t bits = p1_.snap.input;
    bool cmdchg = !fired.empty();
    if (bits != lastLogBits_ || p1_.snap.state != lastLogState_ || cmdchg || !input_.KeyEvents().empty()) {
      std::fprintf(inputLog_,
                   "f=%d fight P1 keys=%s cmds=", frame_, bitsStr(bits).c_str());
      if (fired.empty()) std::fprintf(inputLog_, "-");
      else {
        for (size_t i = 0; i < fired.size(); i++) {
          if (i) std::fprintf(inputLog_, ",");
          std::fprintf(inputLog_, "%s", fired[i].c_str());
        }
      }
      std::fprintf(inputLog_, " state=%d ctrl=%d type=%c vel=%.2f,%.2f pos=%.1f,%.1f pause=%d\n",
                   p1_.snap.state, p1_.snap.ctrl, p1_.snap.stateType,
                   p1_.snap.vel.x, p1_.snap.vel.y, p1_.snap.pos.x, p1_.snap.pos.y, p1_.snap.hitpause);
      std::fflush(inputLog_);
      lastLogBits_ = bits;
      lastLogState_ = p1_.snap.state;
    }
  }
  if (!ggpo_.Active()) frame_++;
}

void Engine::ResetMatch() {
  world_.ResetMatch();
  p1_.Reset(1, stage_.p1startx, stage_.p1starty);
  p2_.Reset(2, stage_.p2startx, stage_.p2starty);
  p1_.id = 0;
  p2_.id = 1;
  p1_.parentId = p2_.parentId = -1;
  p1_.helperIndex = p2_.helperIndex = 0;
  p1_.snap.facing = stage_.p1facing;
  p2_.snap.facing = stage_.p2facing;
  cns_.Enter(p1_, State::Stand, 1);
  cns_.Enter(p2_, State::Stand, 1);
  frame_ = 0;
  netSendFrame_ = 0;
  lastNetSendMs_ = 0;
}

void Engine::NextRound() {
  int w0 = world_.wins[0], w1 = world_.wins[1], rn = world_.roundNo + 1;
  world_.ResetRound();
  world_.wins[0] = w0;
  world_.wins[1] = w1;
  world_.roundNo = rn;
  p1_.Reset(1, stage_.p1startx, stage_.p1starty);
  p2_.Reset(2, stage_.p2startx, stage_.p2starty);
  p1_.id = 0;
  p2_.id = 1;
  p1_.snap.facing = stage_.p1facing;
  p2_.snap.facing = stage_.p2facing;
  cns_.Enter(p1_, State::Stand, 1);
  cns_.Enter(p2_, State::Stand, 1);
}

void Engine::StepRoundState() {
  FinishType before = world_.finishType;
  if (world_.finishType == FinishType::NotYet) {
    bool ko0 = !p1_.snap.alive || p1_.snap.life <= 0;
    bool ko1 = !p2_.snap.alive || p2_.snap.life <= 0;
    if (ko0 || ko1) {
      if (ko0 && ko1) {
        world_.finishType = FinishType::DKO;
        world_.winTeam = -1;
      } else {
        world_.finishType = FinishType::KO;
        world_.winTeam = ko0 ? 1 : 0;
      }
    }
  }
  if (before == FinishType::NotYet && world_.finishType != FinishType::NotYet) {
    if (world_.winTeam == 0) world_.wins[0]++;
    else if (world_.winTeam == 1) world_.wins[1]++;
    if (world_.wins[0] >= world_.roundsToWin || world_.wins[1] >= world_.roundsToWin)
      world_.matchOver = 1;
  }
  if (world_.finishType != FinishType::NotYet) world_.intro--;
  if (world_.RoundOver()) {
    if (world_.matchOver) {
      delayNet_.Close();
      room_.Close();
      backToLobby();
    } else {
      NextRound();
    }
  }
}

void Engine::Tick() {
  input_.SetCombatPad(screen_ == kFight);
  input_.Poll();
  if (inputLog_) {
    for (auto& ev : input_.KeyEvents())
      std::fprintf(inputLog_, "f=%d key %s\n", frame_, ev.c_str());
    if (input_.Clicked())
      std::fprintf(inputLog_, "f=%d click %d,%d screen=%s\n", frame_, input_.ClickX(), input_.ClickY(),
                   screen_ == kTitle ? "title" : "fight");
  }
  if (input_.Quit()) running_ = false;
  if (screen_ == kNetWait) {
    if (input_.EscPressed()) {
      backToLobby();
      return;
    }
    if (room_.Active()) {
      room_.Pump();
      if (room_.Dropped()) {
        backToLobby();
        return;
      }
      if (room_.Joined()) {
        std::srand(room_.Seed());
        ResetMatch();
        lastRoomStepMs_ = 0;
        SDL_StopTextInput();
        SDL_RaiseWindow(gWin);
        SDL_SetWindowInputFocus(gWin);
        screen_ = kFight;
      }
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
        std::fprintf(stderr, "delay handshake ready, entering fight\n");
        std::fflush(stderr);
        std::srand(delayNet_.Seed());
        ResetMatch();
        screen_ = kFight;
      } else if (h == NetPump::Fail) failNet();
    }
    return;
  }
  if (screen_ == kLogin) {
    lobby_.Pump();
    if (lobby_.LoggedIn()) {
      SDL_StopTextInput();
      screen_ = kRooms;
      return;
    }
    if (!autoUser_.empty() && !autoAuthSent_) {
      const std::string& st = lobby_.Status();
      if (!lobby_.Active()) {
        applyIpSettings(false);
        lobby_.Connect(lobbyHost_, lobbyPort_);
      }
      if (st != "CONNECTING" && st != "LOGIN" && st != "REGISTER" && st != "CONN FAIL") {
        loginUser_ = autoUser_;
        autoAuthSent_ = true;
        lobby_.SendRegister(autoUser_, "");
      }
    }
    if (input_.EscPressed()) {
      running_ = false;
      return;
    }
    if (input_.Tab()) loginField_ = (loginField_ + 1) % 4;
    auto tryAuth = [&](bool reg) {
      if (loginUser_.empty()) {
        loginField_ = 0;
        loginNote_ = "NEED USER";
        return;
      }
      applyIpSettings(true);
      lobby_.Close();
      if (!lobby_.Connect(lobbyHost_, lobbyPort_)) {
        loginNote_ = "CONN FAIL";
        return;
      }
      const std::string& st = lobby_.Status();
      if (st == "REGISTER" || st == "LOGIN") return;
      if (reg) lobby_.SendRegister(loginUser_, loginPass_);
      else lobby_.SendLogin(loginUser_, loginPass_);
      loginNote_ = lobby_.Status();
    };
    auto appendChar = [&](char ch) {
      std::string* t = &loginUser_;
      size_t cap = 16;
      if (loginField_ == 1) {
        t = &loginPass_;
        cap = 16;
      } else if (loginField_ == 2) {
        t = &lobbyEdit_;
        cap = 22;
      } else if (loginField_ == 3) {
        t = &relayEdit_;
        cap = 22;
      }
      if (t->size() < cap) t->push_back(ch);
    };
    auto delChar = [&]() {
      std::string* t = loginField_ == 0 ? &loginUser_ : loginField_ == 1 ? &loginPass_ : loginField_ == 2 ? &lobbyEdit_ : &relayEdit_;
      if (!t->empty()) t->pop_back();
    };
    if (input_.Clicked()) {
      int hit = title_.HitLogin(input_.ClickX(), input_.ClickY(), true);
      if (hit >= 0 && hit <= 3) loginField_ = hit;
      else if (hit == 10) tryAuth(false);
      else if (hit == 11) tryAuth(true);
      else if (hit == 12) applyIpSettings(true);
      else if (hit >= 100 && hit < 112) {
        static const char keys[12] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '.', ':'};
        if (loginField_ < 2) loginField_ = 2;
        appendChar(keys[hit - 100]);
      } else if (hit == 200) delChar();
    }
    for (unsigned char ch : input_.Text()) {
      if (loginField_ <= 1) {
        if ((std::isalnum(ch) || ch == '_') && loginField_ == 0) appendChar((char)ch);
        else if (loginField_ == 1 && ch >= 32 && ch < 127) appendChar((char)ch);
      } else if ((std::isdigit(ch) || ch == '.' || ch == ':') || std::isalpha(ch)) {
        appendChar((char)ch);
      }
    }
    if (input_.Backspace()) delChar();
    if (input_.Pressed() & kInputS) tryAuth(false);
#ifdef __ANDROID__
    static const int fy[4] = {108, 162, 216, 270};
    SDL_Rect tr{280, fy[loginField_ < 0 || loginField_ > 3 ? 0 : loginField_], 960, 46};
    SDL_SetTextInputRect(&tr);
#endif
    return;
  }
  if (screen_ == kRooms) {
    lobby_.Pump();
    if (autoCreate_ && lobby_.LoggedIn() && lobby_.RoomId().empty() &&
        lobby_.Status() != "CREATE" && lobby_.Status() != "HOST WAIT") {
      autoCreate_ = false;
      lobby_.SendCreate();
    }
    if (autoJoin_ && lobby_.LoggedIn() && lobby_.RoomId().empty()) {
      for (auto& rm : lobby_.Rooms()) {
        if (rm.status == 0 && rm.n < (rm.maxn ? rm.maxn : 2) && !rm.id.empty()) {
          autoJoin_ = false;
          lobby_.SendJoin(rm.id);
          break;
        }
      }
    }
    if (lobby_.MatchReady()) {
      std::string id = lobby_.RoomId();
      while (id.size() < 8) id.push_back('x');
      if (id.size() > 8) id.resize(8);
      roomName_ = id;
      lobby_.ClearMatch();
      if (!beginRoom(lobby_.IsHost())) screen_ = kRooms;
      return;
    }
    if (input_.EscPressed()) {
      SDL_StopTextInput();
      screen_ = kTitle;
      return;
    }
    int n = (int)lobby_.Rooms().size();
    if (n > 0) {
      if (input_.Pressed() & kInputU) roomCursor_ = (roomCursor_ + n - 1) % n;
      if (input_.Pressed() & kInputD) roomCursor_ = (roomCursor_ + 1) % n;
      if (roomCursor_ >= n) roomCursor_ = 0;
    }
    bool create = (input_.Pressed() & kInputC) != 0;
    for (auto& ev : input_.KeyEvents())
      if (ev == "DOWN C") create = true;
    if (create) lobby_.SendCreate();
    auto tryJoin = [&](int idx) {
      if (idx < 0 || idx >= n) return;
      const auto& rm = lobby_.Rooms()[idx];
      if (rm.status != 0 || rm.n >= (rm.maxn ? rm.maxn : 2)) return;
      lobby_.SendJoin(rm.id);
    };
    if ((input_.Pressed() & kInputS) || (input_.Pressed() & kInputA)) tryJoin(roomCursor_);
    if (input_.Clicked()) {
      int mx = input_.ClickX();
      int my = input_.ClickY();
      if (mx >= 980 && my >= 10 && my <= 80) {
        lobby_.Close();
        openLogin(false);
        return;
      }
      for (int i = 0; i < 7 && i < n; i++) {
        float y = 160.f + i * 54.f;
        if (my >= y - 8 && my <= y + 40) {
          roomCursor_ = i;
          tryJoin(roomCursor_);
        }
      }
    }
    return;
  }
  if (screen_ == kTitle) {
    if (lobby_.Active()) lobby_.Pump();
    auto act = title_.Tick(input_.Pressed(), input_.EscPressed(), 1.f,
                           input_.ClickX(), input_.ClickY(), input_.Clicked());
    if (act == TitleScreen::StartFight) {
      int m = title_.FightMenu();
      p1AiLevel_ = 0;
      p2AiLevel_ = 0;
      if (m == TitleMenu::Versus) gameMode_ = "versus";
      else if (m == TitleMenu::Practice) gameMode_ = "training";
      else if (m == TitleMenu::Watch) {
        gameMode_ = "watch";
        p1AiLevel_ = 4.f;
        p2AiLevel_ = 4.f;
      } else if (m == TitleMenu::Online) {
        if (lobby_.LoggedIn()) {
          screen_ = kRooms;
          return;
        }
        if (!beginLobby()) {
#ifdef _WIN32
          MessageBoxA(nullptr, "Cannot connect lobby (TCP proto3).", "Ikemen net", MB_OK);
#endif
        }
        return;
      } else         if (m == TitleMenu::HostNet || m == TitleMenu::JoinNet) {
        gameMode_ = "versus";
        if (!beginRoom(m == TitleMenu::HostNet)) {
#ifdef _WIN32
          MessageBoxA(nullptr, "Cannot start room sync (UDP).", "Ikemen net", MB_OK);
#endif
          return;
        }
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
    backToLobby();
    return;
  }
  uint32_t i1 = input_.P1();
  uint32_t i2 = input_.P2();
  bool netOn = delayNet_.Active() || ggpo_.Active() || room_.Active();
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
  if (room_.Active()) {
    room_.Pump();
    if (room_.Dropped()) {
      room_.Close();
      screen_ = kTitle;
      return;
    }
    uint32_t local = input_.P1() | input_.P2();
    const int kAhead = 2;
    uint32_t now = SDL_GetTicks();
    if (netSendFrame_ < frame_) netSendFrame_ = frame_;
    if (netSendFrame_ < frame_ + kAhead && (lastNetSendMs_ == 0 || now - lastNetSendMs_ >= 16)) {
      room_.SendInput(netSendFrame_, local);
      netSendFrame_++;
      lastNetSendMs_ = now;
    }
    int behind = room_.ServerLatest() - room_.WantFrame();
    int maxSim = 1;
    if (behind > 8) maxSim = 16;
    else if (lastRoomStepMs_ != 0 && now - lastRoomStepMs_ < 15) maxSim = 0;
    int n = 0;
    uint32_t p0 = 0, p1 = 0;
    while (n < maxSim && room_.NextConfirm(p0, p1)) {
      StepFightWithInputs(p0, p1, true);
      n++;
    }
    if (n > 0) lastRoomStepMs_ = now;
    return;
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

  StepFightWithInputs(i1, i2, netOn);
}

void Engine::SimulateFight() {
  auto step = [&](Fighter& f, Fighter& o) {
    cns_.ApplyQueuedDamage(f);
    if (f.snap.hitpause > 0) f.snap.hitpause--;
    f.snap.asf = 0;
    if (f.snap.hitpause <= 0) {
      int st0 = f.snap.state;
      lua_.CallState(f, &o);
      // MUGEN Time=0 on the first tick of a new state; don't bump if ChangeState just reset it.
      if (f.snap.state == st0) f.snap.time++;
    }
    cns_.ActionFinish(f);
  };
  step(p1_, p2_);
  step(p2_, p1_);
  for (auto& h : world_.helpers) {
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
  if (p1_.snap.moveType != MoveType::Attack && p1_.snap.moveType != MoveType::Hit && p1_.snap.ctrl)
    world_.comboHits[0] = world_.comboDmg[0] = 0;
  if (p2_.snap.moveType != MoveType::Attack && p2_.snap.moveType != MoveType::Hit && p2_.snap.ctrl)
    world_.comboHits[1] = world_.comboDmg[1] = 0;
  cns_.TickProjectiles(p1_, p2_, stage_.leftbound, stage_.rightbound);
  p1_.TickAnim();
  p2_.TickAnim();
  cam_.Update(p1_, p2_);
  StepRoundState();
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
    render_.DrawRect(x - 11.f, y - 8.f, 22.f, 16.f, 1.f, 0.28f, 0.05f, 1);
    render_.DrawRect(x - 7.f, y - 5.f, 14.f, 10.f, 1.f, 0.75f, 0.15f, 1);
    float tip = p.facing >= 0 ? x + 6.f : x - 14.f;
    render_.DrawRect(tip, y - 3.f, 8.f, 6.f, 1.f, 0.95f, 0.45f, 0.95f);
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
  render_.DrawWord(4.f, 226.f, "FPS", 0.7f, 0.85f, 0.95f, 1, 1);
  render_.DrawDigits(28.f, 225.f, fpsShow_, 1.1f, 0.85f, 0.95f, 1, 1);
  if (room_.Active() && room_.Joined()) {
    int ping = room_.RttMs();
    render_.DrawWord(70.f, 226.f, "PING", 0.7f, 0.4f, 1, 0.5f, 1);
    render_.DrawDigits(108.f, 225.f, ping < 0 ? 0 : ping, 1.1f, 0.4f, 1, 0.5f, 1);
    render_.DrawWord(150.f, 226.f, "MS", 0.7f, 0.4f, 1, 0.5f, 1);
    render_.DrawWord(180.f, 226.f, "F", 0.7f, 1, 0.9f, 0.4f, 1);
    render_.DrawDigits(192.f, 225.f, frame_, 1.1f, 1, 0.9f, 0.4f, 1);
#ifndef __ANDROID__
    if (!(SDL_GetWindowFlags(gWin) & SDL_WINDOW_INPUT_FOCUS))
      render_.DrawWord(40.f, 110.f, "CLICK WINDOW TO MOVE", 1.4f, 1, 0.85f, 0.2f, 1);
#endif
  }
  render_.End();
  if (input_.VirtualPad() && input_.CombatPad()) {
    render_.BeginHud(1280.f, 720.f);
    auto disc = [&](float cx, float cy, float r, float rr, float gg, float bb, float aa) {
      render_.DrawRect(cx - r, cy - r, r * 2.f, r * 2.f, rr, gg, bb, aa);
      render_.DrawRect(cx - r * 0.72f, cy - r * 0.72f, r * 1.44f, r * 1.44f, rr * 0.85f, gg * 0.85f, bb * 0.85f, aa);
    };
    disc(input_.StickOX(), input_.StickOY(), VPad::DR, 0.22f, 0.28f, 0.38f, 0.38f);
    disc(input_.StickKX(), input_.StickKY(), VPad::KR, 0.55f, 0.72f, 0.95f, input_.StickHeld() ? 0.85f : 0.5f);
    disc(VPad::Jx, VPad::Jy, VPad::JR, 0.9f, 0.28f, 0.22f, input_.BtnJ() ? 0.9f : 0.5f);
    disc(VPad::Kx, VPad::Ky, VPad::BR, 0.22f, 0.48f, 0.95f, input_.BtnK() ? 0.9f : 0.5f);
    const Fighter& me = (room_.Active() && !room_.IsHost()) ? p2_ : p1_;
    bool skillOn = me.snap.rage >= me.snap.rageMax && me.snap.rageMax > 0;
    disc(VPad::Cx, VPad::Cy, VPad::CR, skillOn ? 1.f : 0.45f, skillOn ? 0.55f : 0.35f,
         skillOn ? 0.08f : 0.18f, input_.BtnC() || skillOn ? 0.95f : 0.4f);
    render_.DrawWord(VPad::Jx - 16.f, VPad::Jy - 14.f, "J", 8.f, 1, 1, 1, 1);
    render_.DrawWord(VPad::Kx - 16.f, VPad::Ky - 14.f, "K", 8.f, 1, 1, 1, 1);
    render_.DrawWord(VPad::Cx - 14.f, VPad::Cy - 12.f, "C", 6.5f, 1, 1, 1, 1);
    render_.End();
  }
}

void Engine::Run() {
  while (running_) {
    int dw = 0, dh = 0;
    if (gWin) SDL_GL_GetDrawableSize(gWin, &dw, &dh);
    if (dw > 0 && dh > 0 && (dw != render_.WinW() || dh != render_.WinH())) {
      render_.Resize(dw, dh);
      input_.SetLetterbox(render_.VpX(), render_.VpTop(), render_.VpW(), render_.VpH(), render_.WinW(),
                          render_.WinH());
    }
    Tick();
    if (screen_ == kTitle) {
      render_.Begin(1280.f, 720.f);
      title_.Draw(render_, lobby_.LoggedIn() ? lobby_.User().c_str() : nullptr);
      render_.End();
    } else if (screen_ == kLogin) {
      render_.Begin(1280.f, 720.f);
      const char* st = loginNote_.empty() ? lobby_.Status().c_str() : loginNote_.c_str();
      title_.DrawLogin(render_, loginUser_, loginPass_, lobbyEdit_, relayEdit_, loginField_, st, true);
      render_.End();
    } else if (screen_ == kRooms) {
      render_.Begin(1280.f, 720.f);
      std::vector<std::string> rows;
      for (auto& rm : lobby_.Rooms()) {
        char line[80];
        std::snprintf(line, sizeof(line), "%s  %s  %d/%d  %s", rm.id.c_str(),
                      rm.host.empty() ? rm.name.c_str() : rm.host.c_str(), rm.n, rm.maxn ? rm.maxn : 2,
                      rm.status ? "FIGHT" : "WAIT");
        rows.push_back(line);
      }
      title_.DrawRooms(render_, lobby_.User(), rows, roomCursor_, lobby_.Status().c_str());
      render_.End();
    } else if (screen_ == kNetWait) {
      render_.Begin(1280.f, 720.f);
      char sub[96];
      std::snprintf(sub, sizeof(sub), "relay %s:%d  room %s", relayHost_.c_str(), relayPort_,
                    roomName_.c_str());
      title_.DrawWait(render_,
                      room_.Active() ? room_.WaitLabel()
                                     : (ggpo_.Active() ? ggpo_.WaitLabel() : delayNet_.WaitLabel()),
                      sub);
      render_.End();
    } else {
      DrawFight();
    }
    SDL_GL_SwapWindow(gWin);
    fpsCount_++;
    uint32_t now = SDL_GetTicks();
    if (fpsMs_ == 0) fpsMs_ = now;
    uint32_t dt = now - fpsMs_;
    if (dt >= 500) {
      fpsShow_ = fpsCount_ * 1000 / (int)(dt ? dt : 1);
      fpsCount_ = 0;
      fpsMs_ = now;
    }
    SDL_Delay(8);
  }
}

void Engine::Shutdown() {
  delayNet_.Close();
  room_.Close();
  ggpo_.Close();
  lobby_.Close();
  lua_.Shutdown();
  render_.Shutdown();
  if (gCtx) SDL_GL_DeleteContext(gCtx);
  if (gWin) SDL_DestroyWindow(gWin);
  if (inputLog_) { std::fclose(inputLog_); inputLog_ = nullptr; }
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
  SimulateFight();
  ggpo_.AdvanceFrame(rbStore_.Checksum(p1_.snap, p2_.snap));
  frame_ = ggpo_.Frame();
}

void Engine::OnEvent(const GgpoEvent& ev) {
  if (ev.code == GgpoEvent::Desync || ev.code == GgpoEvent::Disconnect) ggpoAbort_ = true;
}
