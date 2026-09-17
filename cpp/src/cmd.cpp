#include "cmd.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

struct ParsedSym {
  uint32_t need = 0;
  bool hold = false;
  bool rel = false;
  bool button = false;
};

static uint32_t dirBit(const std::string& t, int facing) {
  auto F = facing >= 0 ? kInputR : kInputL;
  auto B = facing >= 0 ? kInputL : kInputR;
  if (t == "U") return kInputU;
  if (t == "D") return kInputD;
  if (t == "F") return F;
  if (t == "B") return B;
  if (t == "UF") return kInputU | F;
  if (t == "UB") return kInputU | B;
  if (t == "DF") return kInputD | F;
  if (t == "DB") return kInputD | B;
  return 0;
}

static uint32_t btnBit(const std::string& t) {
  if (t == "a") return kInputA;
  if (t == "b") return kInputB;
  if (t == "c") return kInputC;
  if (t == "x") return kInputX;
  if (t == "y") return kInputY;
  if (t == "z") return kInputZ;
  if (t == "s") return kInputS;
  return 0;
}

static ParsedSym parseSym(std::string s, int facing) {
  ParsedSym o;
  auto trim = [](std::string x) {
    while (!x.empty() && isspace((unsigned char)x.front())) x.erase(x.begin());
    while (!x.empty() && isspace((unsigned char)x.back())) x.pop_back();
    return x;
  };
  s = trim(s);
  while (!s.empty() && (s[0] == '/' || s[0] == '~' || s[0] == '>' || s[0] == '$')) {
    if (s[0] == '/') o.hold = true;
    if (s[0] == '~') o.rel = true;
    s.erase(s.begin());
    while (!s.empty() && isdigit((unsigned char)s[0])) s.erase(s.begin());
  }
  s = trim(s);
  uint32_t need = 0;
  bool anyBtn = false;
  std::string tok;
  for (size_t i = 0; i <= s.size(); i++) {
    if (i == s.size() || s[i] == '+') {
      if (!tok.empty()) {
        uint32_t d = dirBit(tok, facing);
        uint32_t b = btnBit(tok);
        if (d) need |= d;
        if (b) { need |= b; anyBtn = true; }
        tok.clear();
      }
    } else tok += s[i];
  }
  o.need = need;
  o.button = anyBtn;
  return o;
}

static bool onBits(uint32_t bits, uint32_t need) {
  return need != 0 && (bits & need) == need;
}

void CmdMatcher::LoadSimple(const std::vector<CmdDef>& defs) {
  defs_ = defs;
  fire_.assign(defs_.size(), 0);
}

bool CmdMatcher::LoadFile(const std::string& path) {
  std::ifstream f(path);
  if (!f) return false;
  defs_.clear();
  CmdDef cur;
  std::string line, sec;
  int defTime = 15;
  auto trim = [](std::string s) {
    while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
    return s;
  };
  while (std::getline(f, line)) {
    auto c = line.find(';');
    if (c != std::string::npos) line = line.substr(0, c);
    line = trim(line);
    if (line.empty()) continue;
    if (line.find('[') != std::string::npos) {
      if (!cur.name.empty()) defs_.push_back(cur);
      cur = {};
      cur.time = defTime;
      sec = line;
      continue;
    }
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
    if (sec.find("Defaults") != std::string::npos && k == "command.time") {
      defTime = std::max(1, atoi(v.c_str()));
      continue;
    }
    if (sec.find("Command") == std::string::npos) continue;
    if (k == "name") {
      if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
      cur.name = v;
    } else if (k == "command") {
      std::stringstream ss(v);
      std::string tok;
      while (std::getline(ss, tok, ',')) cur.symbols.push_back(trim(tok));
    } else if (k == "time") cur.time = std::max(1, atoi(v.c_str()));
  }
  if (!cur.name.empty()) defs_.push_back(cur);
  fire_.assign(defs_.size(), 0);
  return !defs_.empty();
}

void CmdMatcher::Push(uint32_t bits, int facing) {
  uint32_t prev = hist_.empty() ? 0 : hist_.back();
  hist_.push_back(bits);
  if ((int)hist_.size() > 64) hist_.pop_front();
  for (size_t i = 0; i < defs_.size(); i++) {
    fire_[i] = 0;
    const auto& d = defs_[i];
    if (d.symbols.empty()) continue;
    std::vector<ParsedSym> sy;
    sy.reserve(d.symbols.size());
    bool okParse = true;
    for (auto& s : d.symbols) {
      auto p = parseSym(s, facing);
      if (p.need == 0) { okParse = false; break; }
      sy.push_back(p);
    }
    if (!okParse) continue;

    bool allHold = true;
    for (auto& p : sy) if (!p.hold) allHold = false;
    if (allHold) {
      bool ok = true;
      for (auto& p : sy) if (!onBits(bits, p.need)) ok = false;
      fire_[i] = ok ? 1 : 0;
      continue;
    }

    int hi = (int)hist_.size() - 1;
    int matched = (int)sy.size() - 1;
    int span = 0;
    uint32_t newer = bits;
    while (hi >= 0 && matched >= 0 && span <= d.time + 8) {
      uint32_t cur = hist_[hi];
      uint32_t older = hi > 0 ? hist_[hi - 1] : 0;
      const ParsedSym& p = sy[matched];
      bool hit = false;
      if (p.hold) {
        if (hi == (int)hist_.size() - 1 && onBits(cur, p.need)) hit = true;
      } else if (p.rel) {
        if (onBits(older, p.need) && !onBits(cur, p.need)) hit = true;
      } else if (p.button) {
        if (onBits(cur, p.need) && !onBits(older, p.need)) hit = true;
      } else {
        bool dup = false;
        for (int j = 0; j < (int)sy.size(); j++)
          if (j != matched && sy[j].need == p.need && !sy[j].hold && !sy[j].button) dup = true;
        if (dup) {
          if (onBits(cur, p.need) && !onBits(older, p.need)) hit = true;
        } else if (onBits(cur, p.need)) hit = true;
      }
      if (hit) {
        matched--;
        newer = cur;
      }
      hi--;
      span++;
      (void)newer;
    }
    if (matched < 0) fire_[i] = 1;
  }
  (void)prev;
}

bool CmdMatcher::Was(const std::string& name) const {
  for (size_t i = 0; i < defs_.size(); i++)
    if (defs_[i].name == name && fire_[i]) return true;
  return false;
}

std::vector<std::string> CmdMatcher::Fired() const {
  std::vector<std::string> o;
  for (size_t i = 0; i < defs_.size(); i++)
    if (fire_[i]) o.push_back(defs_[i].name);
  return o;
}

void CmdMatcher::Cheat(float aiLevel) {
  // Go: RandF32(0, aiLevel/2+32)>32 then cheat = Rand(0, len(Commands)-1);
  // command() returns true only if that index is multi-step or multi-key.
  if (defs_.empty()) return;
  float span = aiLevel * 0.5f + 32.f;
  float r = (float)rand() / (float)RAND_MAX * span;
  if (r <= 32.f) return;
  int i = rand() % (int)defs_.size();
  const auto& d = defs_[i];
  bool multiStep = (int)d.symbols.size() > 1;
  bool multiKey = !d.symbols.empty() && d.symbols[0].find('+') != std::string::npos;
  if (multiStep || multiKey) fire_[i] = 1;
}

void CmdMatcher::ClearHeld() {}
