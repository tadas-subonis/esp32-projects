# M5Paper Color

**SKU:** C151  
**Source:** https://docs.m5stack.com/en/core/PaperColor  
**Retrieved:** 2026-06-22

---

## Description

PaperColor is a development board featuring a 4-inch E Ink Spectra 6 full-color e-paper display with a resolution of 400×600, offering both low power consumption and high visibility. It is built around an integrated ESP32-S3R8 core with 16MB Flash and 8MB PSRAM, and supports 2.4 GHz Wi-Fi wireless communication.

The board incorporates a complete human-machine interaction system, including three programmable buttons, an audio system based on the ES8311 audio codec, a MEMS microphone with AEC echo cancellation, and a speaker driven by an AW8737A amplifier — enabling high-quality voice capture and audio playback.

Additional peripherals: SHT40 temperature/humidity sensor, RX8130CE RTC, microSD card slot, infrared emitter, two RGB LEDs, and an HY2.0-4P expansion interface. Combined with the M5PM1 multi-stage power management system and a 1250mAh battery.

**Applications:** smart digital photo frame, data display panel, electronic signage, voice interaction terminals.

## Important note

**Refresh time:** approximately **10–20 s**, depending on color distribution complexity. Spectra-6 is **full-refresh only** (no partial refresh). **No touch panel.**

## Specifications

| Specification | Parameter |
|---------------|-----------|
| SoC | ESP32-S3R8 @ Dual-core Xtensa LX7, up to 240 MHz |
| Flash | 16 MB |
| PSRAM | 8 MB |
| Wi-Fi | 2.4 GHz |
| Display | 4" E-Paper Spectra 6 (E6) ED2208-DOA (EL040EF1), **400×600** |
| Input power | USB Type-C DC 5 V |
| Battery | 1250 mAh |
| Audio codec | ES8311 |
| Microphone | MEMS + ES7210 ADC, AEC circuit |
| Speaker | 1 W @ 8 Ω, AW8737A amplifier |
| Temp/humidity | SHT40 |
| Storage | microSD |
| RTC | RX8130CE |
| Buttons | 3× user + 1× power (ON/OFF/RESET/BOOT) |
| Standby power | 92.53 µA |
| Full load | 211.97 mA |
| Size | 70.8 × 103.9 × 8.5 mm |
| Weight | 73.3 g |

## Power on/off

- **Power on / restart:** press power button once
- **Power off:** press power button twice in quick succession

## Download mode

Connect USB Type-C, then **press and hold the side reset button** until download mode; flash firmware.

## Pin map

### E-Paper (EL040EF1)

| ESP32-S3 | G15 | G13 | G44 | G43 | G11 | G12 |
|----------|-----|-----|-----|-----|-----|-----|
| Signal | SPI_CLK | SPI_MOSI | EINK_CS | EINK_DC | EINK_BUSY | EINK_RST |

| M5PM1 | PYG0 |
|-------|------|
| EL040EF1 | PY_EPD_EN (e-paper power enable) |

### User buttons

| ESP32-S3 | G1 | G9 | G10 |
|----------|----|----|-----|
| Button | USER_KEY1 (C) | USER_KEY2 (B) | USER_KEY3 (A) |

### IR

| ESP32-S3 | G48 |
|----------|-----|
| IR_TX | IR emitter |

### RGB LED

| ESP32-S3 | G21 |
|----------|-----|
| RGB | 2× WS2812-style LEDs |

### Audio

| ESP32-S3 | G2 | G3 |
|----------|----|----|
| ES8311 (0x18) | AUDIO_I2C_SCL | AUDIO_I2C_SDA |
| ES7210 (0x40) | AUDIO_I2C_SCL | AUDIO_I2C_SDA |

| ESP32-S3 | G42 | G41 | G40 | G39 | G38 |
|----------|-----|-----|-----|-----|-----|
| ES8311 | I2S_MCLK | I2S_LRCK | I2S_BCLK | | I2S_DSDIN |
| ES7210 | I2S_MCLK | I2S_LRCK | I2S_BCLK | I2S_SDOUT | |

