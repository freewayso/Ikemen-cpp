#include "cns.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>

static std::string trim(std::string s) {
  while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
  while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
  return s;
}
static std::string lower(std::string s) {
  for (char& c : s) c = (char)tolower((unsigned char)c);
  return s;
}
static std::string unquote(std::string s) {
  s = trim(s);
  if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')))
    return s.substr(1, s.size() - 2);
  return s;
}
static const char* skipsp(const char* p) {
  while (*p && isspace((unsigned char)*p)) p++;
  return p;
}

static bool parseSectionState(const std::string& sec, int& no, bool& isDef) {
  std::string s = lower(sec);
  isDef = false;
  if (s.rfind("[statedef", 0) == 0) {
    isDef = true;
    return sscanf(sec.c_str(), "%*[^0-9-]%d", &no) == 1 || sscanf(sec.c_str(), "[Statedef %d", &no) == 1
        || sscanf(lower(sec).c_str(), "[statedef %d", &no) == 1;
  }
  if (s.rfind("[state", 0) == 0) {
    return sscanf(lower(sec).c_str(), "[state %d", &no) == 1;
  }
  return false;
}

static std::vector<std::string> splitTopComma(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  int depth = 0;
  for (char ch : s) {
    if (ch == '(') depth++;
    else if (ch == ')') depth--;
    if (ch == ',' && depth == 0) {
      out.push_back(trim(cur));
      cur.clear();
    } else cur += ch;
  }
  std::string t = trim(cur);
  if (!t.empty() || !out.empty()) out.push_back(t);
  return out;
}

static int parseAnimType(const std::string& v) {
  std::string u = lower(v);
  if (u.find("diagup") != std::string::npos) return 5;
  if (u.find("up") != std::string::npos) return 4;
  if (u.find("back") != std::string::npos) return 3;
  if (u.find("hard") != std::string::npos) return 2;
  if (u.find("medium") != std::string::npos || u == "med") return 1;
  return 0;
}

static int parseHitType(const std::string& v) {
  std::string u = lower(v);
  if (u.find("trip") != std::string::npos) return 3;
  if (u.find("low") != std::string::npos) return 2;
  return 1;
}

static float parsePairFirst(const std::string& v, float def = 0) {
  float a = def, b = 0;
  if (sscanf(v.c_str(), "%f , %f", &a, &b) >= 1 || sscanf(v.c_str(), "%f,%f", &a, &b) >= 1 || sscanf(v.c_str(), "%f", &a) == 1)
    return a;
  return def;
}
static float parsePairSecond(const std::string& v, float def = 0) {
  float a = 0, b = def;
  if (sscanf(v.c_str(), "%f , %f", &a, &b) == 2 || sscanf(v.c_str(), "%f,%f", &a, &b) == 2) return b;
  return def;
}

bool CnsBank::LoadFile(const std::string& path) {
  std::ifstream f(path);
  if (!f) return false;
  std::string line;
  CnsStateDef* def = nullptr;
  CnsCtrl* ctrl = nullptr;
  while (std::getline(f, line)) {
    auto c = line.find(';');
    if (c != std::string::npos) line = line.substr(0, c);
    line = trim(line);
    if (line.empty()) continue;
    if (line.front() == '[') {
      int no = 0;
      bool isDef = false;
      if (!parseSectionState(line, no, isDef)) {
        def = nullptr;
        ctrl = nullptr;
        continue;
      }
      if (isDef) {
        def = &defs_[no];
        def->no = no;
        ctrl = nullptr;
      } else {
        if (!def || def->no != no) def = &defs_[no];
        def->no = no;
        def->ctrls.push_back({});
        ctrl = &def->ctrls.back();
      }
      continue;
    }
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string k = lower(trim(line.substr(0, eq)));
    std::string v = trim(line.substr(eq + 1));
    if (k.rfind("trigger", 0) == 0 && def && ctrl) {
      if (k == "triggerall") ctrl->triggerall.push_back(v);
      else {
        int n = atoi(k.c_str() + 7);
        if (n < 1) n = 1;
        if ((int)ctrl->triggers.size() < n) ctrl->triggers.resize(n);
        ctrl->triggers[n - 1].push_back(v);
      }
      continue;
    }
    if (def && !ctrl) {
      if (k == "type" && !v.empty()) def->type = (char)toupper((unsigned char)v[0]);
      else if (k == "movetype" && !v.empty()) def->movetype = (char)toupper((unsigned char)v[0]);
      else if (k == "physics" && !v.empty()) def->physics = (char)toupper((unsigned char)v[0]);
      else if (k == "anim") def->anim = atoi(v.c_str());
      else if (k == "ctrl") def->ctrl = atoi(v.c_str());
      else if (k == "poweradd") def->poweradd = atoi(v.c_str());
      else if (k == "velset") {
        def->velx = parsePairFirst(v, 0);
        def->vely = parsePairSecond(v, 0);
        if (v.find(',') == std::string::npos) def->vely = 1e9f;
      }
    } else if (ctrl) {
      if (k == "type") ctrl->type = lower(v);
      else if (k == "persistent") ctrl->persistent = atoi(v.c_str());
      else if (k == "ignorehitpause") ctrl->ignorehitpause = atoi(v.c_str());
      else ctrl->params.push_back({k, v});
    } else {
      if (k == "life") life = atoi(v.c_str());
      else if (k == "attack") attackBase = atoi(v.c_str());
      else if (k == "defence") defenceBase = atoi(v.c_str());
      else if (k == "walk.fwd") walkFwd = (float)atof(v.c_str());
      else if (k == "walk.back") walkBack = (float)atof(v.c_str());
      else if (k == "run.fwd") { runFwd = parsePairFirst(v, 4.6f); }
      else if (k == "run.back") { runBackX = parsePairFirst(v, -4.5f); runBackY = parsePairSecond(v, -3.8f); }
      else if (k == "jump.fwd") jumpFwd = parsePairFirst(v, 2.5f);
      else if (k == "jump.back") jumpBack = parsePairFirst(v, -2.55f);
      else if (k == "yaccel") yaccel = (float)atof(v.c_str());
      else if (k == "stand.friction") standFric = (float)atof(v.c_str());
      else if (k == "crouch.friction") crouchFric = (float)atof(v.c_str());
      else if (k == "ground.front") wFront = atoi(v.c_str());
      else if (k == "ground.back") wBack = atoi(v.c_str());
    }
  }
  return !defs_.empty();
}

static void zssSplitKV(const std::string& body, std::vector<std::pair<std::string, std::string>>& kv) {
  std::string cur;
  int depth = 0;
  for (size_t i = 0; i <= body.size(); i++) {
    char ch = i < body.size() ? body[i] : ';';
    if (ch == '(') depth++;
    else if (ch == ')') depth--;
    if ((ch == ';' || i == body.size()) && depth == 0) {
      auto eq = cur.find(':');
      if (eq != std::string::npos)
        kv.push_back({lower(trim(cur.substr(0, eq))), trim(cur.substr(eq + 1))});
      cur.clear();
    } else cur += ch;
  }
}

