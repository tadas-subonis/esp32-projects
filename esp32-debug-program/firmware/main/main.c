#include <stdio.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "font8.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "p4bench.h"

/*******************************************************************************
 * Compile-time config
 *
 * Board: Waveshare ESP32-P4-WIFI6-POE-ETH Rev 2.0
 * Power: TFT VCC=5V, GND=GND, LED=3.3V (backlight is NOT a GPIO)
 *
 * TEST_MODE:
 *   0  pin walk — no SPI. Meter each TFT pin. Serial says LOW/HIGH.
 *   1  ILI9488 color test (tag hw-lcd-working)
 *   2  read controller ID on MISO (wire TFT SDO to GPIO1)
 *   3  eight-button smoketest (Stage 1)
 *   4  P4Bench — performance suite (serial console)
 ******************************************************************************/
#define TEST_MODE               4

#define PIN_LCD_CS              GPIO_NUM_22
#define PIN_LCD_RST             GPIO_NUM_5
#define PIN_LCD_DC              GPIO_NUM_4
#define PIN_LCD_MOSI            GPIO_NUM_36
#define PIN_LCD_SCK             GPIO_NUM_32
#define PIN_LCD_MISO            GPIO_NUM_1   /* TEST_MODE 2 only */

/* Left / inner header column. Each switch to GND. Pressed = LOW. */
#define PIN_BTN_UP              GPIO_NUM_20
#define PIN_BTN_DOWN            GPIO_NUM_6
#define PIN_BTN_LEFT            GPIO_NUM_3
#define PIN_BTN_RIGHT           GPIO_NUM_2
#define PIN_BTN_A               GPIO_NUM_33
#define PIN_BTN_B               GPIO_NUM_26
#define PIN_BTN_SELECT          GPIO_NUM_48
#define PIN_BTN_START           GPIO_NUM_47
#define BTN_DEBOUNCE_SAMPLES    3
#define BTN_POLL_MS             10

#define PIN_LCD_SD_CS           GPIO_NUM_NC

#define LCD_H_RES               480
#define LCD_V_RES               320
#define LCD_SPI_HOST            SPI2_HOST
#define LCD_SPI_HZ              (60 * 1000 * 1000)  /* match lcd_hw.h everyday clock */
#define LCD_MADCTL              0x28  /* MV|BGR landscape; try 0x48 / 0x68 / 0xE8 if rotated */

#define DELAY_RST_LOW_MS        80
#define DELAY_RST_HIGH_MS       250
#define DELAY_AFTER_CMD_MS      5
#define DELAY_SLPOUT_MS         250
#define COLOR_HOLD_MS           1500
#define PINWALK_HOLD_MS         2000

#define COL_BLACK               0x0000
#define COL_WHITE               0xFFFF
#define COL_RED                 0xF800
#define COL_GREEN               0x07E0
#define COL_BLUE                0x001F
#define COL_YELLOW              0xFFE0
#define COL_GRAY                0x7BEF
#define COL_DKGRAY              0x39E7

static const char *TAG = "lcd-test";
static esp_lcd_panel_io_handle_t s_io;
static uint8_t *s_line;
static int s_bpp = 16;
static int s_verbose_lcd = 1;

static bool alloc_line(void);

static void delay_ms(int ms)
{
    if (ms <= 0) {
        return;
    }
    /* 5 ms is 0 ticks at the default 100 Hz FreeRTOS rate; vTaskDelay(0)
     * does not let IDLE run, so the task watchdog fires on CPU0. */
    TickType_t ticks = pdMS_TO_TICKS(ms);
    vTaskDelay(ticks > 0 ? ticks : 1);
}

static void pin_as_output_high(gpio_num_t pin)
{
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 1);
}

static void pinwalk_one(gpio_num_t pin, const char *name)
{
    ESP_LOGW(TAG, "TEST %s GPIO%d LOW  — meter TFT %s, expect ~0 V", name, pin, name);
    gpio_set_level(pin, 0);
    delay_ms(PINWALK_HOLD_MS);
    ESP_LOGW(TAG, "TEST %s GPIO%d HIGH — meter TFT %s, expect ~3.3 V", name, pin, name);
    gpio_set_level(pin, 1);
    delay_ms(PINWALK_HOLD_MS);
}

