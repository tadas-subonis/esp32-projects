# Native Windows development

USB boards appear as `COM*` ports. Host C++ (tests, server, terminal play, SDL emulator) uses the **local MSVC toolchain** — Visual Studio Build Tools + CMake/Ninja. Firmware still uses ESP-IDF.

PowerShell does not run `make` from the current directory. Prefix with `.\`:

```powershell
.\make help
.\make deps
.\make test
.\make play
.\make emu
.\make server
.\make play-remote
.\make cli CMD=status
.\make flash
.\make flash-monitor
.\make flash-monitor LOG=logs\run.log
.\make cmd CMD=status
.\make snap
```

In `cmd.exe`, `make emu` works without the prefix.

- **Implementation:** `make.ps1` / `make.cmd` → `scripts/make.ps1` → `scripts/devctl.py` / CMake Ninja + `cl.exe` / IDF `export.ps1`

## One-time setup

- Git for Windows, Python 3.10+, PowerShell 5.1+
- **Visual Studio Build Tools** (2019 or later) with MSVC x64, CMake, and Ninja
- ESP-IDF v5.5+ at `%USERPROFILE%\esp\esp-idf` (firmware only)

```powershell
.\make deps
```

Installs pyserial and runs `install.ps1 esp32p4`. The first `.\make test` / `.\make emu` downloads SDL2 into `build-host/` (not a system package).

## Agent coverage

| Task | Command |
|------|---------|
| Tests | `.\make test` |
| Play vs bot (terminal) | `.\make play` |
| Play vs bot (SDL window) | `.\make emu` |
| Authoritative TCP server | `.\make server` (port 7420, `0.0.0.0`) |
| Two handhelds vs this PC | `.\make server` then flash both; allow TCP 7420 inbound |
| Play against TCP server | `.\make play-remote` |
| One JSON command | `.\make cli CMD=status` |
| List ports | `.\make ports` |
| Identify | `.\make identify` |
| Flash | `.\make flash` (auto Type-C) |
| Flash + serial log | `.\make flash-monitor` (screen + `logs\`) |
| Command | `.\make cmd CMD=status` |
| Screenshot | `.\make snap` (PNG under `logs\`) |
| Logs | `.\make logs SECONDS=8` |
