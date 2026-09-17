#pragma once
#include "types.hpp"
#include <string>
#include <vector>
#include <deque>

struct CmdDef {
  std::string name;
  std::vector<std::string> symbols;
  int time = 15;
};

class CmdMatcher {
 public:
  void LoadSimple(const std::vector<CmdDef>& defs);
  bool LoadFile(const std::string& path);
  void Push(uint32_t bits, int facing);
  bool Was(const std::string& name) const;
  std::vector<std::string> Fired() const;
  void Cheat(float aiLevel);
  int Count() const { return (int)defs_.size(); }
  void ClearHeld();
 private:
  std::vector<CmdDef> defs_;
  std::deque<uint32_t> hist_;
  std::vector<int> fire_;
};