static void run_pinwalk(void)
{
    ESP_LOGW(TAG, "PIN WALK — no SPI. Measure at the TFT module pin, not the P4 header.");
    ESP_LOGI(TAG, "CS=GPIO%d RST=GPIO%d DC=GPIO%d MOSI=GPIO%d SCK=GPIO%d",
             PIN_LCD_CS, PIN_LCD_RST, PIN_LCD_DC, PIN_LCD_MOSI, PIN_LCD_SCK);

    pin_as_output_high(PIN_LCD_CS);
    pin_as_output_high(PIN_LCD_RST);
    pin_as_output_high(PIN_LCD_DC);
    pin_as_output_high(PIN_LCD_MOSI);
    pin_as_output_high(PIN_LCD_SCK);

    while (1) {
        pinwalk_one(PIN_LCD_CS, "CS");
        pinwalk_one(PIN_LCD_RST, "RESET");
        pinwalk_one(PIN_LCD_DC, "DC");
        pinwalk_one(PIN_LCD_MOSI, "MOSI");
        pinwalk_one(PIN_LCD_SCK, "SCK");
    }
}

static void lcd_cmd(uint8_t cmd, const uint8_t *data, size_t len)
{
    if (s_verbose_lcd) {
        if (data && len) {
            char hex[80];
            size_t n = 0;
            for (size_t i = 0; i < len && n + 4 < sizeof(hex); ++i) {
                n += (size_t)snprintf(hex + n, sizeof(hex) - n, " %02X", data[i]);
            }
            ESP_LOGI(TAG, "CMD 0x%02X%s", cmd, hex);
        } else {
            ESP_LOGI(TAG, "CMD 0x%02X", cmd);
        }
    }
    esp_err_t err = esp_lcd_panel_io_tx_param(s_io, cmd, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tx_param 0x%02X failed: %s", cmd, esp_err_to_name(err));
    }
    if (s_verbose_lcd) {
        delay_ms(DELAY_AFTER_CMD_MS);
    }
}

static void hardware_reset(void)
{
    ESP_LOGI(TAG, "hardware RESET GPIO%d LOW %d ms then HIGH", PIN_LCD_RST, DELAY_RST_LOW_MS);
    pin_as_output_high(PIN_LCD_RST);
    delay_ms(10);
    gpio_set_level(PIN_LCD_RST, 0);
    delay_ms(DELAY_RST_LOW_MS);
    gpio_set_level(PIN_LCD_RST, 1);
    delay_ms(DELAY_RST_HIGH_MS);
}

static bool setup_spi(bool with_miso)
{
    gpio_num_t miso = with_miso ? PIN_LCD_MISO : GPIO_NUM_NC;
    ESP_LOGI(TAG, "SPI init SCK=%d MOSI=%d MISO=%d CS=%d DC=%d  %d Hz",
             PIN_LCD_SCK, PIN_LCD_MOSI, miso, PIN_LCD_CS, PIN_LCD_DC, LCD_SPI_HZ);

    gpio_reset_pin(PIN_LCD_SCK);
    gpio_reset_pin(PIN_LCD_MOSI);
    gpio_reset_pin(PIN_LCD_CS);
    gpio_reset_pin(PIN_LCD_DC);
    if (with_miso) {
        gpio_reset_pin(PIN_LCD_MISO);
    }

    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_LCD_SCK,
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = miso,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 3,
    };
    esp_err_t err = spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(err));
        return false;
    }

    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_LCD_CS,
        .dc_gpio_num = PIN_LCD_DC,
        .pclk_hz = LCD_SPI_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_config, &s_io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel_io_spi failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

static void init_common_sleep_on(void)
{
    lcd_cmd(LCD_CMD_SLPOUT, NULL, 0);
    delay_ms(DELAY_SLPOUT_MS);
    lcd_cmd(LCD_CMD_INVOFF, NULL, 0);
    lcd_cmd(LCD_CMD_DISPON, NULL, 0);
    delay_ms(100);
}

static void init_st7796(void)
{
    ESP_LOGW(TAG, "---- INIT ST7796 16-bit RGB565 ----");
    s_bpp = 16;
    hardware_reset();
    lcd_cmd(LCD_CMD_SWRESET, NULL, 0);
    delay_ms(DELAY_RST_HIGH_MS);
    lcd_cmd(LCD_CMD_COLMOD, (uint8_t[]){0x55}, 1);
    lcd_cmd(LCD_CMD_MADCTL, (uint8_t[]){LCD_MADCTL}, 1);
    init_common_sleep_on();
}

