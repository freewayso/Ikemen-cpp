#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct LobbyRoomInfo {
  std::string id;
  std::string name;
  std::string host;
  int n = 0;
  int status = 0;
  int maxn = 2;
};

class LobbyClient {
 public:
  bool Connect(const std::string& ip, int port);
  void Close();
  void Pump();
  void SendLogin(const std::string& user, const std::string& pass);
  void SendRegister(const std::string& user, const std::string& pass);
  void SendList();
  void SendCreate();
  void SendJoin(const std::string& id);
  void SendLeave();
  bool Active() const { return sock_ != 0; }
  bool LoggedIn() const { return logged_; }
  bool MatchReady() const { return match_; }
  bool IsHost() const { return hostRole_; }
  const std::string& User() const { return user_; }
  const std::string& RoomId() const { return roomId_; }
  const std::string& Status() const { return status_; }
  const std::vector<LobbyRoomInfo>& Rooms() const { return rooms_; }
  void ClearMatch() { match_ = false; }

 private:
  void sendMsg(int type, const std::string& body);
  void onMsg(int type, const std::string& body);
  uintptr_t sock_ = 0;
  std::string rx_;
  std::string user_;
  std::string roomId_;
  std::string status_ = "CONNECTING";
  std::vector<LobbyRoomInfo> rooms_;
  bool logged_ = false;
  bool match_ = false;
  bool hostRole_ = false;
  std::string pendingUser_;
  std::string pendingPass_;
  bool autoLogin_ = false;
  uint32_t lastListMs_ = 0;
};
