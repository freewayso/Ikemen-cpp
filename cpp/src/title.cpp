#include "title.hpp"
#include "types.hpp"
#include <algorithm>
#include <cstring>
#include <string>

static const TitleScreen::Item kRoot[] = {
  {"ARCADE", TitleScreen::Sub, +TitleMenu::Arcade},
  {"VS MODE", TitleScreen::Sub, +TitleMenu::Versus},
  {"NETWORK", TitleScreen::Sub, +TitleMenu::Network},
  {"PRACTICE", TitleScreen::Sub, +TitleMenu::Practice},
  {"MISSION", TitleScreen::Sub, +TitleMenu::Mission},
  {"WATCH MODE", TitleScreen::Sub, +TitleMenu::Watch},
  {"OPTIONS", TitleScreen::Ignore, -1},
  {"EXIT", TitleScreen::ExitGame, -1},
};
static const TitleScreen::Item kArcade[] = {
  {"SINGLE MODE", TitleScreen::Fight, -1},
  {"TEAM ARCADE", TitleScreen::Ignore, -1},
  {"TEAM CO-OP", TitleScreen::Ignore, -1},
  {"BACK", TitleScreen::Back, -1},
};
static const TitleScreen::Item kVersus[] = {
  {"1P VS 2P", TitleScreen::Fight, -1},
  {"TEAM VERSUS", TitleScreen::Ignore, -1},
  {"VERSUS CO-OP", TitleScreen::Ignore, -1},
  {"QUICK MATCH", TitleScreen::Fight, -1},
  {"BACK", TitleScreen::Back, -1},
};
static const TitleScreen::Item kNet[] = {
  {"ONLINE", TitleScreen::Online, -1},
  {"HOST GAME", TitleScreen::HostNet, -1},
  {"JOIN GAME", TitleScreen::JoinNet, -1},
  {"BACK", TitleScreen::Back, -1},
};
static const TitleScreen::Item kPrac[] = {
  {"TRAINING", TitleScreen::Fight, -1},
  {"BACK", TitleScreen::Back, -1},
};
static const TitleScreen::Item kMission[] = {
  {"SURVIVAL", TitleScreen::Ignore, -1},
  {"SURVIVAL CO-OP", TitleScreen::Ignore, -1},
  {"TIME ATTACK", TitleScreen::Ignore, -1},
  {"BONUS GAMES", TitleScreen::Ignore, -1},
  {"BACK", TitleScreen::Back, -1},
};
static const TitleScreen::Item kWatch[] = {
  {"CPU MATCH", TitleScreen::Fight, -1},
  {"RANDOMTEST", TitleScreen::Ignore, -1},
  {"REPLAY", TitleScreen::Ignore, -1},
  {"BACK", TitleScreen::Back, -1},
};