static void init_ili9488(void)
{
    ESP_LOGW(TAG, "---- INIT ILI9488 18-bit RGB666 ----");
    s_bpp = 18;
    hardware_reset();
    lcd_cmd(LCD_CMD_SWRESET, NULL, 0);
    delay_ms(DELAY_RST_HIGH_MS);
    lcd_cmd(0xC0, (uint8_t[]){0x17, 0x15}, 2);
    lcd_cmd(0xC1, (uint8_t[]){0x41}, 1);
    lcd_cmd(0xC5, (uint8_t[]){0x00, 0x12, 0x80}, 3);
    lcd_cmd(LCD_CMD_MADCTL, (uint8_t[]){LCD_MADCTL}, 1);
    lcd_cmd(LCD_CMD_COLMOD, (uint8_t[]){0x66}, 1);
    lcd_cmd(0xB0, (uint8_t[]){0x80}, 1);
    lcd_cmd(0xB1, (uint8_t[]){0xA0}, 1);
    lcd_cmd(0xB4, (uint8_t[]){0x02}, 1);
    lcd_cmd(0xB6, (uint8_t[]){0x02, 0x02, 0x3B}, 3);
    lcd_cmd(0xB7, (uint8_t[]){0xC6}, 1);
    lcd_cmd(0xF7, (uint8_t[]){0xA9, 0x51, 0x2C, 0x02}, 4);
    init_common_sleep_on();
}

static void set_window(int x0, int y0, int x1, int y1)
{
    uint8_t caset[] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
    uint8_t raset[] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
    lcd_cmd(LCD_CMD_CASET, caset, 4);
    lcd_cmd(LCD_CMD_RASET, raset, 4);
}

static void rgb565_to_666(uint16_t px, uint8_t out[3])
{
    out[0] = (uint8_t)((px >> 8) & 0xF8);
    out[1] = (uint8_t)((px >> 3) & 0xFC);
    out[2] = (uint8_t)((px << 3) & 0xF8);
}

static void fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (w <= 0 || h <= 0 || !s_io || !s_line) {
        return;
    }
    if (s_bpp == 16) {
        uint16_t packed = (uint16_t)((color << 8) | (color >> 8));
        uint16_t *line = (uint16_t *)s_line;
        for (int i = 0; i < w; ++i) {
            line[i] = packed;
        }
    } else {
        uint8_t pix[3];
        rgb565_to_666(color, pix);
        for (int i = 0; i < w; ++i) {
            s_line[i * 3 + 0] = pix[0];
            s_line[i * 3 + 1] = pix[1];
            s_line[i * 3 + 2] = pix[2];
        }
    }
    set_window(x, y, x + w - 1, y + h - 1);
    size_t nbytes = (s_bpp == 16) ? (size_t)w * 2 : (size_t)w * 3;
    /* 0x2C every row rewinds to (x,y) — only the last line stays visible. */
    for (int row = 0; row < h; ++row) {
        uint8_t cmd = (row == 0) ? LCD_CMD_RAMWR : LCD_CMD_RAMWRC;
        esp_err_t err = esp_lcd_panel_io_tx_color(s_io, cmd, s_line, nbytes);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "RAMWR failed: %s", esp_err_to_name(err));
            return;
        }
    }
}

static void fill_screen(uint16_t color)
{
    fill_rect(0, 0, LCD_H_RES, LCD_V_RES, color);
}

static void line_set_px(int i, uint16_t color)
{
    if (s_bpp == 16) {
        uint16_t packed = (uint16_t)((color << 8) | (color >> 8));
        ((uint16_t *)s_line)[i] = packed;
    } else {
        uint8_t pix[3];
        rgb565_to_666(color, pix);
        s_line[i * 3 + 0] = pix[0];
        s_line[i * 3 + 1] = pix[1];
        s_line[i * 3 + 2] = pix[2];
    }
}

static void blit_span(int x, int y, int w)
{
    set_window(x, y, x + w - 1, y);
    size_t nbytes = (s_bpp == 16) ? (size_t)w * 2 : (size_t)w * 3;
    esp_err_t err = esp_lcd_panel_io_tx_color(s_io, LCD_CMD_RAMWR, s_line, nbytes);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "blit failed: %s", esp_err_to_name(err));
    }
}

