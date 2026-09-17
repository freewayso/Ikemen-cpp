#include "gl_loader.hpp"
#include <SDL.h>
#include <SDL_opengl.h>
#include <cstring>

static void* (*getProc)(const char*) = nullptr;

void* LoadGL(const char* name) {
  return SDL_GL_GetProcAddress(name);
}

bool InitGLLoader() {
  getProc = (void* (*)(const char*))SDL_GL_GetProcAddress;
  return LoadGL("glCreateShader") != nullptr || LoadGL("glGenTextures") != nullptr;
}
