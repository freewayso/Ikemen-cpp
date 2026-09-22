#include "render.hpp"
#include "gl_loader.hpp"
#include <cstdio>
#include <cmath>
#include <cctype>

#ifndef APIENTRY
#define APIENTRY
#endif

#define GLFN(ret, name, ...) typedef ret (APIENTRY *name##_t)(__VA_ARGS__); static name##_t p##name = nullptr;
GLFN(unsigned, glCreateShader, unsigned)
GLFN(void, glShaderSource, unsigned, int, const char* const*, const int*)
GLFN(void, glCompileShader, unsigned)
GLFN(unsigned, glCreateProgram, void)
GLFN(void, glAttachShader, unsigned, unsigned)
GLFN(void, glLinkProgram, unsigned)
GLFN(void, glUseProgram, unsigned)
GLFN(void, glGenTextures, int, unsigned*)
GLFN(void, glBindTexture, unsigned, unsigned)
GLFN(void, glTexImage2D, unsigned, int, int, int, int, int, unsigned, unsigned, const void*)
GLFN(void, glTexParameteri, unsigned, unsigned, int)
GLFN(void, glGenBuffers, int, unsigned*)
GLFN(void, glBindBuffer, unsigned, unsigned)
GLFN(void, glBufferData, unsigned, intptr_t, const void*, unsigned)
GLFN(void, glBufferSubData, unsigned, intptr_t, intptr_t, const void*)
GLFN(void, glEnableVertexAttribArray, unsigned)
GLFN(void, glDisable, unsigned)
GLFN(void, glVertexAttribPointer, unsigned, int, unsigned, unsigned char, int, const void*)
GLFN(void, glDrawArrays, unsigned, int, int)
GLFN(int, glGetUniformLocation, unsigned, const char*)
GLFN(void, glUniformMatrix4fv, int, int, unsigned char, const float*)
GLFN(void, glUniform4f, int, float, float, float, float)
GLFN(void, glUniform1i, int, int)
GLFN(void, glGenVertexArrays, int, unsigned*)
GLFN(void, glBindVertexArray, unsigned)
GLFN(void, glGetShaderiv, unsigned, unsigned, int*)
GLFN(void, glGetShaderInfoLog, unsigned, int, int*, char*)
GLFN(void, glGetProgramiv, unsigned, unsigned, int*)
GLFN(void, glGetProgramInfoLog, unsigned, int, int*, char*)
GLFN(void, glScissor, int, int, int, int)
GLFN(void, glDeleteShader, unsigned)
GLFN(void, glEnable, unsigned)
GLFN(void, glBlendFunc, unsigned, unsigned)
GLFN(void, glPixelStorei, unsigned, int)
GLFN(void, glClear, unsigned)
GLFN(void, glClearColor, float, float, float, float)
GLFN(void, glViewport, int, int, int, int)

static void bindGL() {
#define BIND(n) p##n = (n##_t)LoadGL(#n);
  BIND(glCreateShader) BIND(glShaderSource) BIND(glCompileShader)
  BIND(glCreateProgram) BIND(glAttachShader) BIND(glLinkProgram) BIND(glUseProgram)
  BIND(glGenTextures) BIND(glBindTexture) BIND(glTexImage2D) BIND(glTexParameteri)
  BIND(glGenBuffers) BIND(glBindBuffer) BIND(glBufferData) BIND(glBufferSubData)
  BIND(glEnableVertexAttribArray) BIND(glVertexAttribPointer) BIND(glDrawArrays)
  BIND(glDisable)
  BIND(glGetUniformLocation) BIND(glUniformMatrix4fv) BIND(glUniform4f) BIND(glUniform1i)
  BIND(glGenVertexArrays) BIND(glBindVertexArray)
  BIND(glGetShaderiv) BIND(glGetShaderInfoLog) BIND(glDeleteShader)
  BIND(glGetProgramiv) BIND(glGetProgramInfoLog) BIND(glScissor)
  BIND(glEnable) BIND(glBlendFunc) BIND(glPixelStorei)
  BIND(glClear) BIND(glClearColor) BIND(glViewport)
#undef BIND
}

