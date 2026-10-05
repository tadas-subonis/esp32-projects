# ESP32-P4 handheld prototype

Minimal ESP-IDF bring-up for a cheap multiplayer handheld:

- **Board:** Waveshare ESP32-P4-WIFI6-POE-ETH Rev 2.0
- **Display:** 3.5" SPI TFT, 480×320, **ILI9488** RGB666 (Stage 0 done)
- **Now:** Stage 2 local artillery — [spec / report / flash notes](docs/stage2-local-artillery.md)

Bring-up notes: [`docs/notes.md`](docs/notes.md). Pin map: [`docs/wiring.md`](docs/wiring.md). Product plan: [`docs/console-plan.md`](docs/console-plan.md). Local artillery (report + spec + runbook): [`docs/stage2-local-artillery.md`](docs/stage2-local-artillery.md). P4Bench how-to: [`docs/p4bench.md`](docs/p4bench.md). Measured SPI performance: [`docs/p4bench-findings.md`](docs/p4bench-findings.md).

The whole app is **`firmware/main/main.c`** (bring-up modes) plus **`firmware/main/p4bench/`** when `TEST_MODE=4`. Pins, SPI clock, `MADCTL`, and `TEST_MODE` are at the top of `main.c`.

## Files

| File | Role |
|---|---|
| `firmware/main/main.c` | GPIOs, `TEST_MODE`, Stage 0/1 bring-up |
| `firmware/main/lcd_hw.c` | Shared ILI9488 SPI driver (P4Bench) |
| `firmware/main/p4bench/` | P4Bench suite — see [`docs/p4bench.md`](docs/p4bench.md) |
| `docs/p4bench-findings.md` | Measured SPI limits + renderer guidance |
| `firmware/main/CMakeLists.txt` | Component list |
| `firmware/main/idf_component.yml` | IDF version pin |
| `firmware/CMakeLists.txt` | ESP-IDF project |
| `firmware/sdkconfig.defaults` | `esp32p4` + UART console |
| `make.cmd` / `scripts/make.ps1` | Windows `make` wrapper |

## Wiring (working hookup)

Control signals are on the **right** column of the 40-pin header (5V at the top).

| TFT | ESP32-P4 |
|---|---|
| VCC | 5 V |
| GND | GND |
| LED | 3.3 V (left-column 3V3) |
| CS | GPIO22 |
| RESET | GPIO5 |
| DC/RS | GPIO4 |
| SDI/MOSI | GPIO36 |
| SCK | GPIO32 |
| SDO/MISO | not connected |
| touch / TF | not connected |

Use the **Type-C** port for flash/serial, not Type-A.

Buttons are on the **left** column. Each switch: GPIO to GND.

| Button | GPIO |
|---|---|
| UP | GPIO20 |
| DOWN | GPIO6 |
| LEFT | GPIO3 |
| RIGHT | GPIO2 |
| A | GPIO33 |
| B | GPIO26 |
| SELECT | GPIO48 |
| START | GPIO47 |

## Build / flash (PowerShell)

From the repo root. Use `.\make.ps1` so Git's `make.exe` is not used. Plug in **Type-C**.

One-shot build + flash (picks the COM port):

```powershell
cd C:\Work\my\esp32-debug-program
.\make.ps1 flash
```

Force a port if several COM devices are present:

```powershell
.\make.ps1 flash PORT=COM8
```

First time on this machine, run `.\make.ps1 deps` once first.

`.\make.cmd …` is the same wrapper. Plug the board in with **Type-C**, not Type-A.

| Target | What it does |
|---|---|
| `.\make.ps1 deps` | Install ESP-IDF for esp32p4 |
| `.\make.ps1 build` | Compile `firmware/` |
| `.\make.ps1 ports` | List COM ports |
| `.\make.ps1 flash` | **Build + flash** (auto COM port, or `PORT=COMx`) |
| `.\make.ps1 monitor PORT=COMx` | Interactive serial (keyboard → UART) + `logs\` |
| `.\make.ps1 flash-monitor PORT=COMx` | Flash, then interactive monitor |
| `.\make.ps1 logs PORT=COMx` | Capture ~8 s of logs |
| `.\make.ps1 clean` | Delete the IDF build tree |
| `.\make.ps1 help` | Print this list |

## What you should see

**TEST_MODE 4 (default):** P4Bench — serial console performance suite. See [`docs/p4bench.md`](docs/p4bench.md). Checker pattern on boot, then type `help`.

**TEST_MODE 3:** black screen titled `BUTTONS`. Eight rows `UP DOWN LEFT RIGHT A B SELECT START` with `[0]`/`[1]` and a green box while held. Wire each listed GPIO through a switch to GND.

**TEST_MODE 1:** full-screen red → green → blue → black → white with corner squares. Git tag `hw-lcd-working`.

`TEST_MODE` in `main.c`: `4` = P4Bench (default), `3` = buttons, `1` = LCD color bars, `0` = GPIO pin walk, `2` = controller ID read.
