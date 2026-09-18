CXX = g++
CC = gcc
CXXFLAGS = -std=c++17 -O2 -I src -I third_party/lua-5.4.7/src -I third_party/kcp -ID:/tool/msys64/mingw64/include/SDL2
CFLAGS = -O2 -I third_party/lua-5.4.7/src -I third_party/kcp
LDFLAGS = -lmingw32 -lSDL2main -lSDL2 -lopengl32 -lws2_32 -lz -limm32

LUA_SRC := $(filter-out third_party/lua-5.4.7/src/lua.c third_party/lua-5.4.7/src/luac.c,$(wildcard third_party/lua-5.4.7/src/*.c))
LUA_OBJ := $(LUA_SRC:.c=.o)
KCP_OBJ := third_party/kcp/ikcp.o
CPP_SRC := src/main.cpp src/engine.cpp src/gl_loader.cpp src/render.cpp src/sff.cpp src/air.cpp src/fighter.cpp src/lua_host.cpp src/cmd.cpp src/input.cpp src/net.cpp src/room_sync.cpp src/camera.cpp src/rollback.cpp src/stage.cpp src/title.cpp src/cns.cpp
CPP_OBJ := $(CPP_SRC:.cpp=.o)
HDRS := $(wildcard src/*.hpp)

.PHONY: all
all: ../bin_cpp/ikemen_cpp.exe ../bin_cpp/ikemen_relay.exe

MINGW_BIN = D:/tool/msys64/mingw64/bin

../bin_cpp/ikemen_cpp.exe: $(LUA_OBJ) $(KCP_OBJ) $(CPP_OBJ)
	if not exist ..\bin_cpp mkdir ..\bin_cpp
	$(CXX) -o $@ $^ $(LDFLAGS)
	-cmd /c copy /Y D:\tool\msys64\mingw64\bin\zlib1.dll ..\bin_cpp\
	-cmd /c copy /Y D:\tool\msys64\mingw64\bin\SDL2.dll ..\bin_cpp\
	-cmd /c copy /Y D:\tool\msys64\mingw64\bin\libgcc_s_seh-1.dll ..\bin_cpp\
	-cmd /c copy /Y D:\tool\msys64\mingw64\bin\libstdc++-6.dll ..\bin_cpp\
	-cmd /c copy /Y D:\tool\msys64\mingw64\bin\libwinpthread-1.dll ..\bin_cpp\

../bin_cpp/ikemen_relay.exe: src/relay_server.cpp src/relay_proto.hpp $(KCP_OBJ)
	if not exist ..\bin_cpp mkdir ..\bin_cpp
	$(CXX) -std=c++17 -O2 -I src -I third_party/kcp -o $@ src/relay_server.cpp $(KCP_OBJ) -lws2_32

src/%.o: src/%.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(LUA_OBJ) $(KCP_OBJ) $(CPP_OBJ) ../bin_cpp/ikemen_cpp.exe ../bin_cpp/ikemen_relay.exe
