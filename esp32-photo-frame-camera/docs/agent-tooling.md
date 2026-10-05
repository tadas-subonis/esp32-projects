# Agent tooling (build/flash/logs/commands)

Agent-friendly loop: build, flash, capture logs, and send commands **without interactive serial monitors**.

- **Windows (default):** `COM*` ports — `make …` from repo root (`make.cmd` → `scripts/make.ps1`). See [windows-native.md](windows-native.md).
- **WSL:** `/dev/ttyACM*` — `make …` via repo `Makefile`. USB via [wsl-usb-flash.md](wsl-usb-flash.md).

Both hosts use the same **`make` target names** and **`VAR=value` syntax**; only the port string changes.

- **CoreS3** (camera sender, PlatformIO Arduino): logs + commands over USB. Flash may require download mode — `make flash-cores3-wait`.
- **PaperColor** (e-ink receiver, ESP-IDF): logs + commands over USB Serial/JTAG.

The common contract is:

- Send a newline-terminated command, e.g. `capture\n`.
- Device emits regular logs.
- Device ends with a single machine-readable result line:

```text
<<< {"cmd":"capture","ok":true,"http":202,"jpeg_bytes":12345,"w":320,"h":240,"ms":840}
```

## Quickstart

**Windows:**

```powershell
make deps
make ports
make identify
make flash-papercolor PORT=COM8
make flash-cores3     PORT=COM7
make capture          CORES3_PORT=COM7
```

**WSL:**

```bash
make deps
make ports
make identify
make flash-papercolor PORT=/dev/ttyACM0
make flash-cores3     PORT=/dev/ttyACM1
make capture CORES3_PORT=/dev/ttyACM1
```

## Make targets (both hosts)

| Target | Purpose |
|--------|---------|
| `make ports` | List serial ports (JSON) |
| `make identify` | Probe ports, guess device kind (JSON) |
| `make build` | Build both firmwares |
| `make flash-cores3` | Build + upload CoreS3 |
| `make flash-cores3-wait` | Retry flash until esptool connects |
| `make flash-papercolor` | Build + flash PaperColor |
| `make cmd-cores3 CMD=status` | Send command, capture `<<< {json}` result |
| `make cmd-papercolor CMD=status` | Same for PaperColor |
| `make logs-cores3 SECONDS=5` | Timed log capture |
| `make logs-papercolor SECONDS=5` | Timed log capture |
| `make capture` | CoreS3: capture JPEG + POST to PaperColor |
| `make rawlogs RAW=1` | Raw byte log dump |
| `make dev-cores3` | Flash CoreS3 + short log capture |
| `make dev-papercolor` | Flash PaperColor + short log capture |
| `make verify-photo-post` | HTTP POST golden JPEG (join `PhotoFrame` Wi‑Fi) |
| `make wake` | `watchdog_reset` — exit ROM download mode |
| `make monitor-cores3` | Interactive monitor (**close before cmd/logs**) |
| `make monitor-papercolor` | Interactive monitor |
| `make clean` | Clean build artifacts |
| `make help` | List targets |

**WSL-only:** `make usb-status`, `make usb-help`, `make usb-attach`, `make flash-cores3-windows`.

### Examples

```powershell
# Windows
make cmd-cores3     CMD=status CORES3_PORT=COM7
make logs-papercolor PAPERCOLOR_PORT=COM8 SECONDS=8 UNTIL="listening on"
```

```bash
# WSL
make cmd-cores3     CMD="status" CORES3_PORT=/dev/ttyACM1
make logs-papercolor PAPERCOLOR_PORT=/dev/ttyACM0 SECONDS=8
```

Implementation: `scripts/devctl.py` (pyserial). Optional log files: `OUT=path`.

## Device commands

### CoreS3 (`cores3/`)

- `help`
- `capture`: capture JPEG and POST it to PaperColor (`/api/v1/photo`)
- `status`: Wi-Fi/IP/heap/uptime summary
- `wifi`: reconnect to PhotoFrame AP
- `ping`: TCP connect to PaperColor host:80

### PaperColor (`papercolor/`)

- `help`
- `status`: SD mounted/inserted, photo count, refresh busy, current index, heap, uptime, AP client count
- `home`: force e-ink status/home screen refresh (recovery when panel looks stuck)
- `gallery`: list all `/data` entries + image count / in-memory gallery index
- `sdtest`: write/read/delete a tiny file on SD (bus-lock smoke test)
- `epdtest`: draw a colour-bar test pattern and report the refresh duration in ms
- `pins`: read-only EPD pin levels (`busy_pin` 1 = ready, `dc_pin`, `cs_pin`)
- `display <index>`: show a photo by index
- `loglevel <tag> <level>`: set runtime log level for a tag (`none|error|warn|info|debug|verbose`)
- `version`: project/version/IDF version