bool CnsBank::LoadZss(const std::string& path) {
  std::ifstream f(path);
  if (!f) return false;
  std::string all, line;
  while (std::getline(f, line)) {
    auto h = line.find('#');
    if (h != std::string::npos) line = line.substr(0, h);
    all += line;
    all += '\n';
  }
  CnsStateDef* def = nullptr;
  std::vector<CnsCtrl>* out = nullptr;
  size_t i = 0;
  auto skipSp = [&]() {
    while (i < all.size() && isspace((unsigned char)all[i])) i++;
  };
  auto pushCtrl = [&](CnsCtrl c) {
    if (out) out->push_back(std::move(c));
    else if (def) def->ctrls.push_back(std::move(c));
  };
  while (i < all.size()) {
    skipSp();
    if (i >= all.size()) break;
    if (all.compare(i, 9, "[StateDef") == 0 || all.compare(i, 9, "[statedef") == 0) {
      auto end = all.find(']', i);
      if (end == std::string::npos) break;
      std::string sec = all.substr(i + 1, end - i - 1);
      i = end + 1;
      int no = 0;
      sscanf(sec.c_str(), "%*[^0-9-]%d", &no);
      def = &defs_[no];
      def->no = no;
      out = &def->ctrls;
      auto sc = sec.find(';');
      if (sc != std::string::npos) {
        std::vector<std::pair<std::string, std::string>> kv;
        zssSplitKV(sec.substr(sc + 1), kv);
        for (auto& p : kv) {
          if (p.first == "type" && !p.second.empty()) def->type = (char)toupper((unsigned char)p.second[0]);
          else if (p.first == "movetype" && !p.second.empty()) def->movetype = (char)toupper((unsigned char)p.second[0]);
          else if (p.first == "physics" && !p.second.empty()) def->physics = (char)toupper((unsigned char)p.second[0]);
          else if (p.first == "anim") def->anim = atoi(p.second.c_str());
          else if (p.first == "ctrl") def->ctrl = atoi(p.second.c_str());
          else if (p.first == "velset") {
            def->velx = parsePairFirst(p.second, 0);
            def->vely = parsePairSecond(p.second, 0);
            if (p.second.find(',') == std::string::npos) def->vely = 1e9f;
          }
        }
      }
      continue;
    }
    if (all.compare(i, 9, "[Function") == 0) {
      auto end = all.find(']', i);
      if (end == std::string::npos) break;
      std::string sec = all.substr(i, end - i + 1);
      i = end + 1;
      std::string inner = sec;
      if (!inner.empty() && inner.front() == '[') inner.erase(inner.begin());
      if (!inner.empty() && inner.back() == ']') inner.pop_back();
      auto p0 = inner.find(' ');
      auto p1 = inner.find('(');
      auto p2 = inner.rfind(')');
      std::string name = "fn";
      if (p0 != std::string::npos)
        name = trim(inner.substr(p0 + 1, (p1 == std::string::npos ? inner.size() : p1) - p0 - 1));
      def = nullptr;
      out = &funcs_[lower(name)];
      if (p1 != std::string::npos && p2 > p1)
        funcParams_[lower(name)] = splitTopComma(inner.substr(p1 + 1, p2 - p1 - 1));
      continue;
    }
    if (!out) { i++; continue; }
    bool ignHp = false;
    int pers = 1;
    if (all.compare(i, 15, "ignoreHitPause") == 0 || all.compare(i, 15, "ignorehitpause") == 0) {
      ignHp = true;
      i += 15;
      skipSp();
    }
    if (all.compare(i, 11, "persistent(") == 0) {
      i += 11;
      pers = atoi(all.c_str() + i);
      while (i < all.size() && all[i] != ')') i++;
      if (i < all.size()) i++;
      skipSp();
    }
    if (all.compare(i, 2, "if") == 0 && (i + 2 >= all.size() || isspace((unsigned char)all[i + 2]) || all[i + 2] == '(')) {
      i += 2;
      skipSp();
      auto brace = all.find('{', i);
      if (brace == std::string::npos) break;
      std::string expr = trim(all.substr(i, brace - i));
      i = brace + 1;
      int depth = 1;
      size_t body0 = i;
      while (i < all.size() && depth) {
        if (all[i] == '{') depth++;
        else if (all[i] == '}') depth--;
        i++;
      }
      std::string body = all.substr(body0, i - body0 - 1);
      std::function<void(std::vector<CnsCtrl>&, const std::string&, const std::string&)> addCtrl;
      std::function<void(std::vector<CnsCtrl>&, const std::string&, const std::string&)> parseStmts;
      addCtrl = [&](std::vector<CnsCtrl>& dest, const std::string& trig, const std::string& stmt0) {
        std::string stmt = trim(stmt0);
        if (stmt.rfind("call ", 0) == 0 || stmt.rfind("call\t", 0) == 0) {
          CnsCtrl c;
          c.type = "call";
          c.ignorehitpause = ignHp ? 1 : 0;
          c.persistent = pers;
          c.triggers.push_back({trig});
          auto par = stmt.find('(');
          std::string nm = trim(stmt.substr(4, par == std::string::npos ? std::string::npos : par - 4));
          c.params.push_back({"name", nm});
          if (par != std::string::npos) {
            auto rp = stmt.rfind(')');
            std::string inside = (rp != std::string::npos && rp > par) ? stmt.substr(par + 1, rp - par - 1) : "";
            auto args = splitTopComma(inside);
            for (size_t ai = 0; ai < args.size(); ai++)
              c.params.push_back({"arg" + std::to_string((int)ai), args[ai]});
          }
          dest.push_back(std::move(c));
          return;
        }
        if (stmt.rfind("let ", 0) == 0) {
          CnsCtrl c;
          c.type = "let";
          c.triggers.push_back({trig});
          auto eq = stmt.find('=');
          std::string nm = trim(stmt.substr(4, eq == std::string::npos ? std::string::npos : eq - 4));
          std::string val = eq == std::string::npos ? "0" : trim(stmt.substr(eq + 1));
          if (!val.empty() && val.back() == ';') val.pop_back();
          c.params.push_back({nm, val});
          dest.push_back(std::move(c));
          return;
        }
        auto lb = stmt.find('{');
        if (lb == std::string::npos) return;
        std::string ty = trim(stmt.substr(0, lb));
        auto rb = stmt.rfind('}');
        std::string inside = rb == std::string::npos ? "" : stmt.substr(lb + 1, rb - lb - 1);
        CnsCtrl c;
        c.type = lower(ty);
        c.ignorehitpause = ignHp ? 1 : 0;
        c.persistent = pers;
        c.triggers.push_back({trig});
        std::vector<std::pair<std::string, std::string>> kv;
        zssSplitKV(inside, kv);
        for (auto& p : kv) c.params.push_back({p.first, p.second});
        dest.push_back(std::move(c));
      };
      parseStmts = [&](std::vector<CnsCtrl>& dest, const std::string& trig, const std::string& body) {
        size_t b = 0;
        while (b < body.size()) {
          while (b < body.size() && isspace((unsigned char)body[b])) b++;
          if (b >= body.size()) break;
          if (body.compare(b, 2, "if") == 0) break;
          if (body.compare(b, 4, "for ") == 0 || body.compare(b, 4, "for(") == 0) {
            auto lb = body.find('{', b);
            if (lb == std::string::npos) break;
            std::string hdr = trim(body.substr(b + 3, lb - (b + 3)));
            int d = 1;
            size_t e = lb + 1;
            while (e < body.size() && d) {
              if (body[e] == '{') d++;
              else if (body[e] == '}') d--;
              e++;
            }
            CnsCtrl fc;
            fc.type = "for";
            fc.triggers.push_back({trig});
            fc.ignorehitpause = ignHp ? 1 : 0;
            fc.persistent = pers;
            auto eq = hdr.find('=');
            auto s1 = hdr.find(';');
            auto s2 = s1 == std::string::npos ? std::string::npos : hdr.find(';', s1 + 1);
            std::string var = "i", start = "0", limit = "0", step = "1";
            if (eq != std::string::npos && s1 != std::string::npos) {
              var = trim(hdr.substr(0, eq));
              if (var.rfind("for", 0) == 0) var = trim(var.substr(3));
              start = trim(hdr.substr(eq + 1, s1 - eq - 1));
              if (s2 != std::string::npos) {
                limit = trim(hdr.substr(s1 + 1, s2 - s1 - 1));
                step = trim(hdr.substr(s2 + 1));
              } else limit = trim(hdr.substr(s1 + 1));
            }
            fc.params.push_back({"var", var});
            fc.params.push_back({"start", start});
            fc.params.push_back({"limit", limit});
            fc.params.push_back({"step", step});
            parseStmts(fc.children, trig, body.substr(lb + 1, e - lb - 2));
            dest.push_back(std::move(fc));
            b = e;
            continue;
          }
          if (body.compare(b, 4, "let ") == 0 || body.compare(b, 5, "call ") == 0) {
            auto sc = body.find(';', b);
            if (sc == std::string::npos) break;
            addCtrl(dest, trig, body.substr(b, sc + 1 - b));
            b = sc + 1;
            continue;
          }
          auto lb = body.find('{', b);
          if (lb == std::string::npos) break;
          int d = 1;
          size_t e = lb + 1;
          while (e < body.size() && d) {
            if (body[e] == '{') d++;
            else if (body[e] == '}') d--;
            e++;
          }
          addCtrl(dest, trig, body.substr(b, e - b));
          b = e;
        }
      };
      parseStmts(*out, expr, body);
      continue;
    }
    if (all.compare(i, 5, "call ") == 0 || all.compare(i, 5, "call\t") == 0) {
      auto sc = all.find(';', i);
      if (sc == std::string::npos) break;
      CnsCtrl c;
      c.type = "call";
      c.ignorehitpause = ignHp ? 1 : 0;
      c.persistent = pers;
      c.triggers.push_back({"1"});
      std::string stmt = all.substr(i, sc + 1 - i);
      auto par = stmt.find('(');
      std::string nm = trim(stmt.substr(4, par == std::string::npos ? std::string::npos : par - 4));
      if (!nm.empty() && nm.back() == ';') nm.pop_back();
      c.params.push_back({"name", nm});
      if (par != std::string::npos) {
        auto rp = stmt.rfind(')');
        std::string inside = (rp != std::string::npos && rp > par) ? stmt.substr(par + 1, rp - par - 1) : "";
        auto args = splitTopComma(inside);
        for (size_t ai = 0; ai < args.size(); ai++)
          c.params.push_back({"arg" + std::to_string((int)ai), args[ai]});
      }
      if (out) out->push_back(std::move(c));
      i = sc + 1;
      continue;
    }
    i++;
  }
  return true;
}

