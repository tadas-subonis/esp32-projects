#include "lcd_hw.h"

#include <stdlib.h>
#include <string.h>

#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "lcd_hw";

#define DELAY_RST_LOW_MS        80
#define DELAY_RST_HIGH_MS       250
#define DELAY_AFTER_CMD_MS      5
#define DELAY_SLPOUT_MS         250
#define LCD_MADCTL_DEFAULT      0x28

static void delay_ms(int ms)
{
    if (ms <= 0) {
        return;
    }
    TickType_t ticks = pdMS_TO_TICKS(ms);
    vTaskDelay(ticks > 0 ? ticks : 1);
}

static void pin_as_output_high(gpio_num_t pin)
{
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 1);
}

static bool on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    lcd_hw_t *lcd = (lcd_hw_t *)user_ctx;
    lcd->xfer_busy = 0;
    if (lcd->done_cb) {
        lcd->done_cb(lcd->done_user);
    }
    return false;
}

static void hardware_reset(void)
{
    pin_as_output_high(PIN_LCD_RST);
    delay_ms(10);
    gpio_set_level(PIN_LCD_RST, 0);
    delay_ms(DELAY_RST_LOW_MS);
    gpio_set_level(PIN_LCD_RST, 1);
    delay_ms(DELAY_RST_HIGH_MS);
}

void lcd_hw_cmd(lcd_hw_t *lcd, uint8_t cmd, const uint8_t *data, size_t len)
{
    if (!lcd || !lcd->io) {
        return;
    }
    if (lcd->verbose) {
        ESP_LOGI(TAG, "CMD 0x%02X len=%u", cmd, (unsigned)len);
    }
    esp_err_t err = esp_lcd_panel_io_tx_param(lcd->io, cmd, data, len);
    if (err != ESP_OK) {
        lcd->xfer_fails++;
        ESP_LOGE(TAG, "tx_param 0x%02X failed: %s", cmd, esp_err_to_name(err));
    }
    if (lcd->verbose) {
        delay_ms(DELAY_AFTER_CMD_MS);
    }
}

void lcd_hw_set_window(lcd_hw_t *lcd, int x0, int y0, int x1, int y1)
{
    uint8_t caset[] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
    uint8_t raset[] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
    lcd_hw_cmd(lcd, LCD_CMD_CASET, caset, 4);
    lcd_hw_cmd(lcd, LCD_CMD_RASET, raset, 4);
}

void lcd_hw_pack_rgb565(lcd_hw_t *lcd, uint16_t color, uint8_t out[3])
{
    if (lcd->bpp == 16) {
        out[0] = (uint8_t)(color >> 8);
        out[1] = (uint8_t)color;
        out[2] = 0;
    } else {
        out[0] = (uint8_t)((color >> 8) & 0xF8);
        out[1] = (uint8_t)((color >> 3) & 0xFC);
        out[2] = (uint8_t)((color << 3) & 0xF8);
    }
}

void lcd_hw_fill_wire_span(lcd_hw_t *lcd, uint8_t *dst, int count, uint16_t color)
{
    if (lcd->bpp == 16) {
        uint16_t packed = (uint16_t)((color << 8) | (color >> 8));
        uint16_t *line = (uint16_t *)dst;
        for (int i = 0; i < count; ++i) {
            line[i] = packed;
        }
    } else {
        uint8_t pix[3];
        lcd_hw_pack_rgb565(lcd, color, pix);
        for (int i = 0; i < count; ++i) {
            dst[i * 3 + 0] = pix[0];
            dst[i * 3 + 1] = pix[1];
            dst[i * 3 + 2] = pix[2];
        }
    }
}

int lcd_hw_bytes_per_pixel(const lcd_hw_t *lcd)
{
    return (lcd->bpp == 16) ? 2 : 3;
}

size_t lcd_hw_rect_wire_bytes(const lcd_hw_t *lcd, int w, int h)
{
    if (w <= 0 || h <= 0) {
        return 0;
    }
    return (size_t)w * (size_t)h * (size_t)lcd_hw_bytes_per_pixel(lcd);
}

