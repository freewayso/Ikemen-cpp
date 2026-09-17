#include "stage.hpp"
#include "log.hpp"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static std::string trim(std::string s) {
  auto ns = [](int c) { return !std::isspace((unsigned char)c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), ns));
  s.erase(std::find_if(s.rbegin(), s.rend(), ns).base(), s.end());
  return s;
}

bool Stage::Load(const std::string& defPath) {
  std::ifstream f(defPath);
  if (!f) return false;
  std::string dir = defPath;
  auto slash = dir.find_last_of("/\\");
  dir = slash == std::string::npos ? std::string(".") : dir.substr(0, slash);
  std::string sprFile = dir + "/kfm.sff";
  std::string section;
  int curi = -1;
  std::string line;
  auto lowkey = [](std::string s) {
    for (char& ch : s) ch = (char)std::tolower((unsigned char)ch);
    return s;
  };
  bgs.clear();
  while (std::getline(f, line)) {
    auto cmt = line.find(';');
    if (cmt != std::string::npos) line = line.substr(0, cmt);
    line = trim(line);
    if (line.empty()) continue;
    if (line.front() == '[' && line.back() == ']') {
      section = lowkey(line.substr(1, line.size() - 2));
      curi = -1;
      if (section.rfind("bg ", 0) == 0 || section == "bg 0" || section.rfind("bg", 0) == 0) {
        if (section != "bgdef" && section.rfind("begin action", 0) != 0) {
          bgs.push_back({});
          curi = (int)bgs.size() - 1;
        }
      }
      continue;
    }
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string k = lowkey(trim(line.substr(0, eq)));
    std::string v = trim(line.substr(eq + 1));
    if (section == "playerinfo") {
      if (k == "p1startx") p1startx = std::strtof(v.c_str(), nullptr);
      else if (k == "p1starty") p1starty = std::strtof(v.c_str(), nullptr);
      else if (k == "p2startx") p2startx = std::strtof(v.c_str(), nullptr);
      else if (k == "p2starty") p2starty = std::strtof(v.c_str(), nullptr);
      else if (k == "p1facing") p1facing = (int)std::strtol(v.c_str(), nullptr, 10);
      else if (k == "p2facing") p2facing = (int)std::strtol(v.c_str(), nullptr, 10);
      else if (k == "leftbound") leftbound = std::strtof(v.c_str(), nullptr);
      else if (k == "rightbound") rightbound = std::strtof(v.c_str(), nullptr);
    } else if (section == "camera") {
      if (k == "boundleft") boundleft = std::strtof(v.c_str(), nullptr);
      else if (k == "boundright") boundright = std::strtof(v.c_str(), nullptr);
      else if (k == "tension") tension = std::strtof(v.c_str(), nullptr);
    } else if (section == "stageinfo") {
      if (k == "zoffset") zoffset = std::strtof(v.c_str(), nullptr);
    } else if (section == "shadow") {
      if (k == "yscale") shadowYScale = std::strtof(v.c_str(), nullptr);
      else if (k == "intensity") shadowAlpha = std::strtof(v.c_str(), nullptr) / 256.f;
    } else if (section == "bgdef") {
      if (k == "spr") sprFile = dir + "/" + v;
    } else if (curi >= 0 && curi < (int)bgs.size()) {
      StageBG& cur = bgs[(size_t)curi];
      if (k == "spriteno") {
        int g = 0, n = 0;
        sscanf(v.c_str(), "%d,%d", &g, &n);
        cur.group = g;
        cur.number = n;
      } else if (k == "layerno") cur.layer = (int)std::strtol(v.c_str(), nullptr, 10);
      else if (k == "start") sscanf(v.c_str(), "%f,%f", &cur.startx, &cur.starty);
      else if (k == "delta") sscanf(v.c_str(), "%f,%f", &cur.deltax, &cur.deltay);
      else if (k == "tile") sscanf(v.c_str(), "%d,%d", &cur.tilex, &cur.tiley);
      else if (k == "tilespacing") sscanf(v.c_str(), "%f,%f", &cur.spacex, &cur.spacey);
      else if (k == "alpha") {
        float a = 256, d = 0;
        sscanf(v.c_str(), "%f,%f", &a, &d);
        cur.alpha = a / 256.f;
      } else if (k == "trans") {
        std::string t = lowkey(v);
        if (t == "addalpha" || t == "add" || t == "add1") cur.alpha = 0.5f;
      }
    }
  }
  if (!sff.Load(sprFile)) {
    GameLog::Get().Warn("stage sff failed %s", sprFile.c_str());
    return false;
  }
  GameLog::Get().Info("stage loaded %s bgs=%zu zoffset=%.0f start=%.0f,%.0f",
               sprFile.c_str(), bgs.size(), zoffset, p1startx, p2startx);
  return true;
}

void Stage::Draw(Renderer& r, float camx, float camy, int layer) const {
  for (const auto& bg : bgs) {
    if (bg.layer != layer) continue;
    const SpriteImage* s = sff.Get((uint16_t)bg.group, (uint16_t)bg.number);
    if (!s) continue;
    float ax = kGameW * 0.5f + bg.startx - camx * bg.deltax;
    float ay = bg.starty - camy * bg.deltay;
    auto blit = [&](float x, float y) {
      r.DrawSprite(*s, x, y, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, bg.alpha);
    };
    if (bg.tilex) {
      float tw = (float)s->w + bg.spacex;
      if (tw < 1.f) tw = (float)std::max(1, s->w);
      float x = std::fmod(ax, tw);
      if (x > 0) x -= tw;
      int n = 0;
      for (; x < kGameW + tw && n < 64; x += tw, ++n)
        blit(x, ay);
    } else {
      blit(ax, ay);
    }
  }
}
