cmake_minimum_required(VERSION 3.16)
project(ikemen_cpp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB LUA_SRC CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua-5.4.7/src/*.c)
list(FILTER LUA_SRC EXCLUDE REGEX ".*/lua\\.c$")
list(FILTER LUA_SRC EXCLUDE REGEX ".*/luac\\.c$")

add_library(lua54 STATIC ${LUA_SRC})
target_include_directories(lua54 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua-5.4.7/src)
if(WIN32)
  target_compile_definitions(lua54 PRIVATE LUA_USE_WINDOWS)
endif()

find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED sdl2)
endif()

add_executable(ikemen_cpp
  src/main.cpp
  src/engine.cpp
  src/gl_loader.cpp
  src/render.cpp
  src/sff.cpp
  src/air.cpp
  src/fighter.cpp
  src/lua_host.cpp
  src/cmd.cpp
  src/input.cpp
  src/net.cpp
  src/camera.cpp
  src/rollback.cpp
  src/stage.cpp
  src/title.cpp
  src/cns.cpp
  src/log.cpp
  third_party/kcp/ikcp.c
)

target_include_directories(ikemen_cpp PRIVATE src ${SDL2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/third_party/kcp)
if(TARGET SDL2::SDL2)
  target_link_libraries(ikemen_cpp PRIVATE lua54 SDL2::SDL2 opengl32)
elseif(TARGET SDL2::SDL2-static)
  target_link_libraries(ikemen_cpp PRIVATE lua54 SDL2::SDL2-static opengl32)
else()
  target_link_libraries(ikemen_cpp PRIVATE lua54 ${SDL2_LIBRARIES} opengl32)
endif()
find_package(ZLIB)
if(ZLIB_FOUND)
  target_link_libraries(ikemen_cpp PRIVATE ZLIB::ZLIB)
else()
  target_link_libraries(ikemen_cpp PRIVATE z)
endif()
if(WIN32)
  target_link_libraries(ikemen_cpp PRIVATE ws2_32 imm32)
endif()

set_target_properties(ikemen_cpp PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin_cpp
)

add_executable(ikemen_relay src/relay_server.cpp)
target_include_directories(ikemen_relay PRIVATE src)
if(WIN32)
  target_link_libraries(ikemen_relay PRIVATE ws2_32)
endif()
set_target_properties(ikemen_relay PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin_cpp
)
