#pragma once
#include <cstdarg>
#include <cstdio>
#include <string>

class GameLog {
 public:
  static GameLog& Get();
  bool Open(const std::string& dir, const std::string& mode, bool fight);
  void Close();
  bool FightOn() const { return fight_; }
  unsigned Pid() const { return pid_; }
  const char* Path() const { return path_; }
  void Info(const char* fmt, ...);
  void Warn(const char* fmt, ...);
  void Error(const char* fmt, ...);
  void Fight(const char* fmt, ...);
  void Flush();

 private:
  GameLog() = default;
  void write(const char* lvl, const char* fmt, va_list ap);
  FILE* fp_ = nullptr;
  bool fight_ = false;
  unsigned pid_ = 0;
  int pending_ = 0;
  unsigned lastFlushMs_ = 0;
  char path_[260]{};
};