static const char* kVertCore = R"(#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUv;
uniform mat4 uProj;
uniform mat4 uModel;
out vec2 vUv;
void main(){ vUv=aUv; gl_Position=uProj*uModel*vec4(aPos,0,1); }
)";
static const char* kFragCore = R"(#version 330 core
in vec2 vUv; out vec4 Frag;
uniform sampler2D uTex; uniform vec4 uTint; uniform int uUseTex;
void main(){ vec4 c = uUseTex!=0 ? texture(uTex,vUv)*uTint : uTint; if(c.a<0.01) discard; Frag=c; }
)";
static const char* kVertEs = R"(#version 300 es
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUv;
uniform mat4 uProj;
uniform mat4 uModel;
out vec2 vUv;
void main(){ vUv=aUv; gl_Position=uProj*uModel*vec4(aPos,0,1); }
)";
static const char* kFragEs = R"(#version 300 es
precision mediump float;
in vec2 vUv; out vec4 Frag;
uniform sampler2D uTex; uniform vec4 uTint; uniform int uUseTex;
void main(){ vec4 c = uUseTex!=0 ? texture(uTex,vUv)*uTint : uTint; if(c.a<0.01) discard; Frag=c; }
)";

static unsigned compile(unsigned type, const char* src) {
  unsigned s = pglCreateShader(type);
  pglShaderSource(s, 1, &src, nullptr);
  pglCompileShader(s);
  int ok = 0;
  pglGetShaderiv(s, 0x8B81, &ok);
  if (!ok) {
    char log[512];
    pglGetShaderInfoLog(s, 512, nullptr, log);
    std::fprintf(stderr, "shader: %s\n", log);
    pglDeleteShader(s);
    return 0;
  }
  return s;
}

bool Renderer::Init(int winW, int winH) {
  winW_ = winW; winH_ = winH;
  bindGL();
  if (!pglCreateShader || !pglGenVertexArrays || !pglUseProgram) {
    std::fprintf(stderr, "GL functions missing (need GL 3.3 or GLES 3)\n");
    return false;
  }
#ifdef __ANDROID__
  unsigned vs = compile(0x8B31, kVertEs);
  unsigned fs = compile(0x8B30, kFragEs);
  if (!vs || !fs) {
    if (vs) pglDeleteShader(vs);
    if (fs) pglDeleteShader(fs);
    vs = compile(0x8B31, kVertCore);
    fs = compile(0x8B30, kFragCore);
  }
#else
  unsigned vs = compile(0x8B31, kVertCore);
  unsigned fs = compile(0x8B30, kFragCore);
  if (!vs || !fs) {
    if (vs) pglDeleteShader(vs);
    if (fs) pglDeleteShader(fs);
    vs = compile(0x8B31, kVertEs);
    fs = compile(0x8B30, kFragEs);
  }
#endif
  if (!vs || !fs) {
    if (vs) pglDeleteShader(vs);
    if (fs) pglDeleteShader(fs);
    std::fprintf(stderr, "shaders failed (need GL 3.3 or GLES 3)\n");
    return false;
  }
  prog_ = pglCreateProgram();
  pglAttachShader(prog_, vs);
  pglAttachShader(prog_, fs);
  pglLinkProgram(prog_);
  pglDeleteShader(vs); pglDeleteShader(fs);
  int linked = 0;
  if (pglGetProgramiv) {
    pglGetProgramiv(prog_, 0x8B82, &linked);
    if (!linked) {
      char log[512] = {};
      if (pglGetProgramInfoLog) pglGetProgramInfoLog(prog_, 512, nullptr, log);
      std::fprintf(stderr, "program link: %s\n", log);
      return false;
    }
  }
  uProj_ = pglGetUniformLocation(prog_, "uProj");
  uModel_ = pglGetUniformLocation(prog_, "uModel");
  uTint_ = pglGetUniformLocation(prog_, "uTint");
  uUseTex_ = pglGetUniformLocation(prog_, "uUseTex");
  float verts[24] = {};
  pglGenVertexArrays(1, &vao_);
  pglGenBuffers(1, &vbo_);
  pglBindVertexArray(vao_);
  pglBindBuffer(0x8892, vbo_);
  pglBufferData(0x8892, sizeof(verts), verts, 0x88E8);
  pglEnableVertexAttribArray(0);
  pglVertexAttribPointer(0, 2, 0x1406, 0, 16, (void*)0);
  pglEnableVertexAttribArray(1);
  pglVertexAttribPointer(1, 2, 0x1406, 0, 16, (void*)8);
  pglGenTextures(1, &texFallback_);
  pglBindTexture(0x0DE1, texFallback_);
  pglTexParameteri(0x0DE1, 0x2801, 0x2600);
  pglTexParameteri(0x0DE1, 0x2800, 0x2600);
  unsigned white = 0xFFFFFFFFu;
  int ifmt0 = 0x1908;
#ifndef __ANDROID__
  ifmt0 = 0x8058;
#endif
  pglTexImage2D(0x0DE1, 0, ifmt0, 1, 1, 0, 0x1908, 0x1401, &white);
  pglEnable(0x0BE2);
  pglBlendFunc(0x0302, 0x0303);
  pglDisable(0x0B44);
  Resize(winW, winH);
  return true;
}