void CnsBank::ActionPrepare(Fighter& f) {
  // Port of src/char.go Char.actionPrepare hardcoded basic actions
  if (f.helperIndex != 0) return;
  if (!f.snap.ctrl) {
    uint32_t in = f.snap.input | f.snap.assertInput;
    bool fwd = f.cmd.Was("holdfwd") || ((f.snap.facing >= 0) ? (in & kInputR) : (in & kInputL));
    bool back = f.cmd.Was("holdback") || ((f.snap.facing >= 0) ? (in & kInputL) : (in & kInputR));
    if (!(f.snap.asf & ASF_nowalk) && f.snap.state == 20 && fwd == back) Enter(f, 0, 1);
    return;
  }
  uint32_t in = f.snap.input | f.snap.assertInput;
  bool up = f.cmd.Was("holdup") || (in & kInputU);
  bool down = f.cmd.Was("holddown") || (in & kInputD);
  bool fwd = f.cmd.Was("holdfwd") || ((f.snap.facing >= 0) ? (in & kInputR) : (in & kInputL));
  bool back = f.cmd.Was("holdback") || ((f.snap.facing >= 0) ? (in & kInputL) : (in & kInputR));
  if (f.snap.stateType == 'S' && up && f.snap.state != 40) Enter(f, 40);
  else if (f.snap.stateType == 'S' && down && f.snap.state != 10) {
    if (f.snap.state != 100) f.snap.vel.x = 0;
    Enter(f, 10);
  } else if (f.snap.stateType == 'C' && !down && f.snap.state != 12) Enter(f, 12);
  else if (f.snap.stateType == 'S' && (fwd != back) && !(f.snap.asf & ASF_nowalk) && f.snap.state != 20)
    Enter(f, 20);
  if (!(f.snap.asf & ASF_nowalk) && f.snap.state == 20 && fwd == back) Enter(f, 0, 1);
}

const CnsStateDef* CnsBank::Get(int no) const {
  auto it = defs_.find(no);
  return it == defs_.end() ? nullptr : &it->second;
}

void CnsBank::Enter(Fighter& f, int no, int ctrlOverride) {
  f.snap.prevState = f.snap.state;
  f.snap.state = no;
  f.snap.time = 0;
  f.snap.moveContact = 0;
  f.snap.hitOnce = 0;
  f.snap.hit.on = false;
  auto it = defs_.find(no);
  if (it != defs_.end()) {
    const auto& d = it->second;
    f.snap.stateType = d.type ? d.type : 'S';
    f.snap.moveType = d.movetype ? d.movetype : 'I';
    f.snap.physics = d.physics ? d.physics : 'S';
    if (d.anim >= 0) f.SetAnim(d.anim);
    else f.SetAnim(no);
    if (d.velx < 1e8f) f.snap.vel.x = d.velx;
    if (d.vely < 1e8f) f.snap.vel.y = d.vely;
    if (ctrlOverride >= 0) f.snap.ctrl = ctrlOverride;
    else if (d.ctrl >= 0) f.snap.ctrl = d.ctrl;
    else f.snap.ctrl = 0;
    f.snap.power = std::clamp(f.snap.power + d.poweradd, 0, f.snap.powerMax);
  } else {
    f.snap.ctrl = ctrlOverride >= 0 ? ctrlOverride : 0;
    if (no == 0) { f.snap.stateType = 'S'; f.snap.moveType = 'I'; f.snap.physics = 'S'; f.snap.ctrl = 1; f.SetAnim(0); }
    else if (no == 10 || no == 11) { f.snap.stateType = 'C'; f.snap.moveType = 'I'; f.snap.physics = 'C'; f.SetAnim(no); f.snap.ctrl = 1; }
    else if (no == 12) { f.snap.stateType = 'S'; f.snap.physics = 'S'; f.SetAnim(12); }
    else if (no == 20) { f.snap.stateType = 'S'; f.snap.physics = 'S'; f.snap.ctrl = 1; f.SetAnim(20); }
    else if (no == 100) {
      f.snap.stateType = 'S'; f.snap.moveType = 'I'; f.snap.physics = 'S';
      f.snap.ctrl = 1;
      f.SetAnim(100);
      f.snap.vel.x = runFwd;
    }
    else if (no == 105) {
      f.snap.stateType = 'A'; f.snap.moveType = 'I'; f.snap.physics = 'A';
      f.snap.ctrl = 0;
      f.SetAnim(105);
      f.snap.vel.x = runBackX;
      f.snap.vel.y = runBackY;
    }
    else if (no == 106) {
      f.snap.stateType = 'S'; f.snap.physics = 'S'; f.snap.ctrl = 0;
      f.SetAnim(47);
      f.snap.vel.y = 0;
    }
    else if (no == 40) { f.snap.stateType = 'S'; f.snap.physics = 'S'; f.SetAnim(40); f.snap.ctrl = 0; f.snap.sysVar1 = 0; }
    else if (no == 50) { f.snap.stateType = 'A'; f.snap.physics = 'A'; f.SetAnim(41); }
    else if (no == 52) { f.snap.stateType = 'S'; f.snap.physics = 'S'; f.SetAnim(47); f.snap.vel = {0, 0}; }
    else if (no == 120 || no == 130 || no == 131) { f.snap.stateType = no == 131 ? 'C' : 'S'; f.snap.physics = f.snap.stateType; f.SetAnim(no); f.snap.ctrl = 1; }
    else if (no == 5000) { f.snap.stateType = 'S'; f.snap.moveType = 'H'; f.snap.physics = 'S'; f.SetAnim(5000); f.snap.ctrl = 0; }
    else if (no == 5030) { f.snap.stateType = 'A'; f.snap.moveType = 'H'; f.snap.physics = 'A'; f.SetAnim(5030); f.snap.ctrl = 0; }
    else if (no == 5110) { f.snap.stateType = 'L'; f.snap.moveType = 'H'; f.snap.physics = 'N'; f.SetAnim(5110); f.snap.vel = {0, 0}; }
    else if (no == 5120) { f.snap.stateType = 'L'; f.snap.physics = 'S'; f.SetAnim(5120); }
    else f.SetAnim(no);
  }
}

static char stType(const Fighter& f) { return f.snap.stateType; }

static int splitTop(const std::string& e, const char* op, std::string& L, std::string& R) {
  int depth = 0;
  size_t opl = strlen(op);
  for (size_t i = 0; i + opl <= e.size(); i++) {
    if (e[i] == '(') depth++;
    else if (e[i] == ')') depth--;
    else if (depth == 0 && e.compare(i, opl, op) == 0) {
      L = trim(e.substr(0, i));
      R = trim(e.substr(i + opl));
      return 1;
    }
  }
  return 0;
}