static void draw_char(int x, int y, char c, uint16_t fg, uint16_t bg)
{
    const uint8_t *g = font8_glyph(c);
    const int scale = 2;
    const int cw = 8 * scale;
    for (int row = 0; row < 8; ++row) {
        uint8_t bits = g[row];
        for (int col = 0; col < 8; ++col) {
            uint16_t color = (bits & (uint8_t)(0x80 >> col)) ? fg : bg;
            for (int sx = 0; sx < scale; ++sx) {
                line_set_px(col * scale + sx, color);
            }
        }
        for (int sy = 0; sy < scale; ++sy) {
            blit_span(x, y + row * scale + sy, cw);
        }
    }
}

static void draw_string(int x, int y, const char *s, uint16_t fg, uint16_t bg)
{
    while (*s) {
        draw_char(x, y, *s, fg, bg);
        x += 16;
        s++;
    }
}

static bool bringup_lcd(void)
{
    s_verbose_lcd = 1;
    hardware_reset();
    if (!setup_spi(false) || !alloc_line()) {
        return false;
    }
    init_ili9488();
    s_verbose_lcd = 0;
    return true;
}

static void color_cycle(const char *driver)
{
    const struct {
        const char *name;
        uint16_t color;
    } tests[] = {
        {"RED", COL_RED},
        {"GREEN", COL_GREEN},
        {"BLUE", COL_BLUE},
        {"BLACK", COL_BLACK},
        {"WHITE", COL_WHITE},
    };
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        ESP_LOGI(TAG, "%s pattern %s", driver, tests[i].name);
        fill_screen(tests[i].color);
        fill_rect(0, 0, 40, 40, COL_YELLOW);
        fill_rect(LCD_H_RES - 40, 0, 40, 40, COL_RED);
        fill_rect(0, LCD_V_RES - 40, 40, 40, COL_GREEN);
        fill_rect(LCD_H_RES - 40, LCD_V_RES - 40, 40, 40, COL_BLUE);
        delay_ms(COLOR_HOLD_MS);
    }
}

static bool alloc_line(void)
{
    s_line = heap_caps_malloc((size_t)LCD_H_RES * 3, MALLOC_CAP_DMA);
    if (!s_line) {
        s_line = malloc((size_t)LCD_H_RES * 3);
    }
    if (!s_line) {
        ESP_LOGE(TAG, "no memory for line buffer");
        return false;
    }
    return true;
}

static void run_lcd_test(void)
{
    if (!bringup_lcd()) {
        while (1) {
            delay_ms(5000);
        }
    }
    while (1) {
        color_cycle("ILI9488");
    }
}

typedef struct {
    const char *name;
    gpio_num_t pin;
    int stable;
    int candidate;
    int count;
    int drawn;
} btn_t;

static void draw_btn_row(int index, const btn_t *b)
{
    int y = 20 + index * 36;
    char gpio[12];
    snprintf(gpio, sizeof(gpio), "GPIO%d", b->pin);
    draw_string(16, y, b->name, COL_WHITE, COL_BLACK);
    draw_string(160, y, gpio, COL_GRAY, COL_BLACK);
    draw_string(320, y, b->stable ? "[1]" : "[0]", b->stable ? COL_GREEN : COL_GRAY, COL_BLACK);
    fill_rect(400, y, 28, 28, b->stable ? COL_GREEN : COL_DKGRAY);
    vTaskDelay(1);
}