static const uint8_t kAZ[26][7] = {
  {0x70,0x88,0x88,0xF8,0x88,0x88,0x88}, // A
  {0xF0,0x88,0x88,0xF0,0x88,0x88,0xF0},
  {0x70,0x88,0x80,0x80,0x80,0x88,0x70},
  {0xF0,0x88,0x88,0x88,0x88,0x88,0xF0},
  {0xF8,0x80,0x80,0xF0,0x80,0x80,0xF8},
  {0xF8,0x80,0x80,0xF0,0x80,0x80,0x80},
  {0x70,0x88,0x80,0xB8,0x88,0x88,0x78},
  {0x88,0x88,0x88,0xF8,0x88,0x88,0x88},
  {0x70,0x20,0x20,0x20,0x20,0x20,0x70},
  {0x08,0x08,0x08,0x08,0x08,0x88,0x70},
  {0x88,0x90,0xA0,0xC0,0xA0,0x90,0x88},
  {0x80,0x80,0x80,0x80,0x80,0x80,0xF8},
  {0x88,0xD8,0xA8,0xA8,0x88,0x88,0x88},
  {0x88,0xC8,0xA8,0x98,0x88,0x88,0x88},
  {0x70,0x88,0x88,0x88,0x88,0x88,0x70},
  {0xF0,0x88,0x88,0xF0,0x80,0x80,0x80},
  {0x70,0x88,0x88,0x88,0xA8,0x90,0x68},
  {0xF0,0x88,0x88,0xF0,0xA0,0x90,0x88},
  {0x78,0x80,0x80,0x70,0x08,0x08,0xF0},
  {0xF8,0x20,0x20,0x20,0x20,0x20,0x20},
  {0x88,0x88,0x88,0x88,0x88,0x88,0x70},
  {0x88,0x88,0x88,0x88,0x88,0x50,0x20},
  {0x88,0x88,0x88,0xA8,0xA8,0xD8,0x88},
  {0x88,0x88,0x50,0x20,0x50,0x88,0x88},
  {0x88,0x88,0x50,0x20,0x20,0x20,0x20},
  {0xF8,0x08,0x10,0x20,0x40,0x80,0xF8},
};
static const uint8_t kDig[10][7] = {
  {0x70,0x88,0x98,0xA8,0xC8,0x88,0x70},
  {0x20,0x60,0x20,0x20,0x20,0x20,0x70},
  {0x70,0x88,0x08,0x30,0x40,0x80,0xF8},
  {0xF8,0x08,0x10,0x30,0x08,0x88,0x70},
  {0x10,0x30,0x50,0x90,0xF8,0x10,0x10},
  {0xF8,0x80,0xF0,0x08,0x08,0x88,0x70},
  {0x30,0x40,0x80,0xF0,0x88,0x88,0x70},
  {0xF8,0x08,0x10,0x20,0x40,0x40,0x40},
  {0x70,0x88,0x88,0x70,0x88,0x88,0x70},
  {0x70,0x88,0x88,0x78,0x08,0x10,0x60},
};

static const uint8_t* glyph(char c) {
  static const uint8_t sp[7] = {};
  static const uint8_t sl[7] = {0x08,0x10,0x10,0x20,0x20,0x40,0x80};
  static const uint8_t mn[7] = {0,0,0,0xF8,0,0,0};
  if (c >= 'a' && c <= 'z') c = (char)(c - 32);
  if (c >= 'A' && c <= 'Z') return kAZ[c - 'A'];
  if (c >= '0' && c <= '9') return kDig[c - '0'];
  if (c == '/') return sl;
  if (c == '-') return mn;
  if (c == '_') return mn;
  if (c == '.') {
    static const uint8_t dt[7] = {0, 0, 0, 0, 0, 0x20, 0x20};
    return dt;
  }
  if (c == ':') {
    static const uint8_t cl[7] = {0, 0x20, 0x20, 0, 0x20, 0x20, 0};
    return cl;
  }
  return sp;
}

bool TitleScreen::Load(const std::string&) {
  menu_ = 0;
  cursor_ = 0;
  scroll_ = 0;
  return true;
}

const std::vector<TitleScreen::Item>& TitleScreen::items() const {
  static std::vector<Item> buf;
  const Item* p = kRoot;
  int n = (int)(sizeof(kRoot) / sizeof(kRoot[0]));
  if (menu_ == TitleMenu::Arcade) { p = kArcade; n = (int)(sizeof(kArcade) / sizeof(kArcade[0])); }
  else if (menu_ == TitleMenu::Versus) { p = kVersus; n = (int)(sizeof(kVersus) / sizeof(kVersus[0])); }
  else if (menu_ == TitleMenu::Network) { p = kNet; n = (int)(sizeof(kNet) / sizeof(kNet[0])); }
  else if (menu_ == TitleMenu::Practice) { p = kPrac; n = (int)(sizeof(kPrac) / sizeof(kPrac[0])); }
  else if (menu_ == TitleMenu::Mission) { p = kMission; n = (int)(sizeof(kMission) / sizeof(kMission[0])); }
  else if (menu_ == TitleMenu::Watch) { p = kWatch; n = (int)(sizeof(kWatch) / sizeof(kWatch[0])); }
  buf.assign(p, p + n);
  return buf;
}