float CnsBank::evalNum(const std::string& e0, Fighter& f, Fighter& p2) const {
  static thread_local int depth = 0;
  if (depth > 48) return 0;
  struct Depth { int& d; Depth(int& x) : d(x) { ++d; } ~Depth() { --d; } } guard(depth);
  std::string e = trim(e0);
  if (e.empty()) return 0;
  if (!e.empty() && e[0] == '$') {
    auto it = f.snap.map.find(e);
    if (it != f.snap.map.end()) return it->second;
    it = f.snap.map.find(std::string("let:") + e.substr(1));
    if (it != f.snap.map.end()) return it->second;
    it = f.snap.map.find(e.substr(1));
    if (it != f.snap.map.end()) return it->second;
  }
  const char* p = skipsp(e.c_str());
  if (*p == '(') {
    int depth = 0;
    const char* q = p;
    for (; *q; q++) {
      if (*q == '(') depth++;
      else if (*q == ')') { depth--; if (depth == 0) {
        std::string inner(p + 1, q);
        if (!*(q + 1)) return evalNum(inner, f, p2);
        break;
      } }
    }
  }
  std::string el0 = lower(e);
  if (el0.rfind("ifelse(", 0) == 0 && e.back() == ')') {
    auto args = splitTopComma(e.substr(7, e.size() - 8));
    if (args.size() >= 3)
      return evalBool(args[0], f, p2) ? evalNum(args[1], f, p2) : evalNum(args[2], f, p2);
  }
  if (el0.rfind("gethitvar(", 0) == 0 && e.back() == ')') {
    std::string n = lower(trim(e.substr(10, e.size() - 11)));
    if (n == "yvel" || n == "yvelocity") return f.snap.ghvVelY;
    if (n == "xvel" || n == "xvelocity") return f.snap.ghvVelX;
    if (n == "fall") return (float)f.snap.ghvFall;
    if (n == "groundtype") return (float)f.snap.ghvGroundType;
    if (n == "airtype") return (float)f.snap.ghvAirType;
    if (n == "animtype") return (float)f.snap.ghvAnimType;
    if (n == "slidetime") return (float)f.snap.ghvSlideTime;
    if (n == "hittime") return (float)f.snap.hitstun;
    if (n == "hitshaketime") return (float)f.snap.hitpause;
    if (n == "yaccel") return f.snap.ghvYaccel;
    return 0;
  }
  std::string L, R;
  if (splitTop(e, "+", L, R)) return evalNum(L, f, p2) + evalNum(R, f, p2);
  if (e.size() > 1 && splitTop(e, "-", L, R) && !L.empty()) return evalNum(L, f, p2) - evalNum(R, f, p2);
  if (splitTop(e, "*", L, R)) return evalNum(L, f, p2) * evalNum(R, f, p2);
  if (splitTop(e, "/", L, R)) {
    float d = evalNum(R, f, p2);
    return d != 0 ? evalNum(L, f, p2) / d : 0;
  }
  if (splitTop(e, "=", L, R)) return evalBool(e, f, p2) ? 1.f : 0.f;
  std::string el = lower(e);
  if (el == "time") return (float)f.snap.time;
  if (el == "hitshakeover") return (f.snap.hitpause <= 0) ? 1.f : 0.f;
  if (el == "hitover") return (f.snap.hitstun < 0) ? 1.f : 0.f;
  if (el == "numhelper") return world ? (float)world->helpers.size() : 0;
  if (el == "numexplod") return 0;
  if (el.size() > 1 && el[0] == '$') {
    auto it = f.snap.map.find(std::string("let:") + el.substr(1));
    if (it != f.snap.map.end()) return it->second;
    return 0;
  }
  {
    auto it = f.snap.map.find(std::string("let:") + el);
    if (it != f.snap.map.end()) return it->second;
  }
  if (el == "anim") return (float)f.snap.anim;
  if (el == "alive") return (float)f.snap.alive;
  if (el == "prevstateno") return (float)f.snap.prevState;
  if (el == "sysvar(1)" || el == "sysVar(1)") return (float)f.snap.sysVar1;
  if (el.rfind("const(", 0) == 0) {
    std::string n = lower(el.substr(6));
    if (n.find("walk.fwd") != std::string::npos) return walkFwd;
    if (n.find("walk.back") != std::string::npos) return walkBack;
    if (n.find("run.fwd") != std::string::npos) return runFwd;
    if (n.find("run.back.x") != std::string::npos || n.find("run.back") != std::string::npos) return runBackX;
    if (n.find("jump.neu.x") != std::string::npos) return jumpNeuX;
    if (n.find("jump.fwd") != std::string::npos) return jumpFwd;
    if (n.find("jump.back") != std::string::npos) return jumpBack;
    if (n.find("jump.y") != std::string::npos || n.find("jump.neu.y") != std::string::npos) return jumpNeuY;
    if (n.find("yaccel") != std::string::npos) return yaccel;
    if (n.find("stand.friction") != std::string::npos) return standFric;
    if (n.find("crouch.friction") != std::string::npos) return crouchFric;
  }
  if (el == "animtime") return (float)f.snap.animTimeLeft;
  if (el == "animelem") return (float)(f.snap.animElem + 1);
  if (el == "ctrl") return (float)f.snap.ctrl;
  if (el == "stateno") return (float)f.snap.state;
  if (el == "power") return (float)f.snap.power;
  if (el == "movecontact") return (float)f.snap.moveContact;
  if (el == "roundstate") return (float)f.snap.roundState;
  if (el == "vel y" || el == "vely") return f.snap.vel.y;
  if (el == "vel x" || el == "velx") return f.snap.vel.x;
  if (el == "pos y" || el == "posy") return f.snap.pos.y;
  if (el == "pos x" || el == "posx") return f.snap.pos.x;
  if (el == "p2bodydist x" || el == "p2bodydistx") {
    float dx = std::fabs(p2.snap.pos.x - f.snap.pos.x) - (float)(wFront + wBack);
    return dx;
  }
  if (el.rfind("var(", 0) == 0) {
    int i = atoi(el.c_str() + 4);
    if (i >= 0 && i < 64) return f.snap.var[i];
  }
  if (el.rfind("animelemtime(", 0) == 0) {
    int elem = atoi(el.c_str() + 13);
    return (float)f.AnimElemTime(elem);
  }
  char* end = nullptr;
  float v = strtof(e.c_str(), &end);
  if (end != e.c_str()) return v;
  return 0;
}

bool CnsBank::evalBool(const std::string& e0, Fighter& f, Fighter& p2) const {
  std::string e = trim(e0);
  if (e.empty()) return false;
  std::string L, R;
  if (splitTop(e, "||", L, R)) return evalBool(L, f, p2) || evalBool(R, f, p2);
  if (splitTop(e, "&&", L, R)) return evalBool(L, f, p2) && evalBool(R, f, p2);
  if (e[0] == '!') return !evalBool(e.substr(1), f, p2);
  if (e[0] == '(' && e.back() == ')') return evalBool(e.substr(1, e.size() - 2), f, p2);

  auto cmp = [&](const std::string& op) {
    return splitTop(e, op.c_str(), L, R);
  };
  auto stEq = [](char t, const std::string& r) {
    std::string u = lower(unquote(r));
    if (u.empty()) return false;
    char c = (char)toupper((unsigned char)u[0]);
    if (u.size() > 1 && (u.find('s') != std::string::npos || u.find('c') != std::string::npos || u.find('a') != std::string::npos))
      return u.find((char)tolower(t)) != std::string::npos;
    return t == c;
  };

  if (cmp("!=")) {
    std::string ll = lower(L);
    if (ll == "command") return !f.cmd.Was(unquote(R));
    if (ll == "statetype") return !stEq(stType(f), R);
    if (ll == "p2statetype") return !stEq(p2.snap.stateType, R);
    if (ll == "p2movetype") return !stEq(p2.snap.moveType, R);
    if (ll == "hitdefattr") return true;
    return evalNum(L, f, p2) != evalNum(R, f, p2);
  }
  if (cmp(">=")) return evalNum(L, f, p2) >= evalNum(R, f, p2);
  if (cmp("<=")) return evalNum(L, f, p2) <= evalNum(R, f, p2);
  if (cmp("=")) {
    std::string ll = lower(L);
    if (ll == "command") return f.cmd.Was(unquote(R));
    if (ll == "statetype") return stEq(stType(f), R);
    if (ll == "p2statetype") return stEq(p2.snap.stateType, R);
    if (ll == "p2movetype") return stEq(p2.snap.moveType, R);
    if (ll == "hitdefattr") return false;
    std::string rr = trim(R);
    if (!rr.empty() && rr.front() == '[') {
      float a = 0, b = 0;
      sscanf(rr.c_str(), "[%f,%f", &a, &b);
      float x = evalNum(L, f, p2);
      bool hiOpen = rr.find(')') != std::string::npos;
      return x >= a && (hiOpen ? x < b : x <= b);
    }
    return evalNum(L, f, p2) == evalNum(R, f, p2);
  }
  if (cmp(">")) return evalNum(L, f, p2) > evalNum(R, f, p2);
  if (cmp("<")) return evalNum(L, f, p2) < evalNum(R, f, p2);
  return evalNum(e, f, p2) != 0;
}

