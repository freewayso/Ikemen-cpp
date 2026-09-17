#include "air.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

static std::string trim(std::string s) {
  auto notsp = [](int c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), notsp));
  s.erase(std::find_if(s.rbegin(), s.rend(), notsp).base(), s.end());
  return s;
}

bool AirBank::Load(const std::string& path) {
  std::ifstream f(path);
  if (!f) return false;
  anims_.clear();
  Animation* cur = nullptr;
  int curAct = -1;
  std::vector<Rect> clsn2d, clsn1d, clsn2, clsn1;
  bool def2 = true, def1 = true;
  std::string line, mode;
  int clsnLeft = 0;
  while (std::getline(f, line)) {
    auto cmt = line.find(';');
    if (cmt != std::string::npos) line = line.substr(0, cmt);
    line = trim(line);
    if (line.empty()) continue;
    std::string low = line;
    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    if (low.rfind("[begin action", 0) == 0) {
      int act = 0;
      sscanf(line.c_str(), "[Begin Action %d", &act);
      curAct = act;
      cur = &anims_[act];
      cur->action = act;
      clsn1d.clear(); clsn2d.clear(); clsn1.clear(); clsn2.clear();
      continue;
    }
    if (curAct < 0) continue;
    cur = &anims_[curAct];
    if (low.find("loopstart") != std::string::npos) {
      cur->loopstart = (int)cur->frames.size();
      continue;
    }
    if (low.rfind("clsn", 0) == 0) {
      if (low.find("clsn2default") != std::string::npos) { def2 = true; mode = "2d"; sscanf(low.c_str(), "clsn2default: %d", &clsnLeft); clsn2d.clear(); continue; }
      if (low.find("clsn1default") != std::string::npos) { def1 = true; mode = "1d"; sscanf(low.c_str(), "clsn1default: %d", &clsnLeft); clsn1d.clear(); continue; }
      if (low.find("clsn2:") != std::string::npos) { def2 = false; mode = "2"; sscanf(low.c_str(), "clsn2: %d", &clsnLeft); clsn2.clear(); continue; }
      if (low.find("clsn1:") != std::string::npos) { def1 = false; mode = "1"; sscanf(low.c_str(), "clsn1: %d", &clsnLeft); clsn1.clear(); continue; }
      Rect r{};
      int idx = 0;
      if (sscanf(line.c_str(), "Clsn%*d[%d] = %f,%f,%f,%f", &idx, &r.x0, &r.y0, &r.x1, &r.y1) >= 5 ||
          sscanf(line.c_str(), " Clsn%*d[%d] = %f,%f,%f,%f", &idx, &r.x0, &r.y0, &r.x1, &r.y1) >= 5) {
        if (mode == "2d") clsn2d.push_back(r);
        else if (mode == "1d") clsn1d.push_back(r);
        else if (mode == "2") clsn2.push_back(r);
        else clsn1.push_back(r);
      }
      continue;
    }
    int g = 0, n = 0, x = 0, y = 0, t = 1;
    char extra[32] = {};
    int got = sscanf(line.c_str(), "%d,%d,%d,%d,%d,%31s", &g, &n, &x, &y, &t, extra);
    if (got >= 5 && curAct >= 0) {
      cur = &anims_[curAct];
      AnimFrame fr;
      fr.group = (uint16_t)g; fr.number = (uint16_t)n; fr.x = x; fr.y = y; fr.ticks = t < 1 ? 1 : t;
      if (std::string(extra).find('H') != std::string::npos || std::string(extra).find('h') != std::string::npos) fr.flipH = 1;
      fr.clsn2 = def2 ? clsn2d : clsn2;
      fr.clsn1 = def1 ? clsn1d : clsn1;
      if (!def2) clsn2.clear();
      if (!def1) clsn1.clear();
      cur->frames.push_back(fr);
    }
  }
  return !anims_.empty();
}

const Animation* AirBank::Get(int action) const {
  auto it = anims_.find(action);
  if (it == anims_.end()) return nullptr;
  return &it->second;
}
