# LCD + button wiring

Board: **Waveshare ESP32-P4-WIFI6-POE-ETH Rev 2.0**  
Panel: **3.5" SPI TFT 480×320 ILI9488** (18-bit RGB666)

Flash from **Type-C**. Backlight is hardwired: LED → 3.3 V (not a GPIO). Screen logic is 3.3 V — never put 5 V on a P4 GPIO.

If the module has **SD_CS**, tie it to **3.3 V**.

## LCD (right / outer column)

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
| SDO/MISO | nc |
| touch / microSD | nc |

Do not use TXD/RXD (GPIO37/38).

## Buttons (left / inner column)

Each tactile switch: **GPIO — switch — GND**. Internal pull-up, pressed = LOW.

| Button | GPIO |
|---|---|
| UP | GPIO20 |
| DOWN | GPIO6 |
| LEFT | GPIO3 |
| RIGHT | GPIO2 |
| A | GPIO33 |
| B | GPIO26 |
| SELECT | GPIO48 |
| START | GPIO47 |

Leave GPIO7/8 (onboard I2C), GPIO53 (speaker amp), and GPIO0 (boot) alone.

GPIOs live at the top of `firmware/main/main.c`. LCD lessons: [`notes.md`](notes.md). Product plan: [`console-plan.md`](console-plan.md).