void Renderer::Resize(int winW, int winH) {
  if (winW < 1) winW = 1280;
  if (winH < 1) winH = 720;
  winW_ = winW;
  winH_ = winH;
  const float target = 1280.f / 720.f;
  const float scr = (float)winW / (float)winH;
  if (scr > target) {
    vpH_ = winH;
    vpW_ = (int)((float)winH * target + 0.5f);
    vpX_ = (winW - vpW_) / 2;
    vpY_ = 0;
  } else {
    vpW_ = winW;
    vpH_ = (int)((float)winW / target + 0.5f);
    vpX_ = 0;
    vpY_ = (winH - vpH_) / 2;
  }
  if (vpW_ < 1) vpW_ = winW;
  if (vpH_ < 1) vpH_ = winH;
}

void Renderer::Begin() {
  Begin((float)kGameW, (float)kGameH);
}

void Renderer::Begin(float vw, float vh) {
  pglViewport(0, 0, winW_, winH_);
  if (pglScissor) {
    pglDisable(0x0C11);
  }
  pglClearColor(0, 0, 0, 1);
  pglClear(0x00004000);
  pglViewport(vpX_, vpY_, vpW_, vpH_);
  if (pglScissor) {
    pglEnable(0x0C11);
    pglScissor(vpX_, vpY_, vpW_, vpH_);
  }
  pglClearColor(0.08f, 0.09f, 0.12f, 1);
  pglClear(0x00004000);
  pglUseProgram(prog_);
  float sx = 2.f / vw, sy = -2.f / vh;
  float proj[16] = {sx,0,0,0, 0,sy,0,0, 0,0,1,0, -1,1,0,1};
  pglUniformMatrix4fv(uProj_, 1, 0, proj);
  pglBindVertexArray(vao_);
}

void Renderer::BeginHud(float vw, float vh) {
  pglViewport(vpX_, vpY_, vpW_, vpH_);
  if (pglScissor) {
    pglEnable(0x0C11);
    pglScissor(vpX_, vpY_, vpW_, vpH_);
  }
  pglUseProgram(prog_);
  float sx = 2.f / vw, sy = -2.f / vh;
  float proj[16] = {sx,0,0,0, 0,sy,0,0, 0,0,1,0, -1,1,0,1};
  pglUniformMatrix4fv(uProj_, 1, 0, proj);
  pglBindVertexArray(vao_);
}

unsigned Renderer::bindSpriteTex(const SpriteImage& spr) {
  if (spr.w <= 0 || spr.h <= 0 || spr.rgba.size() < (size_t)spr.w * spr.h * 4) return 0;
  auto it = texCache_.find(&spr);
  if (it != texCache_.end()) {
    pglBindTexture(0x0DE1, it->second);
    return it->second;
  }
  unsigned id = 0;
  pglGenTextures(1, &id);
  pglBindTexture(0x0DE1, id);
  pglPixelStorei(0x0CF5, 1);
  pglTexParameteri(0x0DE1, 0x2801, 0x2600);
  pglTexParameteri(0x0DE1, 0x2800, 0x2600);
  pglTexParameteri(0x0DE1, 0x2802, 0x812F);
  pglTexParameteri(0x0DE1, 0x2803, 0x812F);
  int ifmt = 0x1908;
#ifndef __ANDROID__
  ifmt = 0x8058;
#endif
  pglTexImage2D(0x0DE1, 0, ifmt, spr.w, spr.h, 0, 0x1908, 0x1401, spr.rgba.data());
  texCache_[&spr] = id;
  return id;
}

