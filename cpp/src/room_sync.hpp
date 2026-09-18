#pragma once
#include "relay_proto.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct IKCPCB;

class RoomSync {
 public:
  bool Start(bool host, const std::string& ip, int port, const std::string& room);
  void Close();
  void Pump();
  void SendInput(int32_t frame, uint32_t bits);
  bool NextConfirm(uint32_t& p0, uint32_t& p1);
  bool Joined() const { return joined_; }
  bool Active() const { return active_; }
  bool Dropped() const { return dropped_; }
  bool IsHost() const { return host_; }
  uint32_t Seed() const { return seed_; }
  const char* WaitLabel() const;
  int WantFrame() const { return want_; }
  int UdpSend(const char* buf, int len);

 private:
  void sendMsg(const FsMsg& m);
  void onPayload(const unsigned char* p, int n);
  void requestCatchup(int32_t from);
  bool createKcp();

  uintptr_t sock_ = 0;
  IKCPCB* kcp_ = nullptr;
  bool active_ = false;
  bool host_ = false;
  bool joined_ = false;
  bool dropped_ = false;
  uint32_t seed_ = 1;
  char room_[8]{};
  unsigned char peer_[128]{};
  int peerLen_ = 0;
  int32_t want_ = 0;
  int32_t lastSentFrame_ = -1;
  uint32_t lastSentBits_ = 0;
  int32_t serverLatest_ = -1;
  uint32_t lastCatchMs_ = 0;
  uint32_t lastJoinMs_ = 0;
  uint32_t lastInputMs_ = 0;
  std::map<int32_t, std::pair<uint32_t, uint32_t>> ready_;
};