bool CnsBank::evalTriggers(const CnsCtrl& c, Fighter& f, Fighter& p2) const {
  for (const auto& t : c.triggerall)
    if (!evalBool(t, f, p2)) return false;
  if (c.triggers.empty()) return c.triggerall.empty() ? false : true;
  for (const auto& grp : c.triggers) {
    if (grp.empty()) continue;
    bool ok = true;
    for (const auto& t : grp)
      if (!evalBool(t, f, p2)) { ok = false; break; }
    if (ok) return true;
  }
  return false;
}

static std::string pget(const CnsCtrl& c, const char* k, const char* def = "") {
  std::string want = lower(k);
  for (auto& p : c.params) if (p.key == want) return p.val;
  return def;
}

void CnsBank::parseHitDef(const CnsCtrl& c, Fighter& f) {
  HitDef h;
  h.on = true;
  std::string dmg = pget(c, "damage", "40");
  h.damage = (int)parsePairFirst(dmg, 40);
  h.guardDamage = (int)parsePairSecond(dmg, 0);
  h.hittime = atoi(pget(c, "ground.hittime", "15").c_str());
  h.gvx = parsePairFirst(pget(c, "ground.velocity", "-4"), -4);
  h.gvy = parsePairSecond(pget(c, "ground.velocity", "-4,0"), 0);
  h.avx = parsePairFirst(pget(c, "air.velocity", "-2"), -2);
  h.avy = parsePairSecond(pget(c, "air.velocity", "-2,-3"), -3);
  std::string pt = pget(c, "pausetime", "8,8");
  h.pause1 = (int)parsePairFirst(pt, 8);
  h.pause2 = (int)parsePairSecond(pt, 8);
  h.fall = atoi(pget(c, "fall", "0").c_str());
  if (pget(c, "air.fall") == "1") h.fall = 1;
  h.slideTime = atoi(pget(c, "ground.slidetime", "0").c_str());
  h.animType = parseAnimType(pget(c, "animtype", "light"));
  std::string gt = pget(c, "ground.type", "high");
  h.groundType = parseHitType(gt);
  std::string at = pget(c, "air.type", "");
  h.airType = parseHitType(at.empty() ? gt : at);
  std::string ya = pget(c, "yaccel", "");
  h.yaccel = ya.empty() ? 0.35f : (float)atof(ya.c_str());
  h.kill = pget(c, "kill", "1").empty() ? 1 : atoi(pget(c, "kill", "1").c_str());
  h.guardKill = atoi(pget(c, "guard.kill", "1").c_str());
  std::string p2s = pget(c, "p2stateno", "");
  h.p2stateno = p2s.empty() ? -1 : atoi(p2s.c_str());
  std::string hf = pget(c, "hitflag", "MAF");
  h.hitflag = 0;
  for (char ch : lower(hf)) {
    if (ch == 'h') h.hitflag |= HF_H;
    else if (ch == 'l') h.hitflag |= HF_L;
    else if (ch == 'a') h.hitflag |= HF_A;
    else if (ch == 'd') h.hitflag |= HF_D;
    else if (ch == 'f') h.hitflag |= HF_F;
    else if (ch == 'm') h.hitflag |= HF_M;
  }
  if (!h.hitflag) h.hitflag = HF_H | HF_L | HF_A | HF_F;
  h.guardflag = 0;
  for (char ch : lower(pget(c, "guardflag", ""))) {
    if (ch == 'h') h.guardflag |= HF_H;
    else if (ch == 'l') h.guardflag |= HF_L;
    else if (ch == 'a') h.guardflag |= HF_A;
    else if (ch == 'm') h.guardflag |= HF_M;
  }
  std::string gv = pget(c, "guard.velocity", "");
  h.guardvx = gv.empty() ? h.gvx : parsePairFirst(gv, h.gvx);
  // Re-running HitDef while AnimElem stays true must not re-arm after a connect,
  // or pause2 is refreshed every frame and the opponent never leaves 5000.
  bool spent = f.snap.hitOnce != 0;
  f.snap.hit = h;
  f.snap.hitOnce = spent ? 1 : 0;
}

void CnsBank::runCtrl(const CnsCtrl& c, int idx, Fighter& f, Fighter& p2) {
  if (f.snap.hitpause > 0 && !c.ignorehitpause) return;
  if (!evalTriggers(c, f, p2)) return;
  if (c.persistent == 0) {
    std::string key = "p:" + std::to_string(f.snap.state) + ":" + std::to_string(idx);
    if (f.snap.map[key] != 0) return;
    f.snap.map[key] = 1;
  }
  const std::string& ty = c.type;
  if (ty == "changestate" || ty == "selfstate") {
    int v = (int)evalNum(pget(c, "value", "0"), f, p2);
    int ctrl = pget(c, "ctrl", "").empty() ? -1 : (int)evalNum(pget(c, "ctrl"), f, p2);
    Enter(f, v, ctrl);
  } else if (ty == "varset") {
    for (auto& p : c.params) {
      if (p.key.rfind("var(", 0) == 0) {
        int i = atoi(p.key.c_str() + 4);
        if (i >= 0 && i < 64) f.snap.var[i] = evalNum(p.val, f, p2);
      }
      if (p.key == "sysvar(1)" || p.key == "sysVar(1)")
        f.snap.sysVar1 = (int)evalNum(p.val, f, p2);
    }
  } else if (ty == "velset") {
    std::string xs = pget(c, "x"), ys = pget(c, "y");
    if (!xs.empty()) f.snap.vel.x = evalNum(xs, f, p2);
    if (!ys.empty()) f.snap.vel.y = evalNum(ys, f, p2);
  } else if (ty == "veladd") {
    std::string xs = pget(c, "x"), ys = pget(c, "y");
    if (!xs.empty()) f.snap.vel.x += evalNum(xs, f, p2);
    if (!ys.empty()) f.snap.vel.y += evalNum(ys, f, p2);
  } else if (ty == "velmul") {
    std::string xs = pget(c, "x"), ys = pget(c, "y");
    if (!xs.empty()) f.snap.vel.x *= evalNum(xs, f, p2);
    if (!ys.empty()) f.snap.vel.y *= evalNum(ys, f, p2);
  } else if (ty == "posadd") {
    std::string xs = pget(c, "x"), ys = pget(c, "y");
    if (!xs.empty()) f.snap.pos.x += evalNum(xs, f, p2) * f.snap.facing;
    if (!ys.empty()) f.snap.pos.y += evalNum(ys, f, p2);
  } else if (ty == "posset") {
    std::string xs = pget(c, "x"), ys = pget(c, "y");
    if (!xs.empty()) f.snap.pos.x = evalNum(xs, f, p2);
    if (!ys.empty()) f.snap.pos.y = evalNum(ys, f, p2);
  } else if (ty == "ctrlset") {
    f.snap.ctrl = atoi(pget(c, "value", "0").c_str());
  } else if (ty == "changeanim") {
    int v = (int)evalNum(pget(c, "value", "0"), f, p2);
    int elem = (int)evalNum(pget(c, "elem", "1"), f, p2);
    f.SetAnim(v, std::max(0, elem - 1));
  } else if (ty == "hitvelset") {
    // Go bytecode.go hitVelSet: vel = ghv.xvel * facing
    if (evalBool(pget(c, "x", "0"), f, p2))
      f.snap.vel.x = f.snap.ghvVelX * (float)f.snap.facing;
    if (evalBool(pget(c, "y", "0"), f, p2))
      f.snap.vel.y = f.snap.ghvVelY;
  } else if (ty == "hitdef") {
    parseHitDef(c, f);
  } else if (ty == "turn") {
    f.snap.facing = -f.snap.facing;
  } else if (ty == "assertspecial") {
    std::string flag = lower(unquote(pget(c, "flag")));
    if (flag == "noko") f.snap.asf |= ASF_noko;
    if (flag == "autoguard") f.snap.asf |= ASF_autoguard;
    if (flag == "noautoturn") f.snap.asf |= ASF_noautoturn;
    if (flag == "nowalk") f.snap.asf |= ASF_nowalk;
    if (flag == "noaicheat") f.snap.asf |= ASF_noaicheat;
    std::string flag2 = lower(unquote(pget(c, "flag2")));
    if (flag2 == "noautoturn") f.snap.asf |= ASF_noautoturn;
    if (flag2 == "nowalk") f.snap.asf |= ASF_nowalk;
  } else if (ty == "call") {
    auto it = funcs_.find(lower(pget(c, "name")));
    if (it != funcs_.end()) {
      auto pit = funcParams_.find(lower(pget(c, "name")));
      if (pit != funcParams_.end()) {
        for (size_t ai = 0; ai < pit->second.size(); ai++) {
          std::string aval = pget(c, (std::string("arg") + std::to_string((int)ai)).c_str());
          float v = aval.empty() ? 0 : evalNum(aval, f, p2);
          const std::string& pn = pit->second[ai];
          f.snap.map[pn] = v;
          f.snap.map[std::string("$") + pn] = v;
          f.snap.map[std::string("let:") + pn] = v;
        }
      }
      runCtrlList(it->second, f, p2);
    }
  } else if (ty == "let") {
    for (auto& p : c.params)
      f.snap.map[std::string("let:") + p.key] = evalNum(p.val, f, p2);
  } else if (ty == "for") {
    std::string var = pget(c, "var", "i");
    int start = (int)evalNum(pget(c, "start", "0"), f, p2);
    int lim = (int)evalNum(pget(c, "limit", "0"), f, p2);
    int step = (int)evalNum(pget(c, "step", "1"), f, p2);
    if (step == 0) step = 1;
    int st0 = f.snap.state;
    int guard = 0;
    for (int i = start; (step > 0 ? i < lim : i > lim) && guard < 256; i += step, guard++) {
      f.snap.map[std::string("let:") + var] = (float)i;
      runCtrlList(c.children, f, p2);
      if (f.snap.state != st0) return;
    }
  } else if (ty == "null") {
  } else if (ty == "statetypeset") {
    std::string st = pget(c, "statetype");
    if (!st.empty()) f.snap.stateType = (char)toupper((unsigned char)st[0]);
    std::string ph = pget(c, "physics");
    if (!ph.empty()) f.snap.physics = (char)toupper((unsigned char)ph[0]);
  } else if (ty == "projectile" && world) {
    Projectile p;
    p.active = 1;
    p.owner = f.playerIndex;
    p.pos = f.snap.pos;
    p.facing = f.snap.facing;
    std::string vx = pget(c, "vel x");
    if (vx.empty()) vx = pget(c, "vel", "4");
    p.vel.x = evalNum(vx, f, p2);
    p.vel.y = evalNum(pget(c, "y", "0"), f, p2);
    std::string an = pget(c, "projanim");
    if (an.empty()) an = pget(c, "anim", "0");
    p.anim = atoi(an.c_str());
    p.removetime = atoi(pget(c, "removetime", "-1").c_str());
    p.hits = atoi(pget(c, "projhits", "1").c_str());
    p.priority = atoi(pget(c, "projpriority", "1").c_str());
    p.priorityPoints = p.priority;
    p.id = atoi(pget(c, "id", "0").c_str());
    p.hit = f.snap.hit;
    p.hit.on = true;
    if ((int)world->projs.size() >= 32) return;
    world->projs.push_back(p);
  } else if (ty == "helper" && world) {
    int used = 0;
    for (auto& ex : world->helpers) if (ex.helperIndex > 0) used++;
    if (used >= world->helperMax) return;
    Fighter h;
    h.SetAssets(f.sff, f.air);
    h.cns = f.cns;
    h.playerIndex = f.playerIndex;
    h.pal = f.pal;
    h.helperIndex = (int)world->helpers.size() + 1;
    h.id = world->nextCharId++;
    h.parentId = f.id;
    h.helperId = atoi(pget(c, "id", "0").c_str());
    h.Reset(f.snap.teamSide, f.snap.pos.x, f.snap.pos.y);
    h.snap.facing = f.snap.facing;
    int st = atoi(pget(c, "stateno", "0").c_str());
    Enter(h, st, 0);
    world->helpers.push_back(std::move(h));
  }
}

