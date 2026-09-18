#pragma once
#include <cstdint>
#include <string>
#include <vector>

enum class NetPump { Pending, Ready, Fail };

struct IKCPCB;

class DelayNet {
  enum Hs {
    HsIdle, HsConnect, HsMagicRecv, HsJsonRecvLen, HsJsonRecvBody,
    HsSeedRecv, HsTimeAck, HsDone, HsFail
  };
 public:
  bool Listen(int port);
  int TryAccept();
  bool BeginConnect(const std::string& ip, int port);
  void SetRelay(const std::string& ip, int port, const std::string& room);
  NetPump PumpConnect();
  void BeginHandshake(const std::string& fingerprint);
  NetPump PumpHandshake();
  NetPump TryExchange(uint32_t localNow, uint32_t& localPlay, uint32_t& remotePlay);
  void Close();
  bool Active() const { return active_; }
  bool Synced() const { return hs_ == HsDone; }
  bool Listening() const { return listening_; }
  bool Connecting() const { return hs_ == HsConnect; }
  bool IsHost() const { return host_; }
  uint32_t Seed() const { return seed_; }
  int Delay() const { return delay_; }
  const char* WaitLabel() const;
  bool sendRaw(const void* p, int n);

 private:
  void resetBuf();
  void setNonBlock(uintptr_t s);
  bool pumpIo();
  bool queueBytes(const void* p, int n);
  bool have(int n) const { return (int)rx_.size() - rxOff_ >= n; }
  void eat(int n);
  uint32_t peekU32() const;
  bool createKcp();
  bool recvRaw(unsigned char* buf, int maxn, int& n, bool updatePeer);

  uintptr_t sock_ = 0;
  uintptr_t listenSock_ = 0;
  bool listening_ = false;
  bool active_ = false;
  bool host_ = false;
  bool peerOk_ = false;
  bool useTcp_ = true;
  uint32_t seed_ = 1;
  uint32_t loc_[32]{}, rem_[32]{};
  int32_t time_ = 0, locInp_ = 0, remInp_ = 0, locSen_ = 0;
  int delay_ = 2;
  Hs hs_ = HsIdle;
  std::string fingerprint_;
  std::string tx_;
  size_t txOff_ = 0;
  std::vector<unsigned char> rx_;
  int rxOff_ = 0;
  int32_t jsonLen_ = 0;
  int dummyLeft_ = 0;
  bool exchSent_ = false;
  int32_t remSen_ = 0;
  IKCPCB* kcp_ = nullptr;
  unsigned char peer_[128]{};
  int peerLen_ = 0;
  uint32_t lastHelloMs_ = 0;
  bool useRelay_ = false;
  char room_[8]{};
  uint32_t lastKeepMs_ = 0;
  unsigned char relay_[128]{};
  int relayLen_ = 0;
  void sendReg();
  void sendKeep();
  void applyRelayPeer();
};

struct RbPkt {
  int32_t frame = 0;
  uint32_t cksum = 0;
  int16_t bits = 0;
  int8_t axes[6]{};
};

class RollbackNet {
 public:
  void Init(int localPort, int remotePort, const std::string& ip, bool host, int delay);
  void SendFrame(int32_t frame, uint32_t input, uint32_t cksum);
  int Poll(RbPkt* out, int maxn);
  bool ExchangePredicted(uint32_t local, uint32_t& remoteOut, bool& confirmed);
  void Close();
  bool Active() const { return active_; }
  int Delay() const { return delay_; }
 private:
  uintptr_t sock_ = 0;
  bool active_ = false;
  int delay_ = 2;
  uint32_t lastRemote_ = 0;
};