static void run_button_test(void)
{
    btn_t btns[] = {
        {"UP", PIN_BTN_UP, 0, 0, 0, -1},
        {"DOWN", PIN_BTN_DOWN, 0, 0, 0, -1},
        {"LEFT", PIN_BTN_LEFT, 0, 0, 0, -1},
        {"RIGHT", PIN_BTN_RIGHT, 0, 0, 0, -1},
        {"A", PIN_BTN_A, 0, 0, 0, -1},
        {"B", PIN_BTN_B, 0, 0, 0, -1},
        {"SELECT", PIN_BTN_SELECT, 0, 0, 0, -1},
        {"START", PIN_BTN_START, 0, 0, 0, -1},
    };
    const int nbtn = (int)(sizeof(btns) / sizeof(btns[0]));

    ESP_LOGW(TAG, "BUTTON TEST — each switch: GPIO to GND, internal pull-up, pressed=LOW");
    for (int i = 0; i < nbtn; ++i) {
        ESP_LOGI(TAG, "  %s GPIO%d", btns[i].name, btns[i].pin);
        gpio_reset_pin(btns[i].pin);
        gpio_set_direction(btns[i].pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode(btns[i].pin, GPIO_PULLUP_ONLY);
    }

    if (!bringup_lcd()) {
        while (1) {
            delay_ms(5000);
        }
    }

    fill_screen(COL_BLACK);
    draw_string(16, 8, "BUTTONS", COL_YELLOW, COL_BLACK);
    draw_string(176, 8, "PRESS=1", COL_GRAY, COL_BLACK);
    for (int i = 0; i < nbtn; ++i) {
        draw_btn_row(i, &btns[i]);
        btns[i].drawn = btns[i].stable;
    }

    while (1) {
        int any = 0;
        for (int i = 0; i < nbtn; ++i) {
            int raw = gpio_get_level(btns[i].pin) == 0;
            if (raw == btns[i].candidate) {
                if (btns[i].count < BTN_DEBOUNCE_SAMPLES) {
                    btns[i].count++;
                }
            } else {
                btns[i].candidate = raw;
                btns[i].count = 1;
            }
            if (btns[i].count >= BTN_DEBOUNCE_SAMPLES && btns[i].stable != btns[i].candidate) {
                btns[i].stable = btns[i].candidate;
                any = 1;
            }
        }
        if (any) {
            ESP_LOGI(TAG, "UP=%d DOWN=%d LEFT=%d RIGHT=%d A=%d B=%d SELECT=%d START=%d",
                     btns[0].stable, btns[1].stable, btns[2].stable,
                     btns[3].stable, btns[4].stable, btns[5].stable,
                     btns[6].stable, btns[7].stable);
            for (int i = 0; i < nbtn; ++i) {
                if (btns[i].drawn != btns[i].stable) {
                    draw_btn_row(i, &btns[i]);
                    btns[i].drawn = btns[i].stable;
                }
            }
        }
        delay_ms(BTN_POLL_MS);
    }
}

static void dump_rx(uint8_t cmd, size_t len)
{
    uint8_t buf[8] = {0};
    if (len > sizeof(buf)) {
        len = sizeof(buf);
    }
    esp_err_t err = esp_lcd_panel_io_rx_param(s_io, cmd, buf, len);
    char hex[40];
    size_t n = 0;
    for (size_t i = 0; i < len && n + 4 < sizeof(hex); ++i) {
        n += (size_t)snprintf(hex + n, sizeof(hex) - n, " %02X", buf[i]);
    }
    ESP_LOGW(TAG, "READ cmd 0x%02X -> %s%s", cmd, esp_err_to_name(err), hex);
}

static void run_id_read(void)
{
    ESP_LOGW(TAG, "ID READ — wire TFT SDO/MISO to GPIO%d (left header column)", PIN_LCD_MISO);
    hardware_reset();
    if (!setup_spi(true)) {
        while (1) {
            delay_ms(5000);
        }
    }
    while (1) {
        ESP_LOGI(TAG, "reading RDDID 0x04 and RDID4 0xD3 (00/FF usually means no SDO path)");
        dump_rx(0x04, 4);
        dump_rx(0xD3, 4);
        delay_ms(3000);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "boot  TEST_MODE=%d  (0=pinwalk 1=lcd 2=idread 3=buttons 4=p4bench)", TEST_MODE);
    ESP_LOGI(TAG, "LCD CS=%d RST=%d DC=%d MOSI=%d SCK=%d",
             PIN_LCD_CS, PIN_LCD_RST, PIN_LCD_DC, PIN_LCD_MOSI, PIN_LCD_SCK);

    if (PIN_LCD_SD_CS >= 0) {
        pin_as_output_high(PIN_LCD_SD_CS);
    }

    if (TEST_MODE == 0) {
        run_pinwalk();
    } else if (TEST_MODE == 2) {
        run_id_read();
    } else if (TEST_MODE == 3) {
        run_button_test();
    } else if (TEST_MODE == 4) {
        p4bench_app_main();
    } else {
        run_lcd_test();
    }
}
