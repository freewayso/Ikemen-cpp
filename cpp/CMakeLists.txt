cmake_minimum_required(VERSION 3.16)
project(ikemen_cpp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(IKEMEN_RELAY_ONLY "Build only the KCP room relay (no SDL game)" OFF)

file(GLOB LUA_SRC CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua-5.4.7/src/*.c)
list(FILTER LUA_SRC EXCLUDE REGEX ".*/lua\\.c$")
list(FILTER LUA_SRC EXCLUDE REGEX ".*/luac\\.c$")

set(IKEMEN_CLIENT_SRC
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
  src/room_sync.cpp
  src/camera.cpp
  src/rollback.cpp
  src/stage.cpp
  src/title.cpp
  src/cns.cpp
  src/lobby.cpp
  src/android_fs.cpp
  third_party/kcp/ikcp.c
)

if(NOT IKEMEN_RELAY_ONLY)
  add_library(lua54 STATIC ${LUA_SRC})
  target_include_directories(lua54 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua-5.4.7/src)
  if(WIN32)
    target_compile_definitions(lua54 PRIVATE LUA_USE_WINDOWS)
  else()
    target_compile_definitions(lua54 PRIVATE LUA_USE_POSIX)
  endif()
  if(ANDROID)
    set_target_properties(lua54 PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()

  if(ANDROID)
    add_library(ikemen_cpp SHARED ${IKEMEN_CLIENT_SRC})
    target_include_directories(ikemen_cpp PRIVATE src ${CMAKE_CURRENT_SOURCE_DIR}/third_party/kcp)
    if(TARGET SDL2::SDL2)
      target_link_libraries(ikemen_cpp PRIVATE lua54 SDL2::SDL2)
    elseif(TARGET SDL2)
      target_link_libraries(ikemen_cpp PRIVATE lua54 SDL2)
    else()
      message(FATAL_ERROR "SDL2 target missing (Android cmake must FetchContent SDL2 first)")
    endif()
    find_library(IK_GLES3 GLESv3 REQUIRED)
    find_library(IK_EGL EGL REQUIRED)
    find_library(IK_LOG log REQUIRED)
    find_library(IK_ANDROID android REQUIRED)
    find_library(IK_Z z REQUIRED)
    target_link_libraries(ikemen_cpp PRIVATE ${IK_GLES3} ${IK_EGL} ${IK_LOG} ${IK_ANDROID} ${IK_Z})
  else()
    find_package(SDL2 QUIET)
    if(NOT SDL2_FOUND)
      find_package(PkgConfig REQUIRED)
      pkg_check_modules(SDL2 REQUIRED sdl2)
    endif()

    add_executable(ikemen_cpp ${IKEMEN_CLIENT_SRC})

    target_include_directories(ikemen_cpp PRIVATE src ${SDL2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/third_party/kcp)
    if(TARGET SDL2::SDL2)
      target_link_libraries(ikemen_cpp PRIVATE lua54 SDL2::SDL2)
    elseif(TARGET SDL2::SDL2-static)
      target_link_libraries(ikemen_cpp PRIVATE lua54 SDL2::SDL2-static)
    else()
      target_link_libraries(ikemen_cpp PRIVATE lua54 ${SDL2_LIBRARIES})
    endif()
    if(WIN32)
      target_link_libraries(ikemen_cpp PRIVATE opengl32)
    else()
      find_package(OpenGL REQUIRED)
      target_link_libraries(ikemen_cpp PRIVATE OpenGL::GL)
    endif()
    find_package(ZLIB)
    if(ZLIB_FOUND)
      target_link_libraries(ikemen_cpp PRIVATE ZLIB::ZLIB)
    else()
      target_link_libraries(ikemen_cpp PRIVATE z)
    endif()
    if(WIN32)
      target_link_libraries(ikemen_cpp PRIVATE ws2_32 imm32)
    else()
      target_link_libraries(ikemen_cpp PRIVATE dl pthread)
    endif()

    set_target_properties(ikemen_cpp PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin_cpp
    )
  endif()
endif()

if(NOT ANDROID)
  add_executable(ikemen_relay src/relay_server.cpp third_party/kcp/ikcp.c)
  target_include_directories(ikemen_relay PRIVATE src ${CMAKE_CURRENT_SOURCE_DIR}/third_party/kcp)
  if(WIN32)
    target_link_libraries(ikemen_relay PRIVATE ws2_32)
  endif()
  set_target_properties(ikemen_relay PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin_cpp
  )

  add_executable(ikemen_lobby src/lobby_server.cpp)
  target_include_directories(ikemen_lobby PRIVATE src)
  if(WIN32)
    target_link_libraries(ikemen_lobby PRIVATE ws2_32)
  endif()
  set_target_properties(ikemen_lobby PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin_cpp
  )
endif()
