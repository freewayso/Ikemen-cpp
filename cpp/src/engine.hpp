#pragma once
#include "render.hpp"
#include "sff.hpp"
#include "air.hpp"
#include "fighter.hpp"
#include "lua_host.hpp"
#include "cns.hpp"
#include "input.hpp"
#include "net.hpp"
#include "room_sync.hpp"
#include "camera.hpp"
#include "rollback.hpp"
#include "stage.hpp"
#include "title.hpp"
#include "world.hpp"
#include "lobby.hpp"
#include <array>
#include <cstdio>
#include <string>
#include <vector>

class Engine : public GgpoSession {
 public:
  bool Init(int argc, char** argv);
  void Run();
  void Shutdown();
  int SaveGameState(int slot) override;
  void LoadGameState(int slot) override;
  void AdvanceFrame(int flags) override;
  void OnEvent(const GgpoEvent& ev) override;
 private:
  void Tick();
  void DetectHits();
  void ResetMatch();
  void NextRound();
  void StepRoundState();
  void DrawFight();
  void SimulateFight();
  void StepFightWithInputs(uint32_t i1, uint32_t i2, bool netOn);
  bool beginRoom(bool host);
  bool beginLobby();
  void openLogin(bool connectNow);
  bool applyIpSettings(bool saveFile);
  void leaveLobbyUi();
  void backToLobby();
  std::string contentFingerprint() const;
  Renderer render_;
  Sff sff_;
  Sff fxSff_;
  AirBank air_;
  Fighter p1_, p2_;
  LuaHost lua_;
  InputSys input_;
  DelayNet delayNet_;
  RoomSync room_;
  GgpoPeer ggpo_;
  RollbackStore rbStore_;
  bool ggpoAbort_ = false;
  Camera cam_;
  Hud hud_;
  Stage stage_;
  TitleScreen title_;
  CnsBank cns_;
  LobbyClient lobby_;
  enum { kTitle, kFight, kNetWait, kLogin, kRooms };
  int screen_ = kTitle;
  bool running_ = true;
  bool training_ = false;
  int frame_ = 0;
  int netSendFrame_ = 0;
  uint32_t lastRoomStepMs_ = 0;
  uint32_t lastNetSendMs_ = 0;
  std::string mode_ = "local";
  std::string connectIp_;
  std::string relayHost_ = "127.0.0.1";
  int relayPort_ = 9000;
  std::string roomName_ = "kfm1";
  std::string lobbyHost_ = "127.0.0.1";
  int lobbyPort_ = 8080;
  std::string loginUser_;
  std::string loginPass_;
  std::string lobbyEdit_;
  std::string relayEdit_;
  std::string loginNote_;
  int loginField_ = 0;
  int roomCursor_ = 0;
  std::string autoUser_;
  bool autoCreate_ = false;
  bool autoJoin_ = false;
  bool autoAuthSent_ = false;
  FILE* inputLog_ = nullptr;
  uint32_t lastLogBits_ = 0xFFFFFFFFu;
  int lastLogState_ = -999;
  std::string gameMode_ = "arcade";
  float p1AiLevel_ = 0.f;
  float p2AiLevel_ = 4.f;
  AiInput p1ai_, p2ai_;
  FightWorld world_;
  std::vector<SpriteImage> cannonShot_, cannonBoom_, cannonMuzzle_;
  std::array<uint32_t, 256> i1hist_{}, i2hist_{};
  int fpsShow_ = 0;
  int fpsCount_ = 0;
  uint32_t fpsMs_ = 0;
};
