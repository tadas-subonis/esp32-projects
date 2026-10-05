# Agent tooling

Non-interactive loop: build, flash, send a command, capture `<<< {json}`.

```powershell
.\make flash
.\make flash-monitor
.\make cmd CMD=status
.\make cmd CMD="new 42"
.\make cmd CMD="angle 50"
.\make cmd CMD="power 72"
.\make cmd CMD=fire
.\make snap
.\make logs SECONDS=6 UNTIL="REPL started"
```

`.\make flash-monitor` writes the serial stream under `logs\` (override with `LOG=logs\run.log`). Do **not** leave `make monitor` / `flash-monitor` open on that port while using `make cmd` / `make snap`.

## Hardware feedback loop

The board already answers newline commands with one `<<< {json}` line. Use that as the oracle, then grab a picture when you need to see the LCD:

```powershell
.\make cmd CMD=status
.\make cmd CMD="new 42"
.\make snap
```

- **`status` / `state` / `new` / `fire` / `tick`** include `fps`, `compose_ms`, `flush_ms`, `frame_ms`, and `dirty_px` (last present). Idle frames should be overlay-only (`dirty_px` around 4560). A full-screen flush is only for map/title changes. Present techniques: [fps.md](fps.md).
- **`snap`** presents the current frame, then dumps a 4× downsampled 120×80 RGB888 payload (`>>>SNAP` … raw bytes … `<<<SNAP_END`). `.\make snap` writes a PNG under `logs\` (override with `OUT=logs\frame.png`). Transfer takes ~2.5 s at 115200; the match is locked for that window.
- Heartbeat logs (`hb phase=... fps=... frame=...ms`) every 5 s on UART. `.\make logs SECONDS=8` captures them without a command.

Close the serial monitor before `cmd` / `snap`. Full 480×320 dumps are not used — too slow on this UART.

## Device commands

| Command | Result |
|---------|--------|
| `status` / `state` | phase, seed, angle, power, wind, hp, heap, fps, compose/flush/frame ms, dirty_px, net, seat, ip, players |
| `version` | project + IDF version |
| `new [seed]` | start vs-bot match |
| `angle <deg>` | set aim |
| `power <n>` | set power 8–100 |
| `fire` | fire and resolve flight |
| `tick [n]` | advance n×16 ms (buttons still polled) |
| `snap` | 120×80 RGB888 dump after `>>>SNAP` (host saves PNG) |

Host vs-bot (no TCP):

```bash
./build-host/artillery-host --local
```

Same compose in a window (local vs-bot):

```bash
./build-host/artillery-emu --seed 42 --start
```

Two SDL windows against a local TCP server (lobby + auto-start PvP):

```powershell
.\make emu-pvp
.\make emu-cmd SEAT=0 CMD=status
.\make emu-cmd SEAT=0 CMD="angle 55"
.\make emu-cmd SEAT=0 CMD=fire
.\make emu-dump SEAT=0 OUT=logs\frame.png
.\make emu-play
.\make emu-cmd SEAT=0 CMD=status TIMEOUT_MS=2000
```

`TIMEOUT_MS` (default 2000) is a hard wall-clock limit for `emu-cmd` / `cli` — the process is killed if the control socket stalls.

**Play-to-end / debug loop (emulators):**

| Command | Purpose |
|---------|---------|
| `.\make emu-pvp` | server + two windows with control ports `17500` / `17501` |
| `.\make emu-cmd SEAT=N CMD=status` | phase, hp, wind, `winner` (−1 until gameover), `firing`, `proj_x/y` |
| `.\make emu-cmd SEAT=N CMD=fire` | fire as that seat (only when `active` matches) |
| `.\make emu-dump SEAT=N OUT=logs\frame.png` | full 480×320 PNG (PPM then convert) |
| `.\make emu-play` | autoplay both seats to `gameover`; writes `logs\play\session.log` + PNGs; on stuck dumps both seats |

Or manually: `artillery-server --port 17421`, then two `artillery-emu --remote --port 17421 --want pvp` (control ports `17500+seat`). `artillery-cli --emu 17500 --timeout-ms 2000 status` talks to the focused-seat control socket; keyboard still works when a window has focus.

Host equivalent against a TCP server:

```bash
./build-host/artillery-server --port 7420
./build-host/artillery-cli status
./build-host/artillery-cli start --seed 42 --mode bot
./build-host/artillery-cli angle 55
./build-host/artillery-cli fire
```

Golden physics without a live match:

```bash
./build-host/artillery-sim state --seed 42
./build-host/artillery-sim fire --seed 42 --angle 45 --power 70
```