void CnsBank::RunMinusOne(Fighter& f, Fighter& p2) {
  auto it = defs_.find(-1);
  if (it == defs_.end()) return;
  runCtrlList(it->second.ctrls, f, p2);
}

void CnsBank::runCtrlList(const std::vector<CnsCtrl>& ctrls, Fighter& f, Fighter& p2) {
  int idx = 0;
  int st = f.snap.state;
  for (const auto& c : ctrls) {
    runCtrl(c, idx++, f, p2);
    if (f.snap.state != st) return;
  }
}

void CnsBank::RunCurrent(Fighter& f, Fighter& p2) {
  auto it = defs_.find(f.snap.state);
  if (it == defs_.end()) {
    CommonLoco(f);
    return;
  }
  int idx = 0;
  int st = f.snap.state;
  for (const auto& c : it->second.ctrls) {
    runCtrl(c, idx++, f, p2);
    if (f.snap.state != st) return;
  }
}

void CnsBank::CommonLoco(Fighter& f) {
  uint32_t in = f.snap.input | f.snap.assertInput;
  bool fwd = f.cmd.Was("holdfwd") || ((f.snap.facing >= 0) ? (in & kInputR) : (in & kInputL));
  bool back = f.cmd.Was("holdback") || ((f.snap.facing >= 0) ? (in & kInputL) : (in & kInputR));
  bool down = f.cmd.Was("holddown") || (in & kInputD);
  bool up = f.cmd.Was("holdup") || (in & kInputU);
  int st = f.snap.state;
  if (st == 0) {
    f.snap.ctrl = 1;
    f.snap.stateType = 'S';
    f.snap.physics = 'S';
    f.snap.moveType = 'I';
    if (f.snap.anim != 0 && f.snap.anim != 5) f.SetAnim(0);
  } else if (st == 10) {
    if (f.snap.time == 0) f.snap.vel.x *= 0.75f;
    if (f.snap.animEnded || f.snap.time > 3) Enter(f, 11);
  } else if (st == 11) {
    f.snap.ctrl = 1;
    if (!down) Enter(f, 12);
  } else if (st == 12) {
    if (f.snap.animEnded || f.snap.time > 4) Enter(f, 0, 1);
  } else if (st == 20) {
    f.snap.ctrl = 1;
    if (up) { Enter(f, 40); return; }
    if (down) { Enter(f, 10); return; }
    if (fwd) {
      f.snap.vel.x = walkFwd;
      if (f.snap.anim != 20 && f.snap.anim != 5) f.SetAnim(20);
    } else if (back) {
      f.snap.vel.x = walkBack;
      if (f.snap.anim != 21 && f.snap.anim != 5) f.SetAnim(21);
    } else { f.snap.vel.x = 0; Enter(f, 0, 1); }
  } else if (st == 100) {
    f.snap.stateType = 'S';
    f.snap.physics = 'S';
    f.snap.vel.x = runFwd;
    f.snap.asf |= ASF_nowalk | ASF_noautoturn;
    if (!fwd) Enter(f, 0, 1);
  } else if (st == 105) {
    if (f.snap.time == 0) {
      f.snap.vel.x = runBackX;
      f.snap.vel.y = runBackY;
    }
    if (f.snap.time >= 2) f.snap.ctrl = 1;
    if (f.snap.vel.y > 0 && f.snap.pos.y >= 0) Enter(f, 106);
  } else if (st == 106) {
    f.snap.pos.y = 0;
    f.snap.vel.y = 0;
    if (f.snap.time >= 7) Enter(f, 0, 1);
  } else if (st == 40) {
    // Go common1 StateDef 40: sample holdfwd/back every tick, jump when animTime=0
    f.snap.stateType = 'S';
    f.snap.physics = 'S';
    f.snap.ctrl = 0;
    if (f.snap.time == 0) f.snap.sysVar1 = 0;
    if (back) f.snap.sysVar1 = -1;
    else if (fwd) f.snap.sysVar1 = 1;
    if (f.snap.animEnded || f.snap.time >= 7) {
      if (f.snap.sysVar1 == 0) f.snap.vel.x = jumpNeuX;
      else if (f.snap.sysVar1 == 1)
        f.snap.vel.x = (f.snap.prevState == 100) ? runFwd : jumpFwd;
      else f.snap.vel.x = jumpBack;
      f.snap.vel.y = jumpNeuY;
      Enter(f, 50, 1);
    }
  } else if (st == 50) {
    f.snap.ctrl = 1;
    f.snap.stateType = 'A';
    f.snap.physics = 'A';
    if (f.snap.time == 0) {
      int an = 41;
      if (f.snap.vel.x > 0) an = 42;
      else if (f.snap.vel.x < 0) an = 43;
      f.SetAnim(an);
    }
    if (f.snap.pos.y >= 0 && f.snap.vel.y >= 0 && f.snap.time > 1) Enter(f, 52);
  } else if (st == 52) {
    f.snap.pos.y = 0;
    if (f.snap.time == 0) f.snap.vel.y = 0;
    if (f.snap.time == 3) f.snap.ctrl = 1;
    if (f.snap.animEnded || f.snap.time > 12) Enter(f, 0, 1);
  } else if (st == 5000) {
    f.snap.moveType = 'H';
    f.snap.ctrl = 0;
    if (f.snap.hitstun <= 0 && f.snap.time > 8) Enter(f, 0, 1);
  } else if (st == 5030) {
    f.snap.moveType = 'H';
    if (f.snap.pos.y >= 0 && f.snap.vel.y >= 0 && f.snap.time > 2) Enter(f, 5110);
  } else if (st == 5110) {
    f.snap.pos.y = 0;
    f.snap.vel = {0, 0};
    if (f.snap.time > 30) Enter(f, 5120);
  } else if (st == 5120) {
    if (f.snap.animEnded || f.snap.time > 12) Enter(f, 0, 1);
  } else if (st == 150 || st == 151) {
    f.snap.moveType = 'H';
    f.snap.ctrl = 0;
    if (f.snap.hitstun <= 0 && f.snap.time > 8) Enter(f, st == 151 ? 11 : 0, 1);
  } else if (st == 130 || st == 131 || st == 120) {
    f.snap.ctrl = 1;
    if (!back) Enter(f, st == 131 ? 11 : 0, 1);
  }
}

