#include "engine.hpp"
#include <cstdio>
#include <exception>
#include <memory>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#ifndef __ANDROID__
#define SDL_MAIN_HANDLED
#endif
#ifdef __ANDROID__
#include <SDL.h>
extern "C"
#endif
int main(int argc, char** argv) {
  try {
    auto e = std::make_unique<Engine>();
    if (!e->Init(argc, argv)) {
      std::fprintf(stderr, "init failed\n");
#ifdef _WIN32
      MessageBoxA(nullptr, "Init failed. Need OpenGL 3.3 and files next to the repo (chars, stages, data).",
                  "Ikemen GO", MB_OK);
#endif
      return 1;
    }
    e->Run();
    e->Shutdown();
  } catch (const std::exception& ex) {
    std::fprintf(stderr, "crash: %s\n", ex.what());
#ifdef _WIN32
    MessageBoxA(nullptr, ex.what(), "Ikemen GO crash", MB_OK);
#endif
    return 1;
  }
  return 0;
}