float TitleScreen::textWidth(const char* s) const {
  int n = s ? (int)std::strlen(s) : 0;
  return n * 24.f;
}

void TitleScreen::drawText(Renderer& r, float x, float y, const char* s, float cr, float cg, float cb, int align) {
  float w = textWidth(s);
  float cx = x;
  if (align < 0) cx = x - w;
  else if (align == 0) cx = x - w * 0.5f;
  const float px = 4.f;
  for (; s && *s; ++s) {
    const uint8_t* g = glyph(*s);
    for (int row = 0; row < 7; row++) {
      for (int col = 0; col < 5; col++) {
        if (g[row] & (0x80 >> col))
          r.DrawRect(cx + col * px, y + row * px, px, px, cr, cg, cb, 1.f);
      }
    }
    cx += 24.f;
  }
}

TitleScreen::Action TitleScreen::Tick(uint32_t pressed, bool escPressed, float dt, int mx, int my, bool click) {
  (void)dt;
  frame_++;
  const auto& it = items();
  int n = (int)it.size();
  if (n <= 0) return None;
  if (pressed & kInputU) cursor_ = (cursor_ + n - 1) % n;
  if (pressed & kInputD) cursor_ = (cursor_ + 1) % n;
  const int vis = 6;
  if (cursor_ < scroll_) scroll_ = cursor_;
  if (cursor_ >= scroll_ + vis) scroll_ = cursor_ - vis + 1;

  auto activate = [&]() -> Action {
    const Item& sel = it[cursor_];
    if (sel.kind == Sub) { menu_ = sel.sub; cursor_ = 0; scroll_ = 0; }
    else if (sel.kind == Back) { menu_ = +TitleMenu::Root; cursor_ = 0; scroll_ = 0; }
    else if (sel.kind == Fight) { fightMenu_ = menu_; menu_ = +TitleMenu::Root; cursor_ = 0; return StartFight; }
    else if (sel.kind == HostNet) { fightMenu_ = +TitleMenu::HostNet; menu_ = +TitleMenu::Root; cursor_ = 0; return StartFight; }
    else if (sel.kind == JoinNet) { fightMenu_ = +TitleMenu::JoinNet; menu_ = +TitleMenu::Root; cursor_ = 0; return StartFight; }
    else if (sel.kind == Online) { fightMenu_ = +TitleMenu::Online; menu_ = +TitleMenu::Root; cursor_ = 0; return StartFight; }
    else if (sel.kind == ExitGame) return Quit;
    return None;
  };

  if (click) {
    for (int i = 0; i < vis && scroll_ + i < n; i++) {
      float y = 300.f + i * 54.f;
      if (mx >= 500 && mx <= 1280 && my >= y - 8 && my <= y + 40) {
        cursor_ = scroll_ + i;
        return activate();
      }
    }
  }
  if ((pressed & kInputA) || (pressed & kInputS)) return activate();
  if (escPressed && menu_ != TitleMenu::Root) { menu_ = +TitleMenu::Root; cursor_ = 0; scroll_ = 0; }
  return None;
}

