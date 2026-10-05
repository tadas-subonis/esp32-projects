#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_H_RES               480
#define LCD_V_RES               320
#define LCD_SPI_HOST            SPI2_HOST
#define LCD_SPI_HZ_DEFAULT      (60 * 1000 * 1000)  /* eye-STABLE OC; datasheet max write 20 MHz */

#define PIN_LCD_CS              GPIO_NUM_22
#define PIN_LCD_RST             GPIO_NUM_5
#define PIN_LCD_DC              GPIO_NUM_4
#define PIN_LCD_MOSI            GPIO_NUM_36
#define PIN_LCD_SCK             GPIO_NUM_32
#define PIN_LCD_MISO            GPIO_NUM_1

/* Wire bpp for ILI9488 COLMOD 0x66 — not the RGB565 framebuffer bpp. */
#define LCD_WIRE_BPP            18
#define LCD_BYTES_PER_PIXEL     3
#define LCD_FULL_FRAME_BYTES    ((size_t)LCD_H_RES * LCD_V_RES * LCD_BYTES_PER_PIXEL)

#define COL_BLACK               0x0000
#define COL_WHITE               0xFFFF
#define COL_RED                 0xF800
#define COL_GREEN               0x07E0
#define COL_BLUE                0x001F
#define COL_YELLOW              0xFFE0
#define COL_CYAN                0x07FF
#define COL_MAGENTA             0xF81F
#define COL_GRAY                0x7BEF
#define COL_DKGRAY              0x39E7
#define COL_ORANGE              0xFD20

typedef void (*lcd_trans_done_cb_t)(void *user);

typedef struct {
    esp_lcd_panel_io_handle_t io;
    uint8_t *line;          /* DMA-capable line / chunk scratch */
    size_t line_cap;        /* bytes */
    int bpp;                /* wire bpp: 16 or 18 */
    int spi_hz;             /* requested */
    int spi_hz_actual;      /* from spi_device_get_actual_freq() */
    int madctl;
    int verbose;
    uint64_t bytes_sent;    /* cumulative wire bytes this session */
    uint32_t xfer_fails;    /* failed tx_color / tx_param count */
    volatile int xfer_busy;
    lcd_trans_done_cb_t done_cb;
    void *done_user;
} lcd_hw_t;

bool lcd_hw_init(lcd_hw_t *lcd, int spi_hz, size_t max_chunk_rows);
void lcd_hw_deinit(lcd_hw_t *lcd);

/* Reconfigure SPI clock via full panel re-init. Updates spi_hz_actual. */
bool lcd_hw_set_spi_hz(lcd_hw_t *lcd, int spi_hz);
bool lcd_hw_reinit_at_hz(lcd_hw_t *lcd, int spi_hz);
int lcd_hw_get_actual_spi_hz(const lcd_hw_t *lcd);

void lcd_hw_set_done_cb(lcd_hw_t *lcd, lcd_trans_done_cb_t cb, void *user);
void lcd_hw_wait_idle(lcd_hw_t *lcd);

void lcd_hw_cmd(lcd_hw_t *lcd, uint8_t cmd, const uint8_t *data, size_t len);
void lcd_hw_set_window(lcd_hw_t *lcd, int x0, int y0, int x1, int y1);

/* Pack RGB565 into wire format at dst (3 bytes for ILI9488). */
void lcd_hw_pack_rgb565(lcd_hw_t *lcd, uint16_t color, uint8_t out[3]);
void lcd_hw_fill_wire_span(lcd_hw_t *lcd, uint8_t *dst, int count, uint16_t color);

/* Transfer a solid rectangle (uses internal line buffer, chunked by rows). */
bool lcd_hw_fill_rect(lcd_hw_t *lcd, int x, int y, int w, int h, uint16_t color);
bool lcd_hw_fill_screen(lcd_hw_t *lcd, uint16_t color);

/*
 * Transfer RGB565 source pixels as a rectangle.
 * src is tightly packed RGB565 (host endian). Converts to wire format.
 * chunk_rows: how many rows per DMA burst (1..h).
 * Returns wire bytes actually transmitted.
 */
size_t lcd_hw_blit_rgb565(lcd_hw_t *lcd, int x, int y, int w, int h,
                          const uint16_t *src, int src_stride_px, int chunk_rows);

/* Transfer already-packed wire bytes (RGB666 triples). */
size_t lcd_hw_blit_wire(lcd_hw_t *lcd, int x, int y, int w, int h,
                        const uint8_t *wire, int wire_stride_bytes, int chunk_rows);

int lcd_hw_bytes_per_pixel(const lcd_hw_t *lcd);
size_t lcd_hw_rect_wire_bytes(const lcd_hw_t *lcd, int w, int h);

#ifdef __cplusplus
}
#endif
