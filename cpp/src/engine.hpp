#pragma once
#include "render.hpp"
#include "sff.hpp"
#include "air.hpp"
#include "fighter.hpp"
#include "lua_host.hpp"
#include "cns.hpp"
#include "input.hpp"
#include "net.hpp"
#include "camera.hpp"
#include "rollback.hpp"
#include "stage.hpp"
#include "title.hpp"
#include "world.hpp"
#include <array>
#include <string>

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
  void DrawFight();
  void SimulateFight();
  std::string contentFingerprint() const;
  Renderer render_;
  Sff sff_;
  AirBank air_;
  Fighter p1_, p2_;
  LuaHost lua_;
  InputSys input_;
  DelayNet delayNet_;
  GgpoPeer ggpo_;
  RollbackStore rbStore_;
  bool ggpoAbort_ = false;
  Camera cam_;
  Hud hud_;
  Stage stage_;
  TitleScreen title_;
  CnsBank cns_;
  enum { kTitle, kFight, kNetWait };
  int screen_ = kTitle;
  bool running_ = true;
  bool training_ = false;
  bool autoHit_ = false;
  int frame_ = 0;
  std::string mode_ = "local";
  std::string connectIp_;
  uint32_t lastLogBits_ = 0xFFFFFFFFu;
  int lastLogState_ = -999;
  int lastLogState2_ = -999;
  std::string gameMode_ = "arcade";
  float p1AiLevel_ = 0.f;
  float p2AiLevel_ = 4.f;
  AiInput p1ai_, p2ai_;
  FightWorld world_;
  std::array<uint32_t, 256> i1hist_{}, i2hist_{};
};