## Important: serial port ownership

Only one process can own a serial port at a time.

- If you run `make monitor-*`, close it before running `make cmd-*` / `make logs-*`.
- `devctl` will fail if the port is busy.

## Identifying which port is which

```text
make identify
```

Prints JSON per port with:

- `serial_number` — stable MAC-based ID (use this when COM / ttyACM indices swap)
- `sniff` — recent boot/log lines captured before probing
- `kind` — best-effort: `papercolor`, `cores3`, `papercolor_fw_psram_error`, or `unknown`
- `probe` — parsed `<<< {json}` from `version` / `status` if the device answered

If `sniff` shows `waiting for download`, press **RESET** while `make logs-*` is open, or `make wake PORT=…`, then re-run `make identify`.

### Stable mapping (this hardware pair)

| `serial_number` | Board | Typical Windows | Typical WSL |
|-----------------|-------|-----------------|-------------|
| `44:1B:F6:C1:85:98` | PaperColor | `COM8` (varies) | `/dev/ttyACM0` |
| `30:ED:A0:D4:BC:14` | CoreS3 | `COM7` (varies) | `/dev/ttyACM1` |

Always match on `serial_number`, not the port name.

## Troubleshooting

| Symptom | Likely cause | What to try |
|---------|----------------|-------------|
| `kind: unknown`, `waiting for download` | ROM download mode | RESET during `make logs-*`; or `make wake PORT=…` |
| `papercolor_fw_psram_error` in sniff | PaperColor firmware on CoreS3 | `make flash-cores3 PORT=…` on that port |
| No `cores3: boot` in logs | Wrong firmware / hung boot | Re-flash CoreS3 |
| `make cmd-*` write timeout (PaperColor) | Stale firmware | Re-flash PaperColor |
| CoreS3 upload fails | Not in download mode | Long-press **RST ~3s** → `make flash-cores3-wait PORT=…` |
| Only one board in `make ports` (Windows) | Cable / power / driver | Replug USB; ignore Bluetooth COM ports; filter `vid` 0x303A |
| Only one board in `make ports` (WSL) | usbipd detach | `make usb-status`; re-attach via usbipd-win |
| `make` runs wrong tool (Windows) | GNU make from Git on PATH | Use `.\make.cmd …` from repo root |
| PaperColor screen never updates; `M5.begin took ~40000ms`; `pins` shows `busy_pin: 0` | Panel asleep and/or EPD rail down, so M5GFX burns a 20s `_wait_busy` timeout per wait | Rails + RST must be set up **before** `M5.begin()` — see *PaperColor e-ink pitfalls* below |

## PaperColor e-ink pitfalls

Two hardware behaviours will silently make the panel look dead. Both are handled in
`DeviceHal::bring_up_power_rails()`, which **must** run before `M5.begin()`.

1. **PMIC rails are off after every reset.** `PWR_CFG` (reg `0x06`) auto-clears, and the EPD
   rail is off by default. Enable charge/DCDC/LDO **and boost** — boost is the ~15V rail the
   e-paper needs to switch pixels. Also set `HOLD_CFG` (reg `0x07`) so the rails survive a USB
   unplug. PMIC is at I2C `0x6E` on SDA=GPIO3 / SCL=GPIO2.
2. **The panel is never hardware-reset.** `cfg.clear_display = false` (used by M5Stack's own
   example to avoid a boot flash) makes M5Unified call `Display.init_without_reset(false)`, so
   M5GFX's PaperColor branch runs `_pin_reset(GPIO12, false)`, which skips the reset pulse. A
   panel left asleep keeps `BUSY` low across ESP32 soft resets, so pulse RST (GPIO12) manually.

Diagnosing: `BUSY` is **GPIO11, active-low** (1 = ready). When it is stuck at 0 every M5GFX
`_wait_busy()` burns its full 20s timeout — `M5.begin()` takes ~40s (2 waits) and a refresh up
to 60s (3 waits), which looks like a hang but is not. Never gate firmware work on this pin;
M5GFX already bounds its own wait. Healthy numbers: `M5.begin()` well under 1s, full colour
refresh ~17s (`epdtest` reports it).

The e-ink panel and the microSD card share **SPI2**, and LovyanGFX drives the panel with direct
register access rather than the IDF SPI driver, so it does not participate in the driver's
arbitration with `sdspi`. Every SD access and every panel push must be wrapped in
`g_hal.lock_bus()` / `unlock_bus()`.
