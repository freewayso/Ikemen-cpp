cmake_minimum_required(VERSION 3.22)
project(ikemen_android C CXX)

include(FetchContent)
set(SDL_SHARED TRUE CACHE BOOL "" FORCE)
set(SDL_STATIC FALSE CACHE BOOL "" FORCE)
set(SDL_TEST FALSE CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL TRUE CACHE BOOL "" FORCE)
FetchContent_Declare(SDL2
  URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.11/SDL2-2.30.11.tar.gz
)
FetchContent_MakeAvailable(SDL2)

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)
add_subdirectory(${REPO_ROOT}/cpp ${CMAKE_BINARY_DIR}/ikemen_cpp)