void Renderer::drawQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1) {
  float verts[] = {
    x0, y0, u0, v0,
    x1, y0, u1, v0,
    x1, y1, u1, v1,
    x0, y0, u0, v0,
    x1, y1, u1, v1,
    x0, y1, u0, v1,
  };
  float ident[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
  pglBindBuffer(0x8892, vbo_);
  pglBufferSubData(0x8892, 0, sizeof(verts), verts);
  pglUniformMatrix4fv(uModel_, 1, 0, ident);
  pglDrawArrays(4, 0, 6);
}

void Renderer::DrawSprite(const SpriteImage& spr, float x, float y, float facing, float scale) {
  DrawSprite(spr, x, y, facing, scale, scale, 1, 1, 1, 1);
}

void Renderer::DrawSprite(const SpriteImage& spr, float x, float y, float facing, float sx, float sy,
                          float r, float g, float b, float a) {
  if (spr.w <= 0 || spr.rgba.empty()) return;
  if (!bindSpriteTex(spr)) return;
  float w = spr.w * std::fabs(sx);
  float h = spr.h * std::fabs(sy);
  float ax = spr.ax * std::fabs(sx);
  float ay = spr.ay * std::fabs(sy);
  float x0, x1, u0, u1;
  if (facing >= 0) {
    x0 = x - ax;
    x1 = x0 + w;
    u0 = 0;
    u1 = 1;
  } else {
    x0 = x + ax - w;
    x1 = x0 + w;
    u0 = 1;
    u1 = 0;
  }
  float y0, y1, v0, v1;
  if (sy >= 0) {
    y0 = y - ay;
    y1 = y0 + spr.h * sy;
    v0 = 0;
    v1 = 1;
  } else {
    // Go/MUGEN shadow: flatten from the axis downward, V flipped.
    y0 = y;
    y1 = y + ay * std::fabs(sy);
    v0 = 1;
    v1 = 0;
  }
  pglUniform4f(uTint_, r, g, b, a);
  pglUniform1i(uUseTex_, 1);
  drawQuad(x0, y0, x1, y1, u0, v0, u1, v1);
}

void Renderer::DrawDigits(float x, float y, int value, float scale, float r, float g, float b, float a) {
  static const char* glyph[10] = {
      "111101101101111", "001001001001001", "111001111100111", "111001111001111", "101101111001001",
      "111100111001111", "111100111101111", "111001001001001", "111101111101111", "111101111001111",
  };
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%d", value);
  float px = 1.2f * scale, gap = 1.2f * scale;
  float cx = x;
  for (const char* p = buf; *p; ++p) {
    if (*p == '-') {
      DrawRect(cx, y + 2.f * px, 3.f * px, px, r, g, b, a);
      cx += 4.f * px + gap;
      continue;
    }
    if (*p < '0' || *p > '9') continue;
    const char* bits = glyph[*p - '0'];
    for (int row = 0; row < 5; row++)
      for (int col = 0; col < 3; col++)
        if (bits[row * 3 + col] == '1')
          DrawRect(cx + col * px, y + row * px, px, px, r, g, b, a);
    cx += 4.f * px + gap;
  }
}

void Renderer::DrawWord(float x, float y, const char* word, float scale, float r, float g, float b, float a) {
  auto bitsOf = [](char ch) -> const char* {
    switch (ch) {
      case 'A': return "010101111101101";
      case 'R': return "111101111110101";
      case 'U': return "101101101101111";
      case 'K': return "101101110101101";
      case 'O': return "111101101101111";
      case 'W': return "101101101111101";
      case 'I': return "111010010010111";
      case 'N': return "101111111101101";
      case 'D': return "110101101101110";
      case 'P': return "111101111100100";
      case 'F': return "111100111100100";
      case 'S': return "011100111001110";
      case 'G': return "011100101101011";
      case 'M': return "101111101101101";
      case 'L': return "100100100100111";
      case 'E': return "111100111100111";
      case 'T': return "111010010010010";
      case 'Y': return "101101010010010";
      case 'J': return "001001001101010";
      case 'C': return "011100100100011";
      case '1': return "001001001001001";
      case '2': return "111001111100111";
      default: return nullptr;
    }
  };
  float px = 1.4f * scale, gap = 1.6f * scale;
  float cx = x;
  for (const char* p = word; *p; ++p) {
    if (*p == ' ') { cx += 3.f * px + gap; continue; }
    const char* bits = bitsOf((char)toupper((unsigned char)*p));
    if (!bits) { cx += 4.f * px + gap; continue; }
    for (int row = 0; row < 5; row++)
      for (int col = 0; col < 3; col++)
        if (bits[row * 3 + col] == '1')
          DrawRect(cx + col * px, y + row * px, px, px, r, g, b, a);
    cx += 4.f * px + gap;
  }
}

void Renderer::DrawRect(float x, float y, float w, float h, float r, float g, float b, float a) {
  pglBindTexture(0x0DE1, texFallback_);
  pglUniform4f(uTint_, r, g, b, a);
  pglUniform1i(uUseTex_, 0);
  drawQuad(x, y, x + w, y + h, 0, 0, 1, 1);
}

void Renderer::End() {
  if (pglDisable) pglDisable(0x0C11);
}
void Renderer::Shutdown() {}