void CnsBank::ApplyPhysics(Fighter& f, float left, float right) {
  // Go posUpdate: skipped during hitpause; friction after integrating vel
  if (f.snap.hitpause > 0) return;
  if (!std::isfinite(f.snap.vel.x)) f.snap.vel.x = 0;
  if (!std::isfinite(f.snap.vel.y)) f.snap.vel.y = 0;
  f.snap.vel.x = std::clamp(f.snap.vel.x, -80.f, 80.f);
  f.snap.vel.y = std::clamp(f.snap.vel.y, -80.f, 80.f);
  f.snap.pos.x += f.snap.vel.x * (float)f.snap.facing;
  f.snap.pos.y += f.snap.vel.y;
  char ph = f.snap.physics;
  const float originLs = 1.f; // localscl * (320 / gameWidth)
  if (ph == 'S') {
    f.snap.vel.x *= standFric;
    if (std::fabs(f.snap.vel.x) < 1.f / originLs) f.snap.vel.x = 0;
    if (f.snap.pos.y > 0) { f.snap.pos.y = 0; f.snap.vel.y = 0; }
  } else if (ph == 'C') {
    f.snap.vel.x *= crouchFric;
    if (f.snap.pos.y > 0) { f.snap.pos.y = 0; f.snap.vel.y = 0; }
  } else if (ph == 'A') {
    f.snap.vel.y += yaccel;
    if (f.snap.pos.y > 0) {
      f.snap.pos.y = 0;
      if (f.snap.state != 52 && f.snap.moveType != 'H') Enter(f, 52);
      else if (f.snap.moveType == 'H' && f.snap.state != 5110) Enter(f, 5110);
    }
  }
  if (!std::isfinite(left) || !std::isfinite(right) || left >= right) {
    left = -1000;
    right = 1000;
  }
  f.snap.pos.x = std::clamp(f.snap.pos.x, left, right);
  f.snap.pos.y = std::clamp(f.snap.pos.y, -400.f, 20.f);
  if (!std::isfinite(f.snap.pos.x)) f.snap.pos.x = 0;
  if (!std::isfinite(f.snap.pos.y)) f.snap.pos.y = 0;
  if (f.snap.hitstun >= 0) f.snap.hitstun--;
}

int CnsBank::ComputeDamage(const Fighter& def, const Fighter& atk, int raw, bool kill, bool bounds) const {
  if (raw == 0) return 0;
  double damage = (double)raw;
  double atkmul = (double)atk.snap.attackMul * ((double)attackBase / 100.0);
  double finalDef = (double)defenceBase / 100.0;
  if (finalDef <= 0) finalDef = 1;
  damage *= atkmul / finalDef;
  if (damage > 0 && damage < 1) damage = 1;
  if (bounds && damage > (double)def.snap.life) damage = (double)def.snap.life;
  if (!kill && damage >= (double)def.snap.life && def.snap.life > 0)
    damage = (double)(def.snap.life - 1);
  return (int)std::lround(damage);
}

void CnsBank::ApplyQueuedDamage(Fighter& f) {
  // Go actionRun: lifeAdd(-ghv.damage, kill=true, absolute=true) if moveType H
  if (f.snap.ghvDamage != 0) {
    int dmg = f.snap.ghvDamage;
    if ((f.snap.asf & ASF_noko) && dmg >= f.snap.life && f.snap.life > 0)
      dmg = f.snap.life - 1;
    f.snap.life = std::max(0, f.snap.life - dmg);
  }
  f.snap.ghvDamage = 0;
}

void CnsBank::OnHit(Fighter& atk, Fighter& def, int hitResult) {
  const HitDef& h = atk.snap.hit;
  if (!h.on || atk.snap.hitOnce) return;
  atk.snap.hitOnce = 1;
  atk.snap.moveContact = 1;
  atk.snap.hitpause = h.pause1;
  def.snap.hitpause = h.pause2;
  bool guarded = hitResult == 2;
  int raw = guarded ? h.guardDamage : h.damage;
  bool kill = guarded ? h.guardKill != 0 : h.kill != 0;
  int dmg = ComputeDamage(def, atk, raw, kill, true);
  def.snap.ghvDamage += dmg;
  if (world) {
    DamagePopup pop;
    pop.pos.x = def.snap.pos.x;
    pop.pos.y = def.snap.pos.y - 72.f;
    pop.amount = dmg;
    pop.ttl = pop.ttlMax = 48;
    pop.guarded = guarded ? 1 : 0;
    if (world->popups.size() < 24) world->popups.push_back(pop);
    int atkSide = atk.playerIndex == 0 ? 0 : 1;
    world->lastHitDmg[atkSide] = dmg;
    world->comboHits[atkSide]++;
    world->comboDmg[atkSide] += dmg;
  }
  def.snap.hitstun = guarded ? h.guardHittime : h.hittime;
  // Go: ghv.xvel = ground_velocity * -attacker.facing; HitVelSet applies * getter.facing
  float byf = (float)atk.snap.facing;
  if (guarded) {
    def.snap.ghvVelX = h.guardvx * -byf;
    def.snap.ghvVelY = 0;
    def.snap.ghvFall = 0;
  } else if (def.snap.stateType == 'A') {
    def.snap.ghvVelX = h.avx * -byf;
    def.snap.ghvVelY = h.avy;
    def.snap.ghvFall = h.fall ? 1 : 0;
    def.snap.hitstun = h.airHittime;
  } else {
    def.snap.ghvVelX = h.gvx * -byf;
    def.snap.ghvVelY = h.gvy;
    def.snap.ghvFall = h.fall ? 1 : 0;
    if (def.snap.ghvFall && def.snap.ghvVelY == 0) def.snap.ghvVelY = -0.001f;
    if (def.snap.ghvVelY != 0) def.snap.hitstun = h.airHittime;
  }
  def.snap.ghvSlideTime = h.slideTime;
  def.snap.ghvAnimType = h.animType;
  def.snap.ghvGroundType = h.groundType;
  def.snap.ghvAirType = h.airType;
  def.snap.ghvYaccel = h.yaccel;
  def.snap.moveType = 'H';
  def.snap.ctrl = 0;
  def.snap.hittmp = (def.snap.ghvFall) ? 2 : 1;
  if (guarded) {
    Enter(def, def.snap.stateType == 'C' ? 152 : (def.snap.stateType == 'A' ? 154 : 150));
    return;
  }
  if (h.p2stateno >= 0) {
    Enter(def, h.p2stateno);
    return;
  }
  if (def.snap.stateType == 'C') Enter(def, 5010);
  else if (def.snap.stateType == 'A') Enter(def, 5020);
  else Enter(def, 5000);
}

