#pragma once
#include "fighter.hpp"
#include "world.hpp"
#include <cstdint>
#include <string>
#include <vector>

enum { kGgpoInputSize = 8, kGgpoMaxPred = 8, kGgpoSlots = 10, kGgpoPlayers = 2 };

struct GgpoEvent {
  enum Code {
    Connected, Syncing, Synced, Running, Disconnect, TimeSync, Desync
  } code = Connected;
  int count = 0, total = 0, player = 0;
  float framesAhead = 0;
};

class GgpoSession {
 public:
  virtual ~GgpoSession() = default;
  virtual int SaveGameState(int slot) = 0;
  virtual void LoadGameState(int slot) = 0;
  virtual void AdvanceFrame(int flags) = 0;
  virtual void OnEvent(const GgpoEvent& ev) = 0;
};

class RollbackStore {
 public:
  void SaveSlot(int slot, int frame, const FighterSnapshot& a, const FighterSnapshot& b,
                const FightWorld& world);
  bool LoadSlot(int slot, int& frame, FighterSnapshot& a, FighterSnapshot& b, FightWorld& world);
  uint32_t Checksum(const FighterSnapshot& a, const FighterSnapshot& b) const;
  void Save(int frame, const FighterSnapshot& a, const FighterSnapshot& b);
  bool Load(int frame, FighterSnapshot& a, FighterSnapshot& b);
  void Trim(int keepFrom);
 private:
  struct Slot {
    int frame = -1;
    FighterSnapshot a, b;
    FightWorld world;
  };
  Slot indexed_[kGgpoSlots];
  std::vector<Slot> slots_;
};

// Wire + sync layer aligned with github.com/ikemen-engine/ggpo (binary ToBytes, not gob).
class GgpoPeer {
 public:
  bool Init(GgpoSession* cb, int localPort, int remotePort, const std::string& ip,
            bool host, int frameDelay);
  void Idle();
  bool AddLocalInput(const uint8_t* bytes, int n);
  bool SyncInput(uint8_t out[kGgpoPlayers][kGgpoInputSize]);
  void AdvanceFrame(uint32_t checksum);
  void Close();
  bool Active() const { return sock_ != 0; }
  bool Running() const { return running_; }
  bool Synced() const { return synchronized_; }
  bool IsHost() const { return host_; }
  int Frame() const { return frameCount_; }
  int LocalPlayer() const { return local_; }
  int Delay() const { return delay_; }
  const char* WaitLabel() const;
 private:
  enum {
    TInvalid = 0, TSyncReq = 1, TSyncRep = 2, TInput = 3,
    TQuality = 4, TQualityRep = 5, TKeep = 6, TInputAck = 7
  };
  struct GameIn {
    int frame = -1;
    int size = kGgpoInputSize;
    uint8_t bits[kGgpoInputSize]{};
    uint32_t checksum = 0;
  };
  struct InQueue {
    int delay = 0;
    int lastUser = -1;
    int lastAdded = -1;
    int firstIncorrect = -1;
    int lastRequested = -1;
    bool first = true;
    int head = 0, tail = 0, length = 0;
    GameIn inputs[128];
    GameIn prediction;
    void setDelay(int d) { delay = d; }
    bool add(GameIn in);
    bool get(int frame, GameIn& out, bool& predicted);
    int firstBad() const { return firstIncorrect; }
    void resetPred(int frame);
    void discard(int frame);
  };
  struct Saved {
    int frame = -1;
    int checksum = 0;
  };

  void sendRaw(const uint8_t* p, int n);
  void sendHdr(uint8_t type, const uint8_t* body, int bodyN);
  void sendSyncReq();
  void sendSyncRep(uint32_t rand);
  void sendInputAck(int32_t ack);
  void sendPendingInput();
  void sendQuality();
  void recvAll();
  void onPacket(const uint8_t* p, int n);
  void onInputPkt(const uint8_t* p, int n);
  void checkRollback();
  void saveCurrent();
  bool loadFrame(int frame);
  void setHdr(uint8_t* b, uint8_t type);

  GgpoSession* cb_ = nullptr;
  uintptr_t sock_ = 0;
  unsigned char peer_[128]{};
  int peerLen_ = 0;
  bool host_ = false;
  bool running_ = false;
  bool synchronized_ = false;
  bool rolling_ = false;
  int local_ = 0;
  int delay_ = 2;
  int frameCount_ = 0;
  int lastConfirmed_ = -1;
  uint16_t magic_ = 1, remoteMagic_ = 0, sendSeq_ = 0;
  uint32_t syncRand_ = 0;
  int syncLeft_ = 5;
  uint32_t lastSendMs_ = 0, lastRecvMs_ = 0, lastQualityMs_ = 0;
  int remoteFrameAdv_ = 0;
  InQueue q_[kGgpoPlayers];
  GameIn pending_[64];
  int pendingN_ = 0;
  int lastRecvIn_ = -1;
  int lastAcked_ = -1;
  Saved saved_[kGgpoSlots];
  int saveHead_ = 0;
  uint32_t pendingCk_[128]{};
  bool pendingCkOk_[128]{};
  uint32_t remoteCk_[128]{};
  bool remoteCkOk_[128]{};
  int32_t localLast_[4]{-1, -1, -1, -1};
  int32_t peerLast_[4]{-1, -1, -1, -1};
};
