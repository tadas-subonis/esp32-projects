# PhotoColor — display / receiver firmware

**Role:** HTTP server, Spectra-6 dither, e-ink refresh, optional caption + TTS.

## Build / flash (repo root)

```powershell
make build-papercolor
make identify
make flash-papercolor PORT=COM8
```

WSL: `make flash-papercolor PORT=/dev/ttyACM0`

Hold **RST** while plugging USB if the port does not appear.

## Prerequisites

- [ESP-IDF v5.5.1](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/index.html) with `esp32s3` target
- Installed by `make deps` (`%USERPROFILE%\esp\esp-idf` on Windows, `~/esp/esp-idf` on WSL)

## Build (direct)

```powershell
. $env:USERPROFILE\esp\esp-idf\export.ps1
cd papercolor
idf.py build
idf.py -p COM8 flash monitor
```

```bash
. ~/esp/esp-idf/export.sh
cd papercolor
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

## Agent commands

```powershell
make cmd-papercolor CMD=status PAPERCOLOR_PORT=COM8
make cmd-papercolor CMD=gallery PAPERCOLOR_PORT=COM8
make logs-papercolor PAPERCOLOR_PORT=COM8 SECONDS=8
```

See [docs/agent-tooling.md](../docs/agent-tooling.md).

## Phase 1 — M5 components

Mirror factory firmware component layout from `M5PaperColor-UserDemo`:

```bash
git submodule add https://github.com/m5stack/M5GFX.git components/M5GFX
git submodule add https://github.com/m5stack/M5Unified.git components/M5Unified
```

Or use `idf_component.yml` dependencies (`m5stack/m5pm1`, etc.) as in the factory `main/idf_component.yml`.

## References

- Factory source: `M5PaperColor-UserDemo` (`main/apps/`, `main/hal/`)
- PMIC + e-ink: `M5PM1`, `M5GFX` Spectra-6 paths
