# Ikemen C++ 运行时 — 网络与大厅技术文档

Word 版（便于传阅与验收）：`cpp/docs/Ikemen-Cpp-Network-Tech.docx`  
本文为同一内容的 Markdown 源稿。实现目录：`cpp/`。协议：`cpp/proto/lobby.proto`。

## 1. 目标与边界

- 玩法侧用 **C++ 引擎 + Lua 角色逻辑** 对齐 Ikemen GO 的战斗循环。
- 联网分两段：
  - **大厅**：TCP + Protocol Buffers 3，负责登录、房间列表、创建/加入（每房最多 2 人）。
  - **对战**：UDP + KCP 帧同步，由 relay 按房间隔离确认帧、历史与追帧。
- 大厅消息风格参考 [luabus](https://github.com/trumanzhao/luabus) 的「命名消息 + 主循环 wait」，序列化未接入 luna/lbus，而是 **proto3 线格式**（自实现编解码，不依赖 `libprotobuf`）。

不在本文范围：完整 GGPO 回滚、ZSS 编译器、云防火墙开通步骤以外的运维细节。

## 2. 进程与端口

| 进程 | 产物 | 传输 | 默认端口 | 职责 |
|------|------|------|----------|------|
| `ikemen_lobby` | `bin_cpp/ikemen_lobby` | TCP | 8080 | 登录、房间管理、START 通知 |
| `ikemen_relay` | `bin_cpp/ikemen_relay` | UDP | 9000 | 多房间 KCP 帧同步 |
| `ikemen_cpp` | `bin_cpp/ikemen_cpp` | 上述两者 | 客户端 | 登录 UI → 房间列表 → 对战 |

云上需同时放行 **TCP 8080** 与 **UDP 9000**。

```
客户端 A ──TCP proto3──► lobby ──房间列表 / START──► 客户端 A/B
客户端 A ──UDP KCP IKFS─► relay ◄── 客户端 B
```

## 3. 客户端流程

普通启动（无 `--host` / `--connect`）：

1. 读 `data/net.ini`，连接 `Lobby:LobbyPort`。
2. 进入 **LOGIN**：填 `USER`（密码可选），点 **LOGIN** / **REGISTER**。
3. 注册成功后自动登录，进入 **房间列表**（实时 `S2C_ROOM_LIST`）。
4. **C** 创建或选 `WAIT` 房 **Enter** 加入；满 2 人后双方收到 `START`。
5. 用大厅房间 id（8 字符，如 `r0000001`）连 relay，KCP 开战。
6. 对战结束或 ESC：`LEAVE` 大厅房间，回到房间列表。登录页 ESC 退出游戏。

命令行覆盖（不写死 IP）：

```text
ikemen_cpp --lobby 127.0.0.1:8080 --relay 127.0.0.1:9000
ikemen_cpp --host          # 跳过大厅，按 net.ini Room 当 host 进 relay
ikemen_cpp --connect       # 跳过大厅，当 guest
```

主菜单 **NETWORK → HOST/JOIN GAME** 仍走固定 `Room=`，用于本地快速测；正式匹配走登录后的房间列表。

## 4. 配置 `data/net.ini`

仓库的 `data/*` 默认被 gitignore，本地/云上自行放置：

```ini
[Net]
Relay=127.0.0.1
Port=9000
Room=kfm1
Lobby=127.0.0.1
LobbyPort=8080
```

`Relay` 可写成 `ip:port`。未单独配 `Lobby` 时，会沿用 `Relay` 的 IP。

## 5. 大厅协议（proto3 / TCP）

定义：`cpp/proto/lobby.proto`。实现：`cpp/src/lobby_proto.hpp`、`lobby.cpp`、`lobby_server.cpp`。

### 5.1 帧

```
[uint32 little-endian 载荷长度][Envelope proto3 字节]
```

单帧载荷上限 **65536**。`Envelope.type` 为消息号，`Envelope.body` 为对应 message 的序列化结果。

### 5.2 消息号

| type | 方向 | body | 说明 |
|------|------|------|------|
| 1 C2S_REGISTER | C→S | AuthReq | 注册，密码可空 |
| 2 C2S_LOGIN | C→S | AuthReq | 登录 |
| 3 C2S_LIST | C→S | 空 | 拉房间列表 |
| 4 C2S_CREATE | C→S | 空 | 创建房间，创建者 role=0 |
| 5 C2S_JOIN | C→S | JoinReq | 加入 `room_id` |
| 6 C2S_LEAVE | C→S | 空 | 离开当前房 |
| 10 S2C_HELLO | S→C | Hello | 接入后立即发送 |
| 11 S2C_ERROR | S→C | Error | `code` + `message` |
| 12 S2C_LOGIN_OK | S→C | UserOk | |
| 13 S2C_REGISTER_OK | S→C | UserOk | 客户端随后自动 LOGIN |
| 14 S2C_ROOM_LIST | S→C | RoomList | 全量快照 |
| 15 S2C_JOIN_OK | S→C | JoinOk | `role`：0 host / 1 guest |
| 16 S2C_START | S→C | JoinOk | 两人到齐，开始连 relay |
| 17 S2C_LEAVE_OK | S→C | 空 | |

### 5.3 RoomInfo

- `id`：`r` + 7 位序号，恰好 8 字符，与 relay 房间键对齐。
- `status`：`0 = WAIT`（可加入），`1 = FIGHT`（拒加入，`ERR FIGHT`）。
- `players` / `max_players`：最多 **2**。
- `host`：创建者用户名。

列表推送时机：登录成功、创建/加入/离开、对端掉线。未进房的客户端约 **1.5s** 再发 `C2S_LIST`。已登录用户都会收到广播。

### 5.4 错误码（Error.code）

`BAD` 非法用户名、`EXISTS` 已注册、`AUTH` 登录失败、`ONLINE` 已在线、`NEEDLOGIN`、`NOROOM`、`FULL`、`FIGHT`、`MAX` 房间数上限、`CMD` 未知命令。

### 5.5 限额

- 用户名：1–16，字母数字下划线。
- 密码：0–32（可空）。
- 大厅房间数：**64**。
- 每房人数：**2**。

## 6. 账号存档 `data/users.pb`

不把明文密码写入文本。文件布局：

```
"IKUS" | uint32le(len) | UserStore proto3 | sha256(UserStore)
```

`Account`：`user`、16 字节 `salt`、`pass_hash = sha256(salt || pass)`。校验失败则拒绝加载。

若没有 `users.pb`，会尝试导入旧版 `data/users.txt`（`user[ pass]` 每行）并写成 pb。之后只读写 pb。

启动：

```text
ikemen_lobby [port=8080] [usersPath=data/users.pb]
```

## 7. 对战协议 IKFS（UDP / KCP）

定义：`cpp/src/relay_proto.hpp`。服务：`relay_server.cpp`。客户端：`room_sync.cpp`。

### 7.1 KCP

- 同一 UDP 端口上多房间并存。
- conversation：`0x4B460000 | (FNV-1a(room[8]) & 0x7FFF)<<1 | role`，role 0=host、1=guest。
- 会话按 **UDP 源地址** 索引；房间状态按 **8 字节 room 键** 隔离。

### 7.2 应用层头（32 字节，小端）

| 偏移 | 长度 | 字段 |
|------|------|------|
| 0 | 4 | magic `IKFS` |
| 4 | 1 | cmd |
| 5 | 1 | role |
| 6 | 8 | room |
| 14 | 4 | frame |
| 18 | 4 | a（INPUT=本端按键；CONFIRM=P0） |
| 22 | 4 | b（CONFIRM=P1） |
| 26 | 4 | seed |
| 30 | 2 | count（CATCHUP_PACK 条数） |

cmd：`JOIN=10` `JOIN_OK=11` `INPUT=12` `CONFIRM=13` `CATCHUP=14` `CATCHUP_PACK=15` `DROP=16`。

`CATCHUP_PACK` 额外载荷：每条 12 字节 `(frame, i0, i1)` u32le，最多 40 条。Relay 保留约 **3600** 帧历史。空闲约 **90s** 超时 DROP。同时进行的对战场次上限 **128**。双方都离开后删除该房间状态。

兼容旧 **IKRL** 13 字节头的 UDP 转发仍保留，正式对战走 IKFS。

## 8. 战斗同步要点

- 双方每帧上传本端 input；relay 凑齐 P0+P1 后广播 `CONFIRM`。
- 丢帧/重连：客户端 `CATCHUP`，用历史包快进模拟到当前确认帧。
- HUD 在对战中显示 FPS、PING、帧号。
- 角色逻辑在 `cpp/lua/chars/`；引擎 `cpp/src/engine.cpp`。

## 9. 编译

Windows（MinGW，`cpp/`）：

```text
mingw32-make game relay lobby
```

Linux 默认可只编服务端：

```text
make          # relay + lobby
make IKEMEN_RELAY_ONLY  # 见 CMake
```

CMake：`cpp/CMakeLists.txt`。游戏依赖 SDL2、OpenGL、zlib、Lua 5.4.7、KCP。

Linux 打包脚本：`bin_cpp/_pack_relay.sh`（relay + lobby）。云上：`run.sh` / `run_lobby.sh`。

## 10. 本地联调

仓库根目录：

```text
bin_cpp/ikemen_lobby 8080 data/users.pb
bin_cpp/ikemen_relay 9000
bin_cpp/ikemen_cpp --lobby 127.0.0.1:8080 --relay 127.0.0.1:9000
```

两客户端用**不同用户名**注册/登录；一方创建，另一方加入。

## 11. 关键源码

| 路径 | 内容 |
|------|------|
| `cpp/proto/lobby.proto` | 大厅与存档 schema |
| `cpp/src/lobby_proto.hpp` | proto3 编解码 |
| `cpp/src/lobby_server.cpp` | 大厅服务 |
| `cpp/src/lobby.cpp` | 客户端 TCP |
| `cpp/src/sha256.hpp` | 密码与文件校验 |
| `cpp/src/relay_proto.hpp` | IKFS / KCP conv |
| `cpp/src/relay_server.cpp` | 多房间 relay |
| `cpp/src/room_sync.cpp` | 客户端帧同步 |
| `cpp/src/ini.hpp` | net.ini |
| `cpp/src/engine.cpp` | 画面流程与开战 |
| `cpp/src/title.cpp` | 登录 / 房间 UI |

## 12. 已知限制

- 登录字体为点阵 A–Z / 数字，界面文案为英文。
- 账号哈希不是慢哈希（无 bcrypt/argon2），文件权限仍需操作系统保护。
- `data/users.pb`、日志、二进制默认不进 git。
- 未编译官方 luabus/`luna`；若以后要 Lua 侧总线，应对齐同一套 `Msg` 与帧格式。
