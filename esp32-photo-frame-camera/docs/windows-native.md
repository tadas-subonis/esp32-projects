# Native Windows development

Develop and flash **directly on Windows** — no WSL or usbipd required. USB boards appear as `COM*` ports.

For WSL2 + usbipd instead, see [wsl-usb-flash.md](wsl-usb-flash.md).

## Windows `make`

From the repo root, use the **same target names and `VAR=value` syntax** as the Linux Makefile:

```powershell
make help
make deps
make build
make ports
make identify
make flash-papercolor PORT=COM8
make flash-cores3     PORT=COM7
make cmd-cores3       CMD=status CORES3_PORT=COM7
make logs-cores3      CORES3_PORT=COM7 SECONDS=8
make capture          CORES3_PORT=COM7
```

- **Implementation:** `make.cmd` → `scripts/make.ps1` → `scripts/devctl.py`
- **Agents:** start with `AGENTS.md` and this doc; run `make help` for targets.
- Works in **cmd.exe** and **PowerShell** (`.\make.cmd …` if GNU `make` from Git shadows `make.cmd`).
- `scripts/dev.ps1` is a backwards-compatible alias to `make.ps1`.

## One-time setup

Prerequisites:

- **Git for Windows** — [git-scm.com](https://git-scm.com/)
- **Python 3.10+** — [python.org](https://www.python.org/) (add to PATH)
- **PowerShell 5.1+** (included with Windows 10/11)

```powershell
make deps
```

This installs:

- **PlatformIO** (user pip) — CoreS3 Arduino build
- **ESP-IDF v5.5.1** at `%USERPROFILE%\esp\esp-idf` — PaperColor build
- **pyserial** — `devctl.py` serial tooling
- **Vendor reference clones** under `vendor/`

First ESP-IDF `install.ps1` run downloads toolchains (~several minutes).

Add PlatformIO to PATH for new shells (adjust Python version if needed):

```powershell
$env:Path = "$env:APPDATA\Python\Python313\Scripts;$env:Path"
```

## Agent workflow coverage

| Agent task | Windows command | Notes |
|------------|-----------------|-------|
| List ports | `make ports` | JSON via `devctl.py` |
| Identify board | `make identify` | Match `serial_number`, not COM index |
| Build both | `make build` | PlatformIO + ESP-IDF |
| Flash PaperColor | `make flash-papercolor PORT=COM8` | |
| Flash CoreS3 | `make flash-cores3 PORT=COM7` | On failure: `make flash-cores3-wait PORT=COM7` |
| Capture logs | `make logs-cores3 CORES3_PORT=COM7 SECONDS=8` | Non-interactive |
| Send command | `make cmd-cores3 CMD=status CORES3_PORT=COM7` | Returns `<<< {json}` result line |
| End-to-end photo | `make capture CORES3_PORT=COM7` | |
| HTTP smoke test | `make verify-photo-post` | Join `PhotoFrame` Wi‑Fi first |
| Exit download mode | `make wake PORT=COM7` | |
| Dev loop | `make dev-cores3 CORES3_PORT=COM7` | flash + short log capture |
| Interactive monitor | `make monitor-cores3 PORT=COM7` | **Close before** `cmd-*` / `logs-*` |

**Not on Windows (WSL-only):** `usb-status`, `usb-help`, `usb-attach`, `flash-cores3-windows` (usbipd bridge).

Full command reference: `make help` or [agent-tooling.md](agent-tooling.md).

## Find serial ports

```powershell
make ports
make identify
```

Use `serial_number` to tell boards apart (COM number can change):

| `serial_number` | Board |
|-----------------|-------|
| `44:1B:F6:C1:85:98` | PaperColor |
| `30:ED:A0:D4:BC:14` | CoreS3 |

## Flash

Power both boards via USB. Set `PORT=` to the correct `COM*`:

```powershell
make flash-papercolor PORT=COM8
make flash-cores3     PORT=COM7
```

**CoreS3 download mode:** if upload fails with "No serial data received", long-press **RST ~3s** (green LED), then:

```powershell
make flash-cores3-wait PORT=COM7
```

**PaperColor:** hold **RST** while plugging USB if the port does not appear.

## Make parity (WSL vs Windows)

| Linux / WSL | Windows |
|-------------|---------|
| `make deps` | `make deps` |
| `make build` | `make build` |
| `make flash-cores3 PORT=/dev/ttyACM1` | `make flash-cores3 PORT=COM7` |
| `make capture CORES3_PORT=…` | `make capture CORES3_PORT=COM7` |

Port syntax is the main difference (`COM*` vs `/dev/ttyACM*`).

## Troubleshooting

| Symptom | What to try |
|---------|-------------|
| `make` runs GNU make instead of `make.cmd` | Run `.\make.cmd …` or `cd` to repo root; uninstall/reorder PATH if Git make shadows |
| `pio` not found | Add `%APPDATA%\Python\Python313\Scripts` to PATH |
| `idf.py` not found | Run `. $env:USERPROFILE\esp\esp-idf\export.ps1`, or use `make` targets (auto-exports) |
| CoreS3 upload fails | Long-press RST ~3s → `make flash-cores3-wait PORT=COM7` |
| Wrong board on port | `make identify` — match `serial_number` |
| Port busy | Close PlatformIO monitor, idf monitor, or other serial tools |
| Bluetooth `COM3`–`COM6` noise | Ignore — filter by `vid` 0x303A (Espressif) in `identify` output |
