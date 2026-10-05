# WSL2 + USB: build and flash on Windows

> **Prefer native Windows?** USB works directly as `COM*` ports — no usbipd. See [windows-native.md](windows-native.md) and use `make` from the repo root.

You develop in **WSL2**; USB serial devices are plugged into **Windows**. WSL does not see them until you forward them with [usbipd-win](https://github.com/dorssel/usbipd-win).

Official background: [Connect USB devices | Microsoft Learn](https://learn.microsoft.com/en-us/windows/wsl/connect-usb).

## One-time setup (Windows)

1. **WSL2** with a recent kernel (`wsl --update`, then `wsl --shutdown` if needed).
2. Install **usbipd-win** (pick one):
   - [Latest `.msi` release](https://github.com/dorssel/usbipd-win/releases)
   - Or: `winget install --interactive --exact dorssel.usbipd-win`
3. In WSL, add your user to the serial group (then **restart WSL**):

   ```bash
   sudo usermod -aG dialout $USER
   ```

4. Repo toolchain (from WSL, at repo root):

   ```bash
   make deps
   ```

## Attach a device (each USB plug / each board)

Run **PowerShell as Administrator** on Windows:

```powershell
usbipd list
```

Find your ESP32 (often **Silicon Labs** / **Espressif** / **USB JTAG/serial**). Note the **BUSID** (e.g. `2-4`).

Share it once per physical USB port (admin):

```powershell
usbipd bind --busid 2-4
```

Attach to WSL (normal PowerShell is fine; keep a WSL terminal open):

```powershell
usbipd attach --wsl --busid 2-4
```

In **WSL**:

```bash
make usb-status
```

You should see a new `/dev/ttyACM0` or `/dev/ttyUSB0`. Use that path as `PORT` below.

When finished (or to use the port from Windows again):

```powershell
usbipd detach --busid 2-4
```

### Two boards (PaperColor + CoreS3)

1. Plug **both** USB cables into Windows.
2. `usbipd list` → two BUSIDs.
3. `usbipd bind --busid …` for each (once per port).
4. `usbipd attach --wsl --busid …` for each.
5. In WSL: `make usb-status` → note which is `ttyACM0` vs `ttyACM1` (order can swap).
6. Flash with separate ports, e.g.:

   ```bash
   make flash-papercolor PORT=/dev/ttyACM0
   make flash-cores3     PORT=/dev/ttyACM1
   ```

**Tip:** Flash **PaperColor first** (it hosts the Wi‑Fi AP), then CoreS3.

## Build (WSL)

From repo root:

```bash
make build              # both targets
make build-cores3       # PlatformIO
make build-papercolor   # ESP-IDF (sources export.sh)
```

Copy Wi‑Fi secrets if needed:

```bash
cp cores3/include/secrets.h.example cores3/include/secrets.h
```

## Flash + monitor (WSL)

| Target | Make target | Default port |
|--------|-------------|----------------|
| M5Paper Color | `make flash-papercolor PORT=/dev/ttyACM0` | `PORT` → `/dev/ttyACM0` |
| CoreS3 | `make flash-cores3 PORT=/dev/ttyACM1` | same `PORT` variable |

**CoreS3 download mode:** if esptool reports `No serial data received`, put CoreS3 in ROM download mode per [M5 docs](https://docs.m5stack.com/en/core/CoreS3): **long-press RST ~3s** (green LED), then run:

```bash
make flash-cores3-wait CORES3_PORT=/dev/ttyACM1
```

This retries until esptool connects (full bootloader + partitions + app). Flash uses `--after no_reset` so usbipd stays attached; `scripts/cores3-post-flash.sh` pulses RTS to start the app.

**Re-attach after USB drop:** `make usb-attach` (calls Windows `usbipd` from WSL via PowerShell).

### CoreS3: flash from Windows (recommended when WSL esptool fails)

WSL usbipd + esptool often cannot talk to CoreS3 when it enumerates as TinyUSB CDC. Use native Windows COM instead:

```bash
# WSL: build firmware
make build-cores3

# WSL: detach CoreS3 from WSL, flash on COM9, re-attach
make flash-cores3-windows CORES3_COM=COM9 CORES3_BUSID=4-4
```

Find `CORES3_COM` / `CORES3_BUSID` from Windows `usbipd list` (CoreS3 is usually the port that is **not** PaperColor).

**While the script retries**, long-press CoreS3 **RST ~3s** (green LED) per [M5 CoreS3 download mode](https://docs.m5stack.com/en/core/CoreS3).

Or run directly in **PowerShell** (repo on disk or `\\wsl.localhost\...`):

```powershell
cd \\wsl.localhost\Ubuntu-24.04\home\tadas\work\my\esp32-photo-frame-camera-monorepo
.\scripts\flash-cores3-windows.ps1 -ComPort COM9 -BusId 4-4
```

PaperColor stays on WSL (`make flash-papercolor`); only CoreS3 needs the Windows path today.

Monitor only (no rebuild):

```bash
make monitor-papercolor PORT=/dev/ttyACM0
make monitor-cores3     PORT=/dev/ttyACM1
```

PaperColor `flash-papercolor` flashes firmware; use `make monitor-papercolor` for serial logs. Exit monitor with `Ctrl+]`.

## Agent-friendly commands + log capture (WSL)

This repo includes a small host helper (`scripts/devctl.py`) and Makefile wrappers for agent-driven iteration.

List ports:

```bash
make ports
```

Send commands (non-interactive):

```bash
make cmd-papercolor CMD="status" PAPERCOLOR_PORT=/dev/ttyACM0
make cmd-cores3     CMD="status" CORES3_PORT=/dev/ttyACM1
make capture                 CORES3_PORT=/dev/ttyACM1
```

Capture logs for a few seconds:

```bash
make logs-papercolor PAPERCOLOR_PORT=/dev/ttyACM0 SECONDS=5
make logs-cores3     CORES3_PORT=/dev/ttyACM1     SECONDS=5
```

Important: the serial port can only be owned by one process at a time — close `make monitor-*` before using `make cmd-*` / `make logs-*`.

## End-to-end smoke test

1. PaperColor: SD card inserted, USB power, flashed and running → serial shows `AP PhotoFrame @ 192.168.4.1`.
2. CoreS3: flashed → serial shows Wi‑Fi connected `192.168.4.2`.
3. Optional from laptop on `PhotoFrame` Wi‑Fi:

   ```bash
   make verify-photo-post
   ```

4. Tap CoreS3 screen → photo on e‑ink (~15–20 s).

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| No `/dev/ttyACM*` in WSL | `usbipd attach` from Windows; run `make usb-status`; keep WSL session open during attach |
| `Permission denied` on port | `sudo usermod -aG dialout $USER`, then `wsl --shutdown` and reopen WSL |
| Wrong board flashed | Unplug one device; `make usb-status`; flash one at a time |
| `idf.py` / `export.sh` errors from `make` | Run `make build-papercolor` from WSL bash (not PowerShell); ensure `~/esp/esp-idf` exists (`make deps`) |
| `pio: not found` | `make deps` or use `make build-cores3` (Makefile picks ESP-IDF’s PlatformIO) |
| Device vanishes from WSL after flash | esptool **hard reset** re-enumerates USB; usbipd drops to **Shared** | `make usb-attach` from WSL; CoreS3 flash uses `--after no_reset` + `scripts/cores3-post-flash.sh` |
| `usbipd: Device is not shared` | Port lost **bind** after detach | **Admin** PowerShell: `usbipd bind --busid <BUSID>` then `make usb-attach` |
| `erase_flash` on CoreS3 | Wipes bootloader — app-only flash won't boot | Flash **bootloader + partitions + app** (full `make flash-cores3` or esptool `0x0` + `0x8000` + `0x10000`) |

## Make targets (reference)

```bash
make help              # list targets
make usb-help          # short usbipd reminder (Windows commands)
make usb-status        # lsusb + serial devices in WSL
make usb-attach        # re-attach detached ESP32 via Windows usbipd (from WSL)
make deps              # toolchain + vendor clones
make build             # cores3 + papercolor
make flash-papercolor  # build + flash + monitor
make flash-cores3      # build + upload
make verify-photo-post # curl test JPEG to frame (on PhotoFrame Wi‑Fi)
make clean             # clean both builds
```

Environment overrides:

- `PORT=/dev/ttyACM1` — serial port for flash/monitor
- `IDF_EXPORT=~/esp/esp-idf/export.sh` — ESP-IDF location
- `PIO=/path/to/pio` — PlatformIO CLI
