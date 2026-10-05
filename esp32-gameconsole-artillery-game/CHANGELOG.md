# Changelog

## [Unreleased]

### Changed

- **UDP v2 session (replaces JSON-over-TCP)** — protocol version 2. `artillery-server`, host proxy/CLI, and firmware speak packed little-endian datagrams with ack bitfields and reliable-message repetition (lost Fire/Impact recover on the next ~50 ms tick without lwIP’s TCP RTO). Aim is an unreliable absolute angle/power sample. Join/resync send a binary height baseline; impacts send crater column deltas. Emulator control ports stay JSON-over-TCP.

### Fixed

- **Slow / unreliable PvP join** — hello no longer switches the socket to blocking with a 4 s `SO_RCVTIMEO` (that stretched “20 × 250 ms” waits into tens of seconds). Connect stays nonblocking; welcome poll budget is ~2.5 s. Soft reconnect backoff is 200 ms→2 s instead of 1–15 s. `esp_wifi_disconnect` runs only on the `net` task (game-task hosted RPC was a C6 path footgun). Server drops sockets that never hello within 3 s.
- **Online aim input queue** — held D-pad sent a nudge every frame (~60/s) into the command log, so aim kept crawling after release. Online nudges are rate-limited (~48 ms) with local-style step ramp; PvP ignores aim/fire when it is not your seat. Replica folds consecutive same-axis nudges when applying a backlog.

### Added

