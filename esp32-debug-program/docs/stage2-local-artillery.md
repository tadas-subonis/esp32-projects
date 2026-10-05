# Local artillery on the POC board

One document: report, HAL spec, and how to run Tank Duel on this Waveshare P4 + ILI9488 breadboard.

**Do not flash the game firmware until the HAL matches this file.** The current game image uses the old plan’s ST7796 pins. Several of those are this board’s button GPIOs. GPIO33 is button A (to GND) but the game drives it as a backlight output — pressing A can short a pin.

| | Path |
|---|---|
| This debug firmware (known-good LCD + buttons) | `C:\Work\my\esp32-debug-program` |
| Game (edit HAL here) | `C:\Work\my\esp32-gameconsole-artillery-game` |
| Board | Waveshare ESP32-P4-WIFI6-POE-ETH Rev 2.0 |
| Panel | 3.5" SPI ILI9488, 480×320, RGB666 |
| Flash / serial | Type-C (CH343 UART), not Type-A, not USB Serial/JTAG |

---

## Report

Stage 0 LCD and Stage 1 buttons are done (`hw-lcd-working`, `hw-buttons-working`). Stage 2 is a local 1v1 tank artillery match on this same wiring. The downloaded console plan assumed a Waveshare ST7796S (SKU 29318) and a first-draft GPIO table. This POC used a different module and a pin map proven in bring-up. That pin map is the one below.

The game is already the right shape: portable sim, vs-bot, hotseat, dirty-rect 480×320 compose, firmware running a **local** `Match` (no Wi-Fi). Work is a HAL port, not a rewrite. Do not copy `game/` into the debug repo. Do not start Wi-Fi, audio, SD, launcher, battery, or matchmaking until a hotseat match finishes on this device.

### What already works (no board)

| Path | Role |
|---|---|
| `game/` | Heightmap, ballistics, craters, match, bot, RGB565 renderer |
| `host/` | SDL emu, terminal play, CLI |
| `server/` | Authoritative TCP — later, not Stage 2 |
| `firmware/` | ESP-IDF P4 app — compiles, HAL is wrong for this board |

On PC: title **TANK DUEL**, D-pad/B toggles **VS BOT** / **HOTSEAT**, **A** starts. Aim with LEFT/RIGHT (angle) and UP/DOWN (power), **A** fires. Game over: **A** rematch, **B** title. Firmware uses the same loop once the HAL works.

### Gap

| | Game firmware today | This POC |
|---|---|---|
| Panel | ST7796, COLMOD `0x55` RGB565 | ILI9488, COLMOD `0x66` RGB666 |
| Invert | `INVON` (`0x21`) | `INVOFF` (`INVON` looked blueish here) |
| SPI | 40 MHz, MISO GPIO20 | **10 MHz**, MISO disconnected |
| Pixel write | `0x2C` every row | `0x2C` first row of a window, then **`0x3C`** |
| LCD | SCLK23 MOSI21 MISO20 CS26 DC27 RST32 BL33 | **CS22 RST5 DC4 MOSI36 SCK32**, LED = 3.3 V (not a GPIO) |
| Buttons | UP2 DOWN3 LEFT4 RIGHT5 A46 B47 | **UP20 DOWN6 LEFT3 RIGHT2 A33 B26** (SELECT48 START47 unused) |
| Console | USB Serial/JTAG | Type-C UART |
| PSRAM | off | on (307,200-byte framebuffer) |

Repeating `0x2C` every row rewinds the write pointer. Symptom: one thin line at the top of the screen.

---

## Wiring (do not change)

Leave the breadboard as it is. Screen logic is 3.3 V — never put 5 V on a P4 GPIO. If the TFT module has **SD_CS**, tie it to 3.3 V.

LCD — **right / outer** 40-pin column (5 V at the top):

| TFT | GPIO / rail |
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

Buttons — **left / inner** column. Each switch: GPIO — switch — GND. Internal pull-up. Pressed = LOW.

| Button | GPIO |
|---|---|
| UP | GPIO20 |
| DOWN | GPIO6 |
| LEFT | GPIO3 |
| RIGHT | GPIO2 |
| A | GPIO33 |
| B | GPIO26 |
| SELECT | GPIO48 (unused by the game) |
| START | GPIO47 (unused by the game) |

Do not use GPIO7/8 (onboard I2C / ES8311), GPIO53 (amp), GPIO37/38 (Type-C UART), GPIO0 (boot). Do not drive GPIO33 or GPIO26 as outputs.

---

## Spec — files to change

All edits in `C:\Work\my\esp32-gameconsole-artillery-game`. Leave debug firmware unmodified so a bad game image can be undone.

| File | Change |
|---|---|
| `firmware/main/device_hal.hpp` | Pin constants below. `backlight()` is a no-op. No BL GPIO. |
| `firmware/main/st7796.cpp` | ILI9488 init + RGB666 flush (rename the file if you want; keep `flush_rect`) |
| `firmware/main/console.cpp` | Read UART, not `usb_serial_jtag_read_bytes` |
| `firmware/sdkconfig.defaults` | PSRAM on, UART console, drop USB Serial/JTAG |
| `game/` physics, renderer, protocol | Do not touch except to compile |