static void init_ili9488(lcd_hw_t *lcd)
{
    ESP_LOGW(TAG, "INIT ILI9488 RGB666  SPI=%d Hz  MADCTL=0x%02X", lcd->spi_hz, lcd->madctl);
    lcd->bpp = 18;
    hardware_reset();
    lcd_hw_cmd(lcd, LCD_CMD_SWRESET, NULL, 0);
    delay_ms(DELAY_RST_HIGH_MS);
    lcd_hw_cmd(lcd, 0xC0, (uint8_t[]){0x17, 0x15}, 2);
    lcd_hw_cmd(lcd, 0xC1, (uint8_t[]){0x41}, 1);
    lcd_hw_cmd(lcd, 0xC5, (uint8_t[]){0x00, 0x12, 0x80}, 3);
    lcd_hw_cmd(lcd, LCD_CMD_MADCTL, (uint8_t[]){(uint8_t)lcd->madctl}, 1);
    lcd_hw_cmd(lcd, LCD_CMD_COLMOD, (uint8_t[]){0x66}, 1);
    lcd_hw_cmd(lcd, 0xB0, (uint8_t[]){0x80}, 1);
    lcd_hw_cmd(lcd, 0xB1, (uint8_t[]){0xA0}, 1);
    lcd_hw_cmd(lcd, 0xB4, (uint8_t[]){0x02}, 1);
    lcd_hw_cmd(lcd, 0xB6, (uint8_t[]){0x02, 0x02, 0x3B}, 3);
    lcd_hw_cmd(lcd, 0xB7, (uint8_t[]){0xC6}, 1);
    lcd_hw_cmd(lcd, 0xF7, (uint8_t[]){0xA9, 0x51, 0x2C, 0x02}, 4);
    lcd_hw_cmd(lcd, LCD_CMD_SLPOUT, NULL, 0);
    delay_ms(DELAY_SLPOUT_MS);
    lcd_hw_cmd(lcd, LCD_CMD_INVOFF, NULL, 0);
    lcd_hw_cmd(lcd, LCD_CMD_DISPON, NULL, 0);
    delay_ms(100);
}

/* Recover SPI device handle from esp_lcd panel_io (layout: base then spi_dev). */
static spi_device_handle_t lcd_spi_dev(esp_lcd_panel_io_handle_t io)
{
    if (!io) {
        return NULL;
    }
    /* esp_lcd_panel_io_spi_t: { esp_lcd_panel_io_t base; spi_device_handle_t spi_dev; ... } */
    return *(spi_device_handle_t *)((uint8_t *)io + sizeof(esp_lcd_panel_io_t));
}

static bool lcd_create_panel_io(lcd_hw_t *lcd, int spi_hz)
{
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_LCD_CS,
        .dc_gpio_num = PIN_LCD_DC,
        .pclk_hz = spi_hz,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 4,
    };
    esp_err_t err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_config, &lcd->io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel_io_spi @ %d Hz: %s", spi_hz, esp_err_to_name(err));
        lcd->io = NULL;
        return false;
    }
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = on_color_trans_done,
    };
    esp_lcd_panel_io_register_event_callbacks(lcd->io, &cbs, lcd);

    lcd->spi_hz = spi_hz;
    int khz = 0;
    spi_device_handle_t dev = lcd_spi_dev(lcd->io);
    if (dev && spi_device_get_actual_freq(dev, &khz) == ESP_OK && khz > 0) {
        lcd->spi_hz_actual = khz * 1000;
    } else {
        lcd->spi_hz_actual = spi_hz;
    }
    ESP_LOGI(TAG, "SPI requested=%d Hz  actual=%d Hz", lcd->spi_hz, lcd->spi_hz_actual);
    return true;
}

/*
 * Soft register poke is not enough after panel_io_del: DC/CS get reset and the
 * panel can sit white while SPI TX still "succeeds". Always full HW re-init.
 */
bool lcd_hw_set_spi_hz(lcd_hw_t *lcd, int spi_hz)
{
    return lcd_hw_reinit_at_hz(lcd, spi_hz);
}

/* Full HW reset + ILI9488 init at a new SPI clock; prove with solid GREEN. */
bool lcd_hw_reinit_at_hz(lcd_hw_t *lcd, int spi_hz)
{
    if (!lcd || spi_hz <= 0) {
        return false;
    }
    if (lcd->io) {
        esp_lcd_panel_io_del(lcd->io);
        lcd->io = NULL;
    }
    if (!lcd_create_panel_io(lcd, spi_hz)) {
        return false;
    }
    lcd->verbose = 0;
    init_ili9488(lcd);
    /* Direct fill (no FB) — if this fails, clock/link is dead. */
    if (!lcd_hw_fill_screen(lcd, COL_GREEN)) {
        ESP_LOGE(TAG, "post-reclock fill GREEN failed @ %d Hz", spi_hz);
        return false;
    }
    return true;
}

