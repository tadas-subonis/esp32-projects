# Vendor reference repositories

Local clones of **official factory firmware** and **driver libraries** for M5Stack devices used in the photo-frame project. These are for offline reading and diffing — not built as part of the Waveshare Rust firmware.

**Refresh clones:**

```bash
make vendor-update   # if Makefile target exists
# or manually:
cd docs/vendor/cores3/CoreS3-UserDemo && git pull
cd docs/vendor/papercolor/M5PaperColor-UserDemo && git submodule update --init --recursive && git pull
```

---

## Factory / pre-installed app source (start here)

These are the firmware images M5 ships on the devices (also available via M5Burner “factory” entries).

| Device | What you see on screen | Source repo | Path |
|--------|------------------------|-------------|------|
| **CoreS3** | LVGL launcher with 8 hardware test apps | [m5stack/CoreS3-UserDemo](https://github.com/m5stack/CoreS3-UserDemo) | `cores3/CoreS3-UserDemo/` |
| **M5Paper Color** | Local photo slideshow + EzData cloud push + web setup | [m5stack/M5PaperColor-UserDemo](https://github.com/m5stack/M5PaperColor-UserDemo) | `papercolor/M5PaperColor-UserDemo/` |

### CoreS3-UserDemo — built-in apps

PlatformIO + LVGL. Entry: `src/main.cpp` → `AppFactory.cpp` registers pages:

| App | Source directory | Tests |
|-----|------------------|-------|
| StartUp | `src/pages/StartUp/` | Boot animation |
| HomeMenu | `src/pages/HomeMenu/` | Icon launcher |
| AppWiFi | `src/pages/AppWiFi/` | Wi-Fi scan/connect |
| AppCamera | `src/pages/AppCamera/` | GC0308 + ALS/PS |
| AppMic | `src/pages/AppMic/` | ES7210 microphones |
| AppPower | `src/pages/AppPower/` | AXP2101 power paths |
| AppIMU | `src/pages/AppIMU/` | BMI270 + BMM150 |
| AppSD | `src/pages/AppSD/` | microSD |
| AppTouch | `src/pages/AppTouch/` | FT6336U touch |
| AppI2C | `src/pages/AppI2C/` | Grove / internal I²C scan |
| AppRTC | `src/pages/AppRTC/` | BM8563 RTC |

**Build:** `pio run -e M5CoreS3` (see repo README for `libesp32-camera.a` workaround).

### M5PaperColor-UserDemo — built-in modes

ESP-IDF v5.5.1. Entry: `main/main.cpp` → `app_manager_start()`.

| Mode / component | Source | Purpose |
|------------------|--------|---------|
| **Local Mode** | `main/apps/local_photo_slideshow/` | Slideshow from `/sdcard/photos` |
| **EzData Mode** | `main/apps/ezdata_photo_push/` | Push images from M5 EzData cloud |
| **Web config** | `main/apps/app_server/` | Captive portal + `index.html` Wi-Fi/mode setup |
| HAL | `main/hal/` | M5PM1, Wi-Fi, storage, e-ink, buttons |
| App manager | `main/apps/app_manager/` | Mode switch, low-power, button handling |

**Build:** `idf.py build flash` (after `git submodule update --init --recursive`).

---

## Driver / library references

| Repo | Role | Path |
|------|------|------|
| [M5CoreS3](https://github.com/m5stack/M5CoreS3) | Board Arduino lib + camera examples | `cores3/M5CoreS3/` |
| [M5Module-LLM](https://github.com/m5stack/M5Module-LLM) | UART API to LLM module (VLM, TTS, …) | `cores3/M5Module-LLM/` |
| [M5Unified](https://github.com/m5stack/M5Unified) | Unified HAL (both boards) | `common/M5Unified/` |
| [M5GFX](https://github.com/m5stack/M5GFX) | Graphics + Spectra-6 e-paper | `common/M5GFX/` |
| [M5PM1](https://github.com/m5stack/M5PM1) | PaperColor PMIC driver | `papercolor/M5PM1/` |

## Community supplements

| Repo | Role | Path |
|------|------|------|
| [PFalko/m5stack-papercolor-esphome](https://github.com/PFalko/m5stack-papercolor-esphome) | ESPHome + PMIC component reference | `papercolor/m5stack-papercolor-esphome/` |

---

## Pinned commits (2026-06-22)

| Path | Commit |
|------|--------|
| `cores3/CoreS3-UserDemo` | `90cbcc6ca40c2404de05d3f9fdb01223ae19d3f0` |
| `papercolor/M5PaperColor-UserDemo` | `1ff998e0cf3b9ce916f2e0319561d043db21cc4a` |
| `cores3/M5CoreS3` | `adce2225e7fd8d012b148f46c990f8cc7b8ecc26` |
| `cores3/M5Module-LLM` | `5d0a761e0618938039d49091570628934b98be0d` |
| `common/M5Unified` | `8108bfad04a20ff4e57c0751f62dd6fdf6b137a6` |
| `common/M5GFX` | `27e1ef0f7bab2db8aa77c2768b8934983a656a7c` |
| `papercolor/m5stack-papercolor-esphome` | `bd0dca4daab3feae9f5657d4922185cdf0eba562` |

Run `git -C <path> rev-parse HEAD` after `git pull` to update pins.

---

## Related markdown docs

- [M5Stack device index](../devices/m5stack/README.md)
- [Architecture](../devices/m5stack/architecture-photo-frame-app.md)
