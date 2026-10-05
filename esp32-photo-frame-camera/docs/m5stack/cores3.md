# CoreS3

**SKU:** K128  
**Source:** https://docs.m5stack.com/en/core/CoreS3  
**Retrieved:** 2026-06-22

---

## Description

CoreS3 is the third-generation M5Stack main controller, based on ESP32-S3 (dual-core Xtensa LX7 @ 240 MHz, Wi-Fi). Integrates 16 MB Flash and 8 MB PSRAM. USB Type-C supports OTG & CDC.

**Front:** 2.0" capacitive-touch IPS (320×240), **GC0308 0.3 MP camera**, LTR-553ALS-WA proximity sensor.

**Power:** AXP2101 PMU. **Sensors:** BMI270 IMU, BMM150 magnetometer. **Audio:** AW88298 1 W speaker, ES7210 dual-mic codec. **Other:** microSD, BM8563 RTC.

**Buttons:** POWER (left), RST (bottom — long-press 3 s for download mode).

Ships with DinBase (DIN-rail / wall mount). External DC 9–24 V or internal 500 mAh battery.

## Specifications

| Specification | Parameter |
|---------------|-----------|
| SoC | ESP32-S3 @ 240 MHz |
| Flash | 16 MB |
| PSRAM | 8 MB Quad |
| Wi-Fi | 2.4 GHz |
| Display | 2.0" ILI9342C 320×240 touch IPS |
| Camera | GC0308 0.3 MP |
| Proximity | LTR-553ALS-WA |
| PMU | AXP2101 |
| IMU | BMI270 (6-axis) |
| Magnetometer | BMM150 (via BMI270 sensor hub) |
| RTC | BM8563 |
| Speaker | AW88298 @ 1 W |
| Audio codec | ES7210 dual mic |
| Size (unit) | 54.0 × 54.0 × 15.5 mm |
| Battery | 500 mAh |

## Download mode

Long-press **RESET** ~3 s until green LED lights, then release.

## Power on/off

- **On:** single-click POWER
- **Off:** long-press POWER 6 s
- **Reset:** single-click RST

## I2C address table

| Chip | Address |
|------|---------|
| AW88298 | 0x36 |
| AW9523 | 0x58 |
| AXP2101 | 0x34 |
| BM8563 | 0x51 |
| BMI270 | 0x69 |
| BMM150 | 0x10 |
| ES7210 | 0x40 |
| FT6336 | 0x38 |
| GC0308 | 0x21 |
| LTR553 | 0x23 |

## Pin map

### LCD (ILI9342C, 320×240)

| ESP32-S3 | G37 | G36 | G3 | G35 |
|----------|-----|-----|----|-----|
| Signal | MOSI | SCK | CS | DC |

| AW9523B (0x58) | P1_1 |
|----------------|------|
| ILI9342C RST | |

| AXP2101 (0x34) | DLDO1 / LX1 |
|----------------|-------------|
| Backlight / power | |

### microSD (max 16 GB)

| ESP32-S3 | G35 | G37 | G36 | G4 |
|----------|-----|-----|-----|-----|
| Signal | MISO | MOSI | SCK | CS |

### Camera GC0308 + LTR-553 (I2C 0x21 / 0x23)

| Signal | ESP32-S3 |
|--------|----------|
| SIOC (SCCB) | G11 |
| SIOD (SCCB) | G12 |
| XCLK | -1 |
| VSYNC | G46 |
| HREF | G38 |
| PCLK | G45 |
| D0–D7 | G39, G40, G41, G42, G15, G16, G48, G47 |
| RESET | -1 (via AW9523B P1_0) |
| PWDN | -1 |

### Touch FT6336U (0x38)

| ESP32-S3 | G12 | G11 |
|----------|-----|-----|
| I2C | SDA | SCL |

| AW9523B | P0_0 | P1_2 |
|---------|------|------|
| Touch | RST | INT |

### HY2.0-4P ports

| Port | Yellow | White |
|------|--------|-------|
| PORT.A | G2 (SDA) | G1 (SCL) |
| PORT.B | G9 | G8 |
| PORT.C | G17 (TX) | G18 (RX) |

**Module LLM UART:** Port C — `G17` = PC_TX, `G18` = PC_RX @ 115200 8N1.

### M5-Bus (selected pins)

| FUNC | PIN | | PIN | FUNC |
|------|-----|---|-----|------|
| | GND | 1–2 | G10 | ADC |
| MOSI | G37 | 7–8 | G5 | GPIO |
| MISO | G35 | 9–10 | G9 | PB_OUT |
| SCK | G36 | 11–12 | 3V3 | |
| RXD0 | G44 | 13–14 | G43 | TXD0 |
| **PC_RX** | **G18** | **15–16** | **G17** | **PC_TX** |
| Int SDA | G12 | 17–18 | G11 | Int SCL |

## PlatformIO configuration

```ini
[env:m5stack-cores3]
platform = espressif32@6.7.0
board = esp32-s3-devkitc-1
framework = arduino
upload_speed = 1500000
monitor_speed = 115200
build_flags =
    -DESP32S3
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
    -DCORE_DEBUG_LEVEL=5
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
lib_deps =
    M5Unified=https://github.com/m5stack/M5Unified
```

## Software resources (official)

- [CoreS3 Arduino Quick Start](https://docs.m5stack.com/en/arduino/m5cores3/program)
- [CoreS3 Camera](https://docs.m5stack.com/en/arduino/m5cores3/camera)
- [**Factory firmware source**](https://github.com/m5stack/CoreS3-UserDemo) → [`vendor/cores3/CoreS3-UserDemo`](../../vendor/cores3/CoreS3-UserDemo)
- [M5CoreS3 library](https://github.com/m5stack/M5CoreS3)
- [CoreS3 ESP-IDF BSP](https://docs.m5stack.com/en/esp_idf/m5cores3/program)

## Related docs in this repo

- [Arduino setup](./guides/cores3-arduino-setup.md)
- [Camera guide](./guides/cores3-camera.md)
- [VLM + LLM example](./stackflow/vlm-cores3-example.md)

## Notes

- **BMM150:** magnets in nearby products can interfere with compass readings.
- **Camera resolution:** practical working size is QVGA (320×240); use `frame2jpg()` for LLM/VLM input.
