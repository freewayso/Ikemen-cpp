CXX = g++
CC = gcc
CXXFLAGS = -std=c++17 -O2 -I src -I third_party/lua-5.4.7/src -I third_party/kcp
CFLAGS = -O2 -I third_party/lua-5.4.7/src -I third_party/kcp

ifeq ($(OS),Windows_NT)
  EXE := .exe
  RELAY_LIBS := -lws2_32
  GAME_CXXFLAGS := $(CXXFLAGS) -ID:/tool/msys64/mingw64/include/SDL2
  LDFLAGS := -lmingw32 -lSDL2main -lSDL2 -lopengl32 -lws2_32 -lz -limm32
  MKDIR_BIN := mkdir -p ../bin_cpp
else
  EXE :=
  RELAY_LIBS :=
  GAME_CXXFLAGS := $(CXXFLAGS)
  LDFLAGS := -lSDL2 -lGL -lz -ldl -lpthread
  MKDIR_BIN := mkdir -p ../bin_cpp
endif

LUA_SRC := $(filter-out third_party/lua-5.4.7/src/lua.c third_party/lua-5.4.7/src/luac.c,$(wildcard third_party/lua-5.4.7/src/*.c))
LUA_OBJ := $(LUA_SRC:.c=.o)
KCP_OBJ := third_party/kcp/ikcp.o
CPP_SRC := src/main.cpp src/engine.cpp src/gl_loader.cpp src/render.cpp src/sff.cpp src/air.cpp src/fighter.cpp src/lua_host.cpp src/cmd.cpp src/input.cpp src/net.cpp src/room_sync.cpp src/camera.cpp src/rollback.cpp src/stage.cpp src/title.cpp src/cns.cpp src/lobby.cpp src/android_fs.cpp
CPP_OBJ := $(CPP_SRC:.cpp=.o)
HDRS := $(wildcard src/*.hpp)

GAME := ../bin_cpp/ikemen_cpp$(EXE)
RELAY := ../bin_cpp/ikemen_relay$(EXE)
LOBBY := ../bin_cpp/ikemen_lobby$(EXE)

.PHONY: all relay lobby game clean

ifeq ($(OS),Windows_NT)
all: game relay lobby
else
all: relay lobby
endif

relay: $(RELAY)
lobby: $(LOBBY)
game: $(GAME)

$(GAME): $(LUA_OBJ) $(KCP_OBJ) $(CPP_OBJ)
	$(MKDIR_BIN)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(RELAY): src/relay_server.cpp src/relay_proto.hpp $(KCP_OBJ)
	$(MKDIR_BIN)
	$(CXX) -std=c++17 -O2 -I src -I third_party/kcp -o $@ src/relay_server.cpp $(KCP_OBJ) $(RELAY_LIBS)

$(LOBBY): src/lobby_server.cpp src/lobby_proto.hpp
	$(MKDIR_BIN)
	$(CXX) -std=c++17 -O2 -I src -o $@ src/lobby_server.cpp $(RELAY_LIBS)

src/%.o: src/%.cpp $(HDRS)
	$(CXX) $(GAME_CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(LUA_OBJ) $(KCP_OBJ) $(CPP_OBJ) $(GAME) $(RELAY) $(LOBBY)