int lcd_hw_get_actual_spi_hz(const lcd_hw_t *lcd)
{
    return lcd ? lcd->spi_hz_actual : 0;
}
bool lcd_hw_init(lcd_hw_t *lcd, int spi_hz, size_t max_chunk_rows)
{
    memset(lcd, 0, sizeof(*lcd));
    lcd->spi_hz = spi_hz > 0 ? spi_hz : LCD_SPI_HZ_DEFAULT;
    lcd->madctl = LCD_MADCTL_DEFAULT;
    lcd->verbose = 1;
    if (max_chunk_rows < 1) {
        max_chunk_rows = 1;
    }
    if (max_chunk_rows > LCD_V_RES) {
        max_chunk_rows = LCD_V_RES;
    }

    size_t max_xfer = LCD_H_RES * LCD_BYTES_PER_PIXEL * max_chunk_rows;
    if (max_xfer < LCD_H_RES * LCD_BYTES_PER_PIXEL) {
        max_xfer = LCD_H_RES * LCD_BYTES_PER_PIXEL;
    }

    gpio_reset_pin(PIN_LCD_SCK);
    gpio_reset_pin(PIN_LCD_MOSI);
    gpio_reset_pin(PIN_LCD_CS);
    gpio_reset_pin(PIN_LCD_DC);

    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_LCD_SCK,
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = (int)max_xfer,
    };
    esp_err_t err = spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize: %s", esp_err_to_name(err));
        return false;
    }

    if (!lcd_create_panel_io(lcd, lcd->spi_hz)) {
        return false;
    }

    lcd->line_cap = max_xfer;
    lcd->line = heap_caps_malloc(lcd->line_cap, MALLOC_CAP_DMA);
    if (!lcd->line) {
        lcd->line = malloc(lcd->line_cap);
    }
    if (!lcd->line) {
        ESP_LOGE(TAG, "no DMA/line buffer (%u bytes)", (unsigned)lcd->line_cap);
        return false;
    }

    init_ili9488(lcd);
    lcd->verbose = 0;
    ESP_LOGI(TAG, "LCD ready  %dx%d  wire=%d bpp  chunk_cap=%u bytes",
             LCD_H_RES, LCD_V_RES, lcd->bpp, (unsigned)lcd->line_cap);
    return true;
}

void lcd_hw_deinit(lcd_hw_t *lcd)
{
    if (!lcd) {
        return;
    }
    if (lcd->line) {
        free(lcd->line);
        lcd->line = NULL;
    }
    if (lcd->io) {
        esp_lcd_panel_io_del(lcd->io);
        lcd->io = NULL;
    }
    spi_bus_free(LCD_SPI_HOST);
}

void lcd_hw_set_done_cb(lcd_hw_t *lcd, lcd_trans_done_cb_t cb, void *user)
{
    lcd->done_cb = cb;
    lcd->done_user = user;
}

void lcd_hw_wait_idle(lcd_hw_t *lcd)
{
    /* Busy-wait only — vTaskDelay(1) is a full FreeRTOS tick (~10 ms) and
     * would dominate per-row SPI transfers. */
    int64_t start = esp_timer_get_time();
    while (lcd->xfer_busy) {
        if (esp_timer_get_time() - start > 2000000) {
            lcd->xfer_busy = 0;
            break;
        }
    }
}

static esp_err_t tx_color(lcd_hw_t *lcd, uint8_t cmd, const void *data, size_t len)
{
    /* esp_lcd SPI tx_color waits for the transaction to finish. Track bytes
     * only; do not add a FreeRTOS-tick wait after every burst. */
    esp_err_t err = esp_lcd_panel_io_tx_color(lcd->io, cmd, data, len);
    if (err != ESP_OK) {
        lcd->xfer_fails++;
        ESP_LOGE(TAG, "tx_color failed: %s", esp_err_to_name(err));
        return err;
    }
    lcd->bytes_sent += len;
    return ESP_OK;
}