void TitleScreen::Draw(Renderer& r, const char* user) {
  r.DrawRect(0, 0, 1280, 720, 0.55f, 0.72f, 0.88f, 1);
  r.DrawRect(0, 0, 1280, 90, 0.45f, 0.62f, 0.82f, 1);
  r.DrawRect(500, 250, 780, 400, 0.05f, 0.05f, 0.08f, 0.55f);
  drawText(r, 80, 30, "IKEMEN GO", 1, 1, 1, 1);
  if (user && user[0]) {
    std::string who = std::string("HI ") + user;
    drawText(r, 1240, 30, who.c_str(), 1, 1, 1, -1);
  }

  const auto& it = items();
  const int vis = 6;
  for (int i = 0; i < vis && scroll_ + i < (int)it.size(); i++) {
    int idx = scroll_ + i;
    float y = 300.f + i * 54.f;
    bool on = idx == cursor_;
    float cr = on ? 123.f / 255.f : 1.f;
    float cg = on ? 206.f / 255.f : 1.f;
    float cb = on ? 1.f : 1.f;
    drawText(r, 1240.f, y, it[idx].label, cr, cg, cb, -1);
  }

  r.DrawRect(0, 690, 1280, 30, 0, 0, 0.25f, 0.9f);
  drawText(r, 640, 696, "CLICK  UP/DOWN  ENTER/J  ESC", 0.8f, 0.8f, 0.8f, 0);
}

void TitleScreen::DrawWait(Renderer& r, const char* msg, const char* sub) {
  r.DrawRect(0, 0, 1280, 720, 0.08f, 0.08f, 0.12f, 1);
  drawText(r, 640, 220, msg ? msg : "WAITING", 1, 1, 1, 0);
  drawText(r, 640, 300, sub && sub[0] ? sub : "data/net.ini  [Net] Relay=  Port=", 0.9f, 0.9f, 0.7f, 0);
  drawText(r, 640, 360, "CLIENT IS OFFSET TO THE RIGHT", 0.85f, 0.85f, 0.9f, 0);
  drawText(r, 640, 430, "ESC TO CANCEL", 0.8f, 0.8f, 0.8f, 0);
}