- **Correlatable net logs** — hello/welcome/error carry `eid` + `t_ms`. Firmware UART and `artillery-server` stderr both print `eid=… t_ms=…` so LAN stalls can be cross-referenced (`shared/protocol/event_trace.hpp`).
- **`flash-monitor`** — build, flash, then serial on screen and under `logs\` (same as the debug program). `.\make flash-monitor` auto-picks the Type-C CH343 port; `LOG=logs\run.log` overrides the file.
- **On-screen debug corner** — bottom-left `FPS` plus last/held button label (`A`, `UP`, `U D A`, …) on device and `artillery-emu`.
- **Match serial logs** — UART is unbuffered; Type-C console prints button edges, serial commands, bot shots, impacts, and a 5s heartbeat.
- **Hardware feedback loop** — `status` JSON now includes `fps`, `compose_ms`, `flush_ms`, `frame_ms`, `dirty_px`. `snap` dumps a 4× downsampled 120×80 RGB888 frame over UART; `.\make snap` writes a PNG under `logs\`.
- **Snappy present path** — ILI9488 SPI at 40 MHz with 16-line DMA strips; HEX PSRAM at 80 MHz. Idle frames flush only the debug overlay; aiming flushes HUD + tanks; full 480×320 is the map/title fallback. Write-up: `docs/fps.md`.
- **SPI flush no longer tears** — ping-pong DMA strip buffers so a queued color transfer is not overwritten mid-flight (that was freezing garbage into dirty-rect frames).
- **Live match is 30% slower** — `Match::tick` uses 7/10 of wall dt (shots, bot think, aim repeat). Present still wakes every 16 ms. Golden `simulate_shot` is unchanged.
- **Easier vs-bot** — the local AI aims wide of the tank (practice dummy), not a 48-sample sniper. It reuses last shot power instead of rolling a new strength every turn.
- **Aim power and angle stick per player** — each tank starts its next turn with the power and angle it last fired.
- **Handheld TCP client** — firmware joins `A1-3E323899-24G`, talks JSON-lines to `artillery-server` on this PC (`192.168.8.229:7420`), `want=pvp`. Two boards are seats 0 and 1; A on the title screen starts the duel. Offline, the local `Match` still runs. Wi-Fi credentials stay in `.local.env` (gitignored).
- **Command-log resync** — `state = reduce(state, command)` with a `seq`. Disconnects hold the PvP seat (`token` on welcome). Clients restore a snapshot then replay cmds; the shell animates locally (`step_fx`) until the next keyframe snapshot. Aim nudges are small `cmd` lines, not a full 8 KB state flood.
- **Online lobby title** — title stays VS BOT / HOTSEAT until you choose. **VS BOT** is local (no Wi-Fi). **HOTSEAT** enables Wi-Fi and joins the TCP server (CONNECTING → WAITING → STARTING). **B** cancels online and returns to the local menu. Snapshots carry `players`; a second hello (or a disconnect) is broadcast so both screens stay in sync. The match auto-starts once two seats are present. Clients parse `type:error` (e.g. `server_full`), show SERVER FULL, and back off instead of reconnect-spamming. Token reclaim works even if the previous TCP socket is still marked present.
- **Desktop PvP emu** — `artillery-emu --remote --want pvp` is the same RGB565 client as firmware. `.\make emu-pvp` opens two windows against a local server on port 17421. Each window listens on a control port (`17500` / `17501`); `.\make emu-cmd SEAT=0 CMD=status` / `fire` / `dump logs\x.ppm` drives it without keyboard focus. `.\make emu-play` autoplays to `gameover` with `logs\play\session.log` + PNG dumps on failure; `.\make emu-dump` writes a PNG. Control `status` includes `winner`, `turn`, `firing`, `proj_x/y`; `dump` ACKs only after the PPM is on disk.

### Fixed

- **Cohesive sprite art** — tanks, cloud, sun, shell, explosion, and grass now share one deterministic palette, outline weight, and pixel scale. Replaced noisy AI crops that included transparent holes and unrelated edge fragments.
- **Remote aim cmds were ignored** — JSON key lookup matched `"cmd"` inside `"type":"cmd"`, so angle/power nudges never updated the replica. Only keyframe snapshots (start/fire) applied. Aiming looks frozen until you fire.
- **Emu freeze on aim hold** — remote `submit` waited/slept for a server ack on the render thread, so holding left/right stalled the window. Input submit is fire-and-forget again; only `emu-cmd` uses a 150 ms wait. `artillery-cli` / `make emu-cmd` now hard-timeout (default 2 s) and return after the first reply line instead of waiting out the full timeout for a second line.
- **Gameover `winner` missing on clients** — snapshots encoded `have_winner` but the codec never parsed it, so control `status` reported `winner:-1` after a finished match.
- **ESP-Hosted reboot loop** — `esp_wifi_connect()` no longer runs on the `sys_evt` task. TCP connect was overflowing the net task with an 8 KB stack buffer; lines are parsed from the existing `rx_` member instead.

### Docs

- **POC hardware is ILI9488, not ST7796** — `docs/hardware.md` matches the working debug-program pinout.

### Added

- **Windows native host toolchain** — `make test` / `make play` / `make server` use MSVC + CMake/Ninja instead of WSL g++. TCP helpers speak Winsock on Windows.
- **Desktop emulator** — `artillery-emu` (`.\make emu` in PowerShell) blits the same 480×320 RGB565 compose as firmware into an SDL2 window; keyboard maps to the six buttons. SDL2 is fetched into `build-host/` (static link).
- **Readable title screen** — 3x5 A–Z glyphs; title draws `TANK DUEL` / `VS BOT` / `HOTSEAT` instead of overlapping placeholder bars.

### Docs

- **Game stack decision** — recorded why this repo keeps portable `game/` + dirty-rect ST7796 instead of device-side SDL3 / PPA scaling (`docs/game-stack.md`). Desktop SDL is `artillery-emu`, not the P4.

### Added

- **In-process local server** — `Authority` is the programmatic server (join/submit/tick). `ServerProxy` picks local (same process, vs-bot) or remote TCP. `make play` no longer needs a socket.
- **Authoritative TCP server** — `artillery-server` owns `Match`; clients send JSON-line intents (`hello`/`start`/`angle`/`power`/`fire`). Default port 7420. `artillery-host` is a thin client; `artillery-cli` sends one command. `Match::apply` rejects illegal fire (`not_your_turn`, `not_aiming`, …).
- **Tank Duel C++ core** — portable sim in `game/`: procedural heightmap, wind-affected ballistics, splash damage, destructible craters, hotseat / vs-bot / pvp match phases, greedy bot.
- **480×320 RGB565 renderer** — dirty rectangles for the projectile, full compose when terrain/HUD changes; title + HUD + tanks.
- **Host tools** — `artillery-sim` JSON CLI (golden physics) plus the TCP client/CLI above.
- **ESP-IDF firmware** — ESP32-P4 target, ST7796 SPI HAL (console-plan GPIOs), six-button input, USB Serial/JTAG console with `<<< {json}` results (offline local `Match`).
- **Agent tooling** — Windows `make.cmd` / `scripts/make.ps1`, `scripts/devctl.py`, docs matching the photo-frame loop.
