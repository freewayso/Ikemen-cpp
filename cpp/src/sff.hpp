#pragma once
#include "types.hpp"
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

class Sff {
 public:
  bool Load(const std::string& path);
  bool Load(const std::string& path, const std::vector<uint16_t>& keepGroups);
  const SpriteImage* Get(uint16_t group, uint16_t number) const { return Get(group, number, -1); }
  const SpriteImage* Get(uint16_t group, uint16_t number, int palRemap) const;
  static bool LoadPng(const std::string& path, SpriteImage& out);
 private:
  std::unordered_map<SpriteKey, SpriteImage, SpriteKeyHash> sprites_;
  std::vector<std::vector<uint8_t>> pals_;
  mutable std::unordered_map<uint64_t, std::unique_ptr<SpriteImage>> remapCache_;
};