void TitleScreen::DrawLogin(Renderer& r, const std::string& user, const std::string& pass, const std::string& lobby,
                            const std::string& relay, int field, const char* status, bool pad) {
  r.DrawRect(0, 0, 1280, 720, 0.08f, 0.1f, 0.16f, 1);
  r.DrawRect(0, 0, 1280, 90, 0.45f, 0.62f, 0.82f, 1);
  drawText(r, 80, 30, "LOGIN / IP", 1, 1, 1, 1);

  auto fieldBox = [&](int id, float y, const char* label, const std::string& val) {
    drawText(r, 40, y + 12, label, 0.8f, 0.9f, 1, 1);
    r.DrawRect(280, y, 960, 46, field == id ? 0.18f : 0.05f, field == id ? 0.32f : 0.12f,
               field == id ? 0.42f : 0.18f, 0.95f);
    drawText(r, 296, y + 12, val.empty() ? "_" : val.c_str(), 1, 1, 1, 1);
  };
  fieldBox(0, 108, "USER", user);
  fieldBox(1, 162, "PASS", std::string(pass.size(), '-'));
  fieldBox(2, 216, "LOBBY", lobby);
  fieldBox(3, 270, "RELAY", relay);

  r.DrawRect(80, 332, 280, 52, 0.12f, 0.35f, 0.22f, 1);
  drawText(r, 220, 346, "LOGIN", 1, 1, 1, 0);
  r.DrawRect(400, 332, 320, 52, 0.35f, 0.22f, 0.12f, 1);
  drawText(r, 560, 346, "REGISTER", 1, 1, 1, 0);
  r.DrawRect(760, 332, 240, 52, 0.18f, 0.28f, 0.5f, 1);
  drawText(r, 880, 346, "SAVE", 1, 1, 1, 0);
  drawText(r, 640, 396, status && status[0] ? status : "SET LOBBY/RELAY IP THEN LOGIN", 1, 0.85f, 0.4f, 0);

  if (pad) {
    static const char* keys[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", ".", ":"};
    for (int i = 0; i < 12; i++) {
      float x = 80.f + (i % 6) * 190.f;
      float y = 450.f + (i / 6) * 70.f;
      r.DrawRect(x, y, 176, 60, 0.16f, 0.2f, 0.28f, 0.95f);
      drawText(r, x + 88, y + 16, keys[i], 1, 1, 1, 0);
    }
    r.DrawRect(80, 594, 360, 56, 0.4f, 0.18f, 0.18f, 1);
    drawText(r, 260, 608, "DEL", 1, 1, 1, 0);
    r.DrawRect(480, 594, 360, 56, 0.18f, 0.28f, 0.5f, 1);
    drawText(r, 660, 608, "SAVE", 1, 1, 1, 0);
    r.DrawRect(880, 594, 320, 56, 0.12f, 0.35f, 0.22f, 1);
    drawText(r, 1040, 608, "LOGIN", 1, 1, 1, 0);
  }
  r.DrawRect(0, 690, 1280, 30, 0, 0, 0.25f, 0.9f);
  drawText(r, 640, 696, pad ? "TAP FIELD  KEYPAD IP  IME USER" : "TAB FIELD  ENTER LOGIN  ESC", 0.8f, 0.8f, 0.8f, 0);
}

int TitleScreen::HitLogin(int mx, int my, bool pad) const {
  auto inBox = [&](float x, float y, float w, float h) {
    return mx >= x && mx <= x + w && my >= y && my <= y + h;
  };
  if (inBox(280, 108, 960, 46)) return 0;
  if (inBox(280, 162, 960, 46)) return 1;
  if (inBox(280, 216, 960, 46)) return 2;
  if (inBox(280, 270, 960, 46)) return 3;
  if (inBox(80, 332, 280, 52)) return 10;
  if (inBox(400, 332, 320, 52)) return 11;
  if (inBox(760, 332, 240, 52)) return 12;
  if (pad) {
    for (int i = 0; i < 12; i++) {
      float x = 80.f + (i % 6) * 190.f;
      float y = 450.f + (i / 6) * 70.f;
      if (inBox(x, y, 176, 60)) return 100 + i;
    }
    if (inBox(80, 594, 360, 56)) return 200;
    if (inBox(480, 594, 360, 56)) return 12;
    if (inBox(880, 594, 320, 56)) return 10;
  }
  return -1;
}

void TitleScreen::DrawRooms(Renderer& r, const std::string& user, const std::vector<std::string>& rows, int cursor,
                            const char* status) {
  r.DrawRect(0, 0, 1280, 720, 0.08f, 0.1f, 0.16f, 1);
  r.DrawRect(0, 0, 1280, 90, 0.45f, 0.62f, 0.82f, 1);
  drawText(r, 80, 30, "ROOMS", 1, 1, 1, 1);
  std::string who = "HI " + user;
  drawText(r, 400, 30, who.c_str(), 1, 1, 1, 1);
  r.DrawRect(980, 18, 260, 54, 0.18f, 0.28f, 0.5f, 1);
  drawText(r, 1110, 32, "IP SET", 1, 1, 1, 0);
  const int vis = 7;
  int scroll = 0;
  if (cursor >= vis) scroll = cursor - vis + 1;
  if (rows.empty()) drawText(r, 640, 320, "NO ROOMS  PRESS C CREATE", 0.8f, 0.8f, 0.9f, 0);
  for (int i = 0; i < vis && scroll + i < (int)rows.size(); i++) {
    int idx = scroll + i;
    float y = 160.f + i * 54.f;
    bool on = idx == cursor;
    r.DrawRect(80, y - 8, 1120, 48, on ? 0.15f : 0.05f, on ? 0.28f : 0.08f, on ? 0.4f : 0.12f, 0.85f);
    drawText(r, 100, y, rows[idx].c_str(), 1, 1, 1, 1);
  }
  drawText(r, 640, 620, status && status[0] ? status : "", 1, 0.85f, 0.4f, 0);
  r.DrawRect(0, 690, 1280, 30, 0, 0, 0.25f, 0.9f);
  drawText(r, 640, 696, "JOIN  CREATE  IP SET  ESC", 0.8f, 0.8f, 0.8f, 0);
}