void CnsBank::GlobalCollision(Fighter& a, Fighter& b) {
  auto overlap = [](const Fighter& fa, const Rect& ra, const Fighter& fb, const Rect& rb) {
    auto box = [](const Fighter& f, const Rect& r, float& x0, float& y0, float& x1, float& y1) {
      x0 = f.snap.pos.x + r.x0 * f.snap.facing;
      x1 = f.snap.pos.x + r.x1 * f.snap.facing;
      if (x0 > x1) std::swap(x0, x1);
      y0 = f.snap.pos.y + r.y0;
      y1 = f.snap.pos.y + r.y1;
      if (y0 > y1) std::swap(y0, y1);
    };
    float ax0, ay0, ax1, ay1, bx0, by0, bx1, by1;
    box(fa, ra, ax0, ay0, ax1, ay1);
    box(fb, rb, bx0, by0, bx1, by1);
    return ax0 < bx1 && ax1 > bx0 && ay0 < by1 && ay1 > by0;
  };
  auto hittable = [](const Fighter& getter, const HitDef& hd) {
    char t = getter.snap.stateType;
    if ((hd.hitflag & HF_H) == 0 && t == 'S') return false;
    if ((hd.hitflag & HF_L) == 0 && t == 'C') return false;
    if ((hd.hitflag & HF_A) == 0 && t == 'A') return false;
    if ((hd.hitflag & HF_D) == 0 && t == 'L') return false;
    if ((hd.hitflag & HF_F) == 0 && getter.snap.hittmp >= 2) return false;
    return true;
  };
  auto hitResultCheck = [](Fighter& getter, const HitDef& hd) -> int {
    uint32_t in = getter.snap.input | getter.snap.assertInput;
    bool back = (getter.snap.facing >= 0) ? (in & kInputL) != 0 : (in & kInputR) != 0;
    bool canguard = (getter.snap.asf & ASF_autoguard) || (back && (getter.snap.ctrl || getter.snap.state == 120 ||
                     getter.snap.state == 130 || getter.snap.state == 131));
    int result = 1;
    if (canguard) {
      char t = getter.snap.stateType;
      if ((hd.guardflag & HF_H) && t == 'S') result = 2;
      else if ((hd.guardflag & HF_L) && t == 'C') result = 2;
      else if ((hd.guardflag & HF_A) && t == 'A') result = 2;
      else if ((hd.guardflag & HF_M) && (t == 'S' || t == 'C')) result = 2;
    }
    return result;
  };
  auto playerHit = [&](Fighter& atk, Fighter& def) {
    if (!atk.snap.hit.on || atk.snap.hitOnce) return;
    if (!hittable(def, atk.snap.hit)) return;
    auto* fa = atk.CurrentFrame();
    auto* fb = def.CurrentFrame();
    std::vector<Rect> atkBoxes, defBoxes;
    if (fa && !fa->clsn1.empty()) atkBoxes = fa->clsn1;
    else atkBoxes.push_back(Rect{-8.f, -80.f, 48.f, -8.f});
    if (fb && !fb->clsn2.empty()) defBoxes = fb->clsn2;
    else defBoxes.push_back(Rect{-(float)wBack, -90.f, (float)wFront, 0.f});
    for (auto c1 : atkBoxes)
      for (auto c2 : defBoxes)
        if (overlap(atk, c1, def, c2)) {
          OnHit(atk, def, hitResultCheck(def, atk.snap.hit));
          return;
        }
  };
  playerHit(a, b);
  playerHit(b, a);
  float d = b.snap.pos.x - a.snap.pos.x;
  float need = (float)(wFront + wBack);
  if (std::fabs(d) < need && std::fabs(a.snap.pos.y - b.snap.pos.y) < 80) {
    float push = (need - std::fabs(d)) * 0.5f;
    if (d >= 0) { a.snap.pos.x -= push; b.snap.pos.x += push; }
    else { a.snap.pos.x += push; b.snap.pos.x -= push; }
  }
}

void CnsBank::TickProjectiles(Fighter& p1, Fighter& p2, float left, float right) {
  if (!world) return;
  auto clsnAt = [](Fighter& owner, int anim, Vec2 pos, int facing, bool clsn1) -> std::vector<Rect> {
    std::vector<Rect> out;
    if (!owner.air) return out;
    const Animation* a = owner.air->Get(anim);
    if (!a || a->frames.empty()) return out;
    const auto& fr = a->frames[0];
    const auto& src = clsn1 ? fr.clsn1 : fr.clsn2;
    for (auto r : src) {
      float x0 = pos.x + r.x0 * facing, x1 = pos.x + r.x1 * facing;
      if (x0 > x1) std::swap(x0, x1);
      float y0 = pos.y + r.y0, y1 = pos.y + r.y1;
      if (y0 > y1) std::swap(y0, y1);
      out.push_back({x0, y0, x1, y1});
    }
    return out;
  };
  auto boxesHit = [](const std::vector<Rect>& a, const std::vector<Rect>& b) {
    for (auto ra : a) for (auto rb : b)
      if (ra.x0 < rb.x1 && ra.x1 > rb.x0 && ra.y0 < rb.y1 && ra.y1 > rb.y0) return true;
    return false;
  };
  auto cancelHits = [](Projectile& p, Projectile& opp) {
    if (p.priorityPoints > opp.priorityPoints) p.priorityPoints--;
    else p.hits--;
    if (p.hits <= 0) p.active = 0;
    else p.hitpause = p.hit.pause1;
  };

  for (size_t i = 0; i < world->projs.size(); i++) {
    auto& p = world->projs[i];
    if (!p.active) continue;
    for (size_t j = i + 1; j < world->projs.size(); j++) {
      auto& q = world->projs[j];
      if (!q.active || p.owner == q.owner) continue;
      Fighter& po = p.owner == 0 ? p1 : p2;
      Fighter& qo = q.owner == 0 ? p1 : p2;
      auto ca = clsnAt(po, p.anim, p.pos, p.facing, false);
      auto cb = clsnAt(qo, q.anim, q.pos, q.facing, false);
      if (ca.empty() || cb.empty()) continue;
      if (boxesHit(ca, cb)) {
        cancelHits(p, q);
        cancelHits(q, p);
      }
    }
  }

  for (auto& p : world->projs) {
    if (!p.active) continue;
    if (p.hitpause > 0) { p.hitpause--; continue; }
    p.pos.x += p.vel.x * (float)p.facing;
    p.pos.y += p.vel.y;
    p.vel.x += p.accel.x;
    p.vel.y += p.accel.y;
    p.vel.x *= p.velmul.x;
    p.vel.y *= p.velmul.y;
    if (p.removetime > 0) p.removetime--;
    if (p.removetime == 0 || p.pos.x < left - 40 || p.pos.x > right + 40 || p.pos.y > 40) {
      p.active = 0;
      continue;
    }
    Fighter& def = (p.owner == 0) ? p2 : p1;
    Fighter& own = (p.owner == 0) ? p1 : p2;
    if (!p.hit.on || p.hits <= 0) continue;
    auto atkBox = clsnAt(own, p.anim, p.pos, p.facing, true);
    if (atkBox.empty()) atkBox = clsnAt(own, p.anim, p.pos, p.facing, false);
    auto* fb = def.CurrentFrame();
    if (!fb || fb->clsn2.empty()) continue;
    std::vector<Rect> defBox;
    for (auto c2 : fb->clsn2) {
      float x0 = def.snap.pos.x + c2.x0 * def.snap.facing;
      float x1 = def.snap.pos.x + c2.x1 * def.snap.facing;
      if (x0 > x1) std::swap(x0, x1);
      float y0 = def.snap.pos.y + c2.y0, y1 = def.snap.pos.y + c2.y1;
      if (y0 > y1) std::swap(y0, y1);
      defBox.push_back({x0, y0, x1, y1});
    }
    if (!boxesHit(atkBox, defBox)) continue;
    Fighter fake;
    fake.snap.hit = p.hit;
    fake.snap.hitOnce = 0;
    fake.snap.attackMul = 1;
    OnHit(fake, def, 1);
    p.hits--;
    p.hitpause = p.hit.pause1;
    if (p.hits <= 0) p.active = 0;
  }
  world->projs.erase(std::remove_if(world->projs.begin(), world->projs.end(),
                                    [](const Projectile& p) { return !p.active; }),
                     world->projs.end());
}
