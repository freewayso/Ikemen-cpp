#pragma once
#include "types.hpp"
#include <string>
#include <unordered_map>

class AirBank {
 public:
  bool Load(const std::string& path);
  const Animation* Get(int action) const;
 private:
  std::unordered_map<int, Animation> anims_;
};
