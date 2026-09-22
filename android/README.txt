Ikemen GO Android client (SDL2 + OpenGL ES 3)

Open this folder in Android Studio (File > Open > android/), not the repo root.

Need: JDK 17, Android SDK 35, NDK 26+, CMake 3.22+ (SDK Manager).

First Gradle sync downloads SDL2 2.30.11. Build a debug APK (arm64-v8a / x86_64 emulator).

Lobby and relay stay on PC/cloud. Edit data/net.ini in the repo before building so assets pick up Lobby/Relay IPs.

Touch in a match: left stick = WASD, J/K = punches. Login UI uses taps (same 1280x720 layout as desktop).