| ESP32-S3 | G45 | G46 |
|----------|-----|-----|
| G45 | AUDIO_PWR_EN — codec + mic power |
| G46 | SPK_EN — speaker amplifier enable |

### RTC (RX8130CE 0x32)

| ESP32-S3 | G7 | G3 | G2 |
|----------|----|----|-----|
| Signal | RTC_IRQ | SYS_SDA | SYS_SCL |

### SHT40 (0x44)

| ESP32-S3 | G3 | G2 |
|----------|----|-----|
| Bus | SYS_SDA | SYS_SCL |

### microSD

| ESP32-S3 | G47 | G15 | G13 | G14 |
|----------|-----|-----|-----|-----|
| Signal | CS | SPI_CLK | SPI_MOSI | SPI_MISO |

### HY2.0-4P (PORT.A)

| Pin | Black | Red | Yellow | White |
|-----|-------|-----|--------|-------|
| PORT.A | GND | 5 V | G4 | G5 |

### M5PM1 (0x6E)

| ESP32-S3 | G3 | G2 |
|----------|----|----|
| Bus | SYS_SDA | SYS_SCL |

**M5PM1 power switches:**

| M5PM1 pin | Function |
|-----------|----------|
| PYG0 (PY_EPD_EN) | E-paper power |
| PYG1 (CARD_DEC) | SD card detect |
| PYG2 (RTC_IRQ) | RTC interrupt |
| PYG3 (PY_SD_PWR_EN) | microSD power |
| PYG4 (PY_SD_DET_EN) | SD detect enable |
| DCDC3V3_EN_PP (PY_MPWR_EN) | 3V3_L2 switch |
| LDO3V3_EN_PP (PY_RGB_PWR_EN) | RGB LED power |
| BOOST5V_EN_PP (PY_GROVE_OUT_EN) | Grove power |

See also [guides/papercolor-m5pm1-power.md](./guides/papercolor-m5pm1-power.md).

## PlatformIO configuration

```ini
[env:m5stack-papercolor]
platform = espressif32 @ 6.12.0
board = esp32s3box
framework = arduino
board_build.partitions = default_16MB.csv
board_upload.flash_size = 16MB
board_upload.maximum_size = 16777216
board_build.arduino.memory_type = qio_opi
monitor_speed = 115200
build_flags =
    -DESP32S3
    -DBOARD_HAS_PSRAM
    -DCORE_DEBUG_LEVEL=5
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
lib_deps =
    M5Unified = https://github.com/m5stack/M5Unified
    M5GFX = https://github.com/m5stack/M5GFX
    M5PM1 = https://github.com/m5stack/M5PM1
```

## Software resources (official)

- [PaperColor Arduino Quick Start](https://docs.m5stack.com/en/arduino/papercolor/program)
- [PaperColor M5PM1 Power Management](https://docs.m5stack.com/en/arduino/papercolor/m5pm1)
- [**Factory firmware source (ESP-IDF)**](https://github.com/m5stack/M5PaperColor-UserDemo) → [`vendor/papercolor/M5PaperColor-UserDemo`](../../vendor/papercolor/M5PaperColor-UserDemo)
- [M5PM1](https://github.com/m5stack/M5PM1) → [`vendor/papercolor/M5PM1`](../../vendor/papercolor/M5PM1)
- [PaperColor Factory Firmware Usage Guide](https://docs.m5stack.com/en/guide/papercolor/factory_firmware)
- [Shop page](https://shop.m5stack.com/products/m5paper-color-esp32s3-dev-kit)

## Related docs in this repo

- [Arduino setup guide](./guides/papercolor-arduino-setup.md)
- [M5PM1 power management](./guides/papercolor-m5pm1-power.md)
- [ESPHome community hardware notes](./community/papercolor-esphome-hardware-notes.md)
