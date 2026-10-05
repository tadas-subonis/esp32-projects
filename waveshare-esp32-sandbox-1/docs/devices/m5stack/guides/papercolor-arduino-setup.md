# PaperColor — Arduino Program Compilation & Upload

**Source:** https://docs.m5stack.com/en/arduino/papercolor/program  
**Retrieved:** 2026-06-22

---

## 1. Prerequisites

1. **Arduino IDE** — install per M5Stack [Arduino IDE Installation Guide](https://docs.m5stack.com/en/arduino/arduino_ide).
2. **Board manager** — install M5Stack board manager; select board **`M5PaperColor`**.
3. **Libraries** — install `M5Unified` and `M5GFX` (install all prompted dependencies).

## 2. Port selection / download mode

1. Connect USB-C.
2. **Press and hold the side power/reset button** until download mode.
3. Select the correct COM port in Arduino IDE.

## 3. Example: color band demo

Use `M5Canvas` with `epd_mode_t::epd_quality` for best e-ink quality. Full example from official docs:

```cpp
#include <M5Unified.h>

M5Canvas Canvas(&M5.Display);

static void drawBoldText(M5Canvas& canvas, const String& text, int x, int y, uint16_t color)
{
    canvas.setTextColor(color);
    canvas.drawString(text, x, y);
    canvas.drawString(text, x + 1, y);
    canvas.drawString(text, x, y + 1);
    canvas.drawString(text, x + 1, y + 1);
}

void setup()
{
    auto cfg          = M5.config();
    cfg.clear_display = false;
    M5.begin(cfg);
    M5.Display.setEpdMode(epd_mode_t::epd_quality);

    const int screen_w = M5.Display.width();
    const int screen_h = M5.Display.height();

    Canvas.createSprite(screen_w, screen_h);
    Canvas.fillSprite(WHITE);

    const uint16_t band_colors[6] = {YELLOW, RED, GREEN, BLUE, BLACK, WHITE};
    const int band_h             = screen_h / 6;

    for (int i = 0; i < 6; ++i) {
        const int y = i * band_h;
        const int h = (i == 5) ? (screen_h - y) : band_h;
        Canvas.fillRect(0, y, screen_w, h, band_colors[i]);
    }

    const int white_band_y = 5 * band_h;
    const int white_band_h = screen_h - white_band_y;

    Canvas.setTextFont(4);
    Canvas.setTextSize(2);
    Canvas.setTextDatum(middle_left);
    Canvas.setTextColor(BLACK);

    const String text_paper = "PAPER";
    const String text_color = "COLOR";
    const int gap_w         = Canvas.textWidth("   ");
    const int total_w       = Canvas.textWidth(text_paper) + gap_w + Canvas.textWidth(text_color);
    const int start_x       = (screen_w - total_w) / 2;
    const int text_y        = white_band_y + (white_band_h / 2);

    drawBoldText(Canvas, text_paper, start_x, text_y, BLACK);

    int x = start_x + Canvas.textWidth(text_paper) + gap_w;
    const uint16_t color_text_colors[5] = {RED, YELLOW, GREEN, YELLOW, BLUE};
    for (int i = 0; i < text_color.length(); ++i) {
        const String ch = text_color.substring(i, i + 1);
        drawBoldText(Canvas, ch, x, text_y, color_text_colors[i]);
        x += Canvas.textWidth(ch);
    }

    Canvas.pushSprite(0, 0);
}

void loop()
{
    M5.update();
    delay(100);
}
```

**Expected result:** six horizontal color bands with “PAPER COLOR” text. Refresh takes ~10–20 s.

## 4. Spectra-6 color constants

Native panel colors used in M5GFX: `BLACK`, `WHITE`, `RED`, `YELLOW`, `GREEN`, `BLUE`. Photos must be quantized to these six inks.

## 5. Related resources

- [M5Unified](https://github.com/m5stack/M5Unified)
- [M5GFX](https://github.com/m5stack/M5GFX)
- [M5PM1 power management](./papercolor-m5pm1-power.md)
- [Hardware spec](../m5paper-color.md)