Do not wire `ServerProxy`, TCP, or C6 Wi-Fi. Modes: vs-bot and hotseat only.

### Pins (`device_hal.hpp`)

```text
LCD  CS=22  RST=5  DC=4  MOSI=36  SCK=32  MISO=NC  BL=NC
BTN  UP=20  DOWN=6  LEFT=3  RIGHT=2  A=33  B=26
```

`artillery::Buttons` stays six buttons. SELECT/START stay unused. No A+B launcher chord.

| Control | Aiming | Title | Game over |
|---|---|---|---|
| LEFT / RIGHT | angle | toggle VS BOT / HOTSEAT | — |
| UP / DOWN | power | toggle | — |
| A | fire | start | rematch |
| B | — | toggle | back to title |

### `sdkconfig.defaults`

```text
CONFIG_IDF_TARGET="esp32p4"
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_SPIRAM=y
CONFIG_ESP_CONSOLE_UART_DEFAULT=y
CONFIG_ESP_CONSOLE_UART_BAUDRATE=115200
```

Remove `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`. Flash and serial share the Type-C COM port. Put the 480×320 RGB565 compose buffer in PSRAM. Use a DMA **line** buffer of `480 * 3` bytes for SPI, not a DMA full frame.

**Perf (measured):** full-frame SPI on this panel is ~2.7 FPS @ 10 MHz RGB666. Dirty-rect compose is required for playable frame rates. Details: [`p4bench-findings.md`](p4bench-findings.md). Suite: [`p4bench.md`](p4bench.md).

### ILI9488 init (copy this)

SPI2, mode 0, **10 MHz** until the picture is stable. Hardware reset on GPIO5: LOW ~80 ms, HIGH ~250 ms. Then:

```text
SWRESET
SLPOUT
INVOFF                          // not INVON
MADCTL    0x28                  // MV|BGR landscape
COLMOD    0x66                  // 18-bit RGB666
C0        0x17 0x15
C1        0x41
C5        0x00 0x12 0x80
B0        0x80
B1        0xA0
B4        0x02
B6        0x02 0x02 0x3B
B7        0xC6
F7        0xA9 0x51 0x2C 0x02
DISPON
```

`game/` still composes RGB565. Convert on flush:

```c
out[0] = (px >> 8) & 0xF8;   // R
out[1] = (px >> 3) & 0xFC;   // G
out[2] = (px << 3) & 0xF8;   // B
```

For a rectangle: `CASET` / `RASET` once, then **`0x2C` on row 0**, **`0x3C` on later rows**. Never send `0x2C` per row. Do not send swapped 16-bit RGB565 words.

### Done when

Tag **`game-artillery-local`** in the **game** repo when all of these pass on this board:

1. Title screen is readable (not white, not a single line at the top).
2. **A** starts vs-bot; bot takes P1; a shot draws, craters, HP updates.
3. Hotseat: two people share the device and finish a match; rematch or title works.
4. Type-C serial: `status`, then `new 42`, then `fire` each return `<<< {json}` with `ok:true` and a real `phase`.
5. LCD and button GPIOs unchanged from the tables above.

---

## How to run it

### 1. Play on PC (today, no board)

```powershell
cd C:\Work\my\esp32-gameconsole-artillery-game
.\make.ps1 test
.\make.ps1 emu
```

`.\make.ps1 play` is vs-bot in the terminal. Emu keys: arrows or WASD, Space/Z = A, X = B, Esc/Q = quit. If that fails: VS Build Tools (MSVC x64), then `.\make.ps1 deps`.

### 2. Confirm the breadboard still matches this doc

Do not rewire for the game. If the picture is in doubt, flash the debug firmware (`TEST_MODE 3` = buttons, `1` = color bars):

```powershell
cd C:\Work\my\esp32-debug-program
.\make.ps1 flash
```

Expect `BUTTONS` with eight rows, or red/green/blue fills with corner squares.

### 3. After the HAL port, flash the game

```powershell
cd C:\Work\my\esp32-gameconsole-artillery-game
.\make.ps1 build-firmware
.\make.ps1 ports
.\make.ps1 flash PORT=COMx
.\make.ps1 cmd CMD=status PORT=COMx
.\make.ps1 cmd CMD="new 42" PORT=COMx
.\make.ps1 cmd CMD=fire PORT=COMx
```

On screen: title → A (vs-bot) → aim → A to shoot. Then hotseat and a full match.

| Symptom | Cause | Fix |
|---|---|---|
| White screen or one line at the top | `0x2C` every row, or RGB565/`INVON` | Flush like this doc; reflash debug firmware to restore a picture |
| Serial silent on Type-C COM | Still USB-JTAG | UART in `sdkconfig.defaults` + `console.cpp`, `idf.py fullclean`, flash again |
| Button does nothing | Wrong GPIOs | Use the table above, not the old plan |

To undo a bad game image:

```powershell
cd C:\Work\my\esp32-debug-program
.\make.ps1 flash
```

### Order of work

1. PC `test` + `emu`
2. Game firmware: pins, ILI9488 init, RGB666 `0x2C`/`0x3C` flush, UART, PSRAM
3. Flash — title screen is the first hardware gate
4. Serial `new` / `fire` with no buttons
5. Vs-bot with buttons
6. Hotseat with a second person
7. Tag `game-artillery-local`
