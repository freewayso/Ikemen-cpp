# Ikemen C++ / Lua runtime (no ZSS)

Build from repo root with MSYS2 MinGW64:

```
export PATH=/mingw64/bin:/usr/bin:$PATH
cmake -S cpp -B cpp/build -G "MinGW Makefiles"
cmake --build cpp/build
./bin_cpp/ikemen_cpp.exe
```

Run from the Ikemen-GO repo root so `chars/kfm` and `cpp/lua` resolve.

- Local: `ikemen_cpp.exe`
- Delay host: `ikemen_cpp.exe --host`
- Delay client: `ikemen_cpp.exe --connect 127.0.0.1`
- Rollback UDP (predicted inputs): `ikemen_cpp.exe --rollback 127.0.0.1`

P1: WASD + J/K  P2: numpad
