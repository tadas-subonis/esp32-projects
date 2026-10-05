# Hardware

POC board: Waveshare ESP32-P4-WIFI6-POE-ETH Rev 2.0 + 3.5" SPI **ILI9488** (480×320, RGB666). Full bring-up report and init sequence: `C:\Work\my\esp32-debug-program\docs\stage2-local-artillery.md`.

Firmware HAL matches this pinout. Flash over Type-C UART (CH343), not Type-A / USB Serial/JTAG.

Wi-Fi 6 is the onboard ESP32-C6 over SDIO (ESP-Hosted): CLK 18, CMD 19, D0–D3 14–17, C6 reset GPIO54. Do not reuse those GPIOs. STA credentials and `SERVER_HOST` are read from `.local.env` at firmware configure time.

ESP-Hosted RPC must not run on `sys_evt` or the game task — only set bits / flags there and let the `net` task call `esp_wifi_connect` / `esp_wifi_disconnect`. Join stalls and mid-match drops are easier to diagnose when UART and server logs share `eid` / `t_ms` (see [architecture.md](architecture.md)).

## LCD — right / outer 40-pin column

| TFT | GPIO / rail |
|---|---|
| VCC | 5 V |
| GND | GND |
| LED | 3.3 V (not a GPIO) |
| CS | GPIO22 |
| RESET | GPIO5 |
| DC/RS | GPIO4 |
| SDI/MOSI | GPIO36 |
| SCK | GPIO32 |
| SDO/MISO | not connected |

SPI2, mode 0, **40 MHz**. Compose RGB565 in PSRAM; flush as RGB666 (`0x2C` first row of a window, then `0x3C`) in 16-line **ping-pong** DMA strips so the queued SPI buffer is never overwritten. `INVOFF`. Backlight is wired to 3.3 V — `DeviceHal::backlight()` is a no-op. Idle frames upload only the debug overlay; aiming uploads HUD + tank rects; a full 480×320 blit is the fallback when the map or title changes.

## Buttons — left / inner column (active-low, internal pull-up)

| Button | GPIO |
|---|---:|
| UP | 20 |
| DOWN | 6 |
| LEFT | 3 |
| RIGHT | 2 |
| A | 33 |
| B | 26 |
| SELECT | 48 (unused) |
| START | 47 (unused) |

Do not use GPIO7/8 (onboard I2C / ES8311), GPIO53 (amp), GPIO37/38 (Type-C UART), GPIO0 (boot). Do not drive GPIO33 or GPIO26 as outputs.
