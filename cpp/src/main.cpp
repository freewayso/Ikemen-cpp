#include "engine.hpp"
#include "log.hpp"
#include <cstdio>
#include <exception>
#include <memory>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#define SDL_MAIN_HANDLED

int main(int argc, char** argv) {
  try {
    auto e = std::make_unique<Engine>();
    if (!e->Init(argc, argv)) {
      GameLog::Get().Error("init failed");
#ifdef _WIN32
      MessageBoxA(nullptr, "Init failed. Need OpenGL 3.3 and files next to the repo (chars, stages, data).",
                  "Ikemen GO", MB_OK);
#endif
      return 1;
    }
    e->Run();
    e->Shutdown();
  } catch (const std::exception& ex) {
    GameLog::Get().Error("crash: %s", ex.what());
#ifdef _WIN32
    MessageBoxA(nullptr, ex.what(), "Ikemen GO crash", MB_OK);
#endif
    return 1;
  }
  return 0;
}
