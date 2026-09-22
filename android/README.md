# Android 客户端

用 Android Studio 打开本目录（`android/`），不要打开仓库根。

完整操作、部署和对战截图说明见仓库 **[cpp/README.md](../cpp/README.md)**。

要点：

- minSdk 24 / target 35 / arm64-v8a，SDL2 + GLES3，与 `cpp/src` 同一套引擎。
- 大厅和对战中继跑在 PC：TCP 8080 + UDP 9000。
- 登录页可改 Lobby / Relay IP；对战页左侧遥感 = WASD，右侧 J/K 出拳，C 为满怒火炮。
- Gradle 同步资源：`chars/kfm`、`cpp/lua`、`data/net.ini`。
- 本机 JDK 路径写在 `local.properties` / 环境变量，不要写进 `gradle.properties`。
