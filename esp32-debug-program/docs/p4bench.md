# P4Bench

ESP32-P4 handheld performance suite for the Waveshare P4 + ILI9488 SPI prototype.

Measures **simulation**, **render**, and **LCD transfer** separately so a single FPS number is never the whole story.

## Build / flash

Same as the rest of this repo. `TEST_MODE` in `firmware/main/main.c` must be **4**.

```powershell
cd C:\Work\my\esp32-debug-program
.\make.ps1 flash
.\make.ps1 monitor PORT=COM5
```

`monitor` is interactive: your keyboard goes to the board (needed for `spisweep` → `s`/`p`/`u`). Logs are still saved under `logs\`.

Type-C UART only. After boot you should see a checker pattern briefly, then:

```text
P4Bench ready. Type 'help'.
>
```

## Commands

| Command | Purpose |
|---|---|
| `help` | Command list |
| `list` | Tests |
| `meta` | Chip / SPI / heap metadata |
| `run <test>` | Start a test |
| `stop` | Stop |
| `report` | Human + JSONL summary |
| `reset` | Clear metrics |
| `chunks` | LCD DMA chunk-size sweep |
| `crossover [secs]` | Dirty vs full FPS sweep (M4) |
| `spisweep` | SPI clock sweep: throughput + dirty + **animated visual proof**; type `s`/`p`/`u` |
| `spisweep auto` | Same without prompt (TX_OK only — still watch the LCD) |
| `endurance [min]` | Long SPI stress @ current clock (default 45). Patterns cycle; `q` aborts. Host suite: `scripts/p4bench_endurance.py` |
| `set sprites N` / `particles` / `enemies` / … | Tunables |
| `set mode full\|dirty\|direct` | Renderer mode |
| `set chunk_rows N` | Rows per SPI DMA burst |
| `set dirty_pct N` | Dirty crossover workload |
| `set overlay on\|off` | HUD |
| `auto sprites\|particles\|simulation\|td [fps] [secs]` | Auto-stress |

### Tests

| Name | What it measures |
|---|---|
| `pattern` | Checker / corner correctness |
| `lcd` | Raw LCD patterns + throughput |
| `memory` | FB clear / memcpy / fill |
| `sprites` | Bouncing sprites |
| `dirty` | Full-frame vs dirty-region crossover |
| `particles` | Particle sim + draw |
| `tiles` | Scrolling tile map |
| `compose` | Opaque / key / alpha / scale / rotate |
| `simulation` | Headless tower-defense (no draw) |
| `td` | Visual TD |
| `chaos` | Combined load |

### Example session

```text
meta
chunks
run lcd
report
stop

set sprites 200
set sprite_w 32
set sprite_h 32
set mode dirty
run sprites
report

set dirty_pct 20
set mode dirty
run dirty
report
set mode full
run dirty
report

auto sprites 30 3
auto particles 30 3
auto simulation 60 2
auto td 20 3

set enemies 500
set towers 50
set projectiles 400
set particles 1000
run chaos
```

## Output format

Human block:

```text
=== P4BENCH RESULT ===
...
Simulation:    x.xx ms
Render:        x.xx ms
LCD:           x.xx ms
```

Machine-readable line (grep `JSONL`):

```text
JSONL {"test":"sprites","fps_avg":32.8,...}
```

## Architecture notes

- Framebuffer is **RGB565 in PSRAM** (307,200 bytes).
- ILI9488 wire format is **RGB666** → **3 bytes/pixel** → full frame **460,800 wire bytes**.
- SPI default **60 MHz** on this unit (eye-STABLE overclock; ILI9488 datasheet write max 20 MHz). `spisweep` marked 80 MHz `UNSTABLE`. Do not treat a faster but corrupted mode as a win.
- Pixel path reuses the known-good init: `0x2C` then `0x3C`, pins CS22 RST5 DC4 MOSI36 SCK32.

## Findings

After hardware runs, results live in [`docs/p4bench-findings.md`](p4bench-findings.md). Full automated capture: `python scripts/p4bench_full_capture.py COM5`. Do not invent numbers.