bool lcd_hw_fill_rect(lcd_hw_t *lcd, int x, int y, int w, int h, uint16_t color)
{
    if (!lcd || !lcd->io || !lcd->line || w <= 0 || h <= 0) {
        return false;
    }
    int bpp = lcd_hw_bytes_per_pixel(lcd);
    size_t row_bytes = (size_t)w * (size_t)bpp;
    if (row_bytes > lcd->line_cap) {
        ESP_LOGE(TAG, "fill_rect width exceeds line buffer");
        return false;
    }

    lcd_hw_fill_wire_span(lcd, lcd->line, w, color);
    lcd_hw_set_window(lcd, x, y, x + w - 1, y + h - 1);

    int rows_per = (int)(lcd->line_cap / row_bytes);
    if (rows_per < 1) {
        rows_per = 1;
    }

    int row = 0;
    while (row < h) {
        int n = h - row;
        if (n > rows_per) {
            n = rows_per;
        }
        if (n == 1) {
            uint8_t cmd = (row == 0) ? LCD_CMD_RAMWR : LCD_CMD_RAMWRC;
            if (tx_color(lcd, cmd, lcd->line, row_bytes) != ESP_OK) {
                return false;
            }
        } else {
            /* replicate first row into chunk buffer */
            for (int r = 1; r < n; ++r) {
                memcpy(lcd->line + (size_t)r * row_bytes, lcd->line, row_bytes);
            }
            uint8_t cmd = (row == 0) ? LCD_CMD_RAMWR : LCD_CMD_RAMWRC;
            if (tx_color(lcd, cmd, lcd->line, row_bytes * (size_t)n) != ESP_OK) {
                return false;
            }
        }
        row += n;
    }
    return true;
}

bool lcd_hw_fill_screen(lcd_hw_t *lcd, uint16_t color)
{
    return lcd_hw_fill_rect(lcd, 0, 0, LCD_H_RES, LCD_V_RES, color);
}

size_t lcd_hw_blit_rgb565(lcd_hw_t *lcd, int x, int y, int w, int h,
                          const uint16_t *src, int src_stride_px, int chunk_rows)
{
    if (!lcd || !src || w <= 0 || h <= 0) {
        return 0;
    }
    int bpp = lcd_hw_bytes_per_pixel(lcd);
    size_t row_bytes = (size_t)w * (size_t)bpp;
    if (chunk_rows < 1) {
        chunk_rows = 1;
    }
    size_t max_rows = lcd->line_cap / row_bytes;
    if (max_rows < 1) {
        return 0;
    }
    if ((size_t)chunk_rows > max_rows) {
        chunk_rows = (int)max_rows;
    }

    lcd_hw_set_window(lcd, x, y, x + w - 1, y + h - 1);
    size_t total = 0;
    int row = 0;
    while (row < h) {
        int n = h - row;
        if (n > chunk_rows) {
            n = chunk_rows;
        }
        for (int r = 0; r < n; ++r) {
            const uint16_t *srow = src + (row + r) * src_stride_px;
            uint8_t *drow = lcd->line + (size_t)r * row_bytes;
            if (bpp == 2) {
                uint16_t *dst = (uint16_t *)drow;
                for (int col = 0; col < w; ++col) {
                    uint16_t c = srow[col];
                    dst[col] = (uint16_t)((c << 8) | (c >> 8));
                }
            } else {
                for (int col = 0; col < w; ++col) {
                    lcd_hw_pack_rgb565(lcd, srow[col], drow + col * 3);
                }
            }
        }
        size_t nbytes = row_bytes * (size_t)n;
        uint8_t cmd = (row == 0) ? LCD_CMD_RAMWR : LCD_CMD_RAMWRC;
        if (tx_color(lcd, cmd, lcd->line, nbytes) != ESP_OK) {
            return total;
        }
        total += nbytes;
        row += n;
    }
    return total;
}

size_t lcd_hw_blit_wire(lcd_hw_t *lcd, int x, int y, int w, int h,
                        const uint8_t *wire, int wire_stride_bytes, int chunk_rows)
{
    if (!lcd || !wire || w <= 0 || h <= 0) {
        return 0;
    }
    int bpp = lcd_hw_bytes_per_pixel(lcd);
    size_t row_bytes = (size_t)w * (size_t)bpp;
    if (chunk_rows < 1) {
        chunk_rows = 1;
    }
    size_t max_rows = lcd->line_cap / row_bytes;
    if (max_rows < 1) {
        return 0;
    }
    if ((size_t)chunk_rows > max_rows) {
        chunk_rows = (int)max_rows;
    }

    lcd_hw_set_window(lcd, x, y, x + w - 1, y + h - 1);
    size_t total = 0;
    int row = 0;
    while (row < h) {
        int n = h - row;
        if (n > chunk_rows) {
            n = chunk_rows;
        }
        for (int r = 0; r < n; ++r) {
            memcpy(lcd->line + (size_t)r * row_bytes,
                   wire + (size_t)(row + r) * (size_t)wire_stride_bytes,
                   row_bytes);
        }
        size_t nbytes = row_bytes * (size_t)n;
        uint8_t cmd = (row == 0) ? LCD_CMD_RAMWR : LCD_CMD_RAMWRC;
        if (tx_color(lcd, cmd, lcd->line, nbytes) != ESP_OK) {
            return total;
        }
        total += nbytes;
        row += n;
    }
    return total;
}
