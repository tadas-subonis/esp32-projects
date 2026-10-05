#include "ili9488.hpp"

#include "device_hal.hpp"

#include "artillery/config.hpp"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace tc {
namespace {

const char* TAG = "ili9488";

constexpr int kSpiHz = 40 * 1000 * 1000;
constexpr int kStripRows = 16;
constexpr size_t kLineBytes = static_cast<size_t>(artillery::kWidth) * 3;
constexpr size_t kStripBytes = kLineBytes * kStripRows;

esp_lcd_panel_io_handle_t s_io = nullptr;
uint8_t* s_strip[2] = {nullptr, nullptr};
int s_phase = 0;
bool s_ready = false;

void delay_ms(int ms)
{
    if (ms <= 0) {
        return;
    }
    TickType_t ticks = pdMS_TO_TICKS(ms);
    vTaskDelay(ticks > 0 ? ticks : 1);
}

esp_err_t cmd(uint8_t c, const uint8_t* data = nullptr, size_t n = 0)
{
    return esp_lcd_panel_io_tx_param(s_io, c, data, n);
}

void rgb565_to_666(uint16_t px, uint8_t out[3])
{
    out[0] = static_cast<uint8_t>((px >> 8) & 0xF8);
    out[1] = static_cast<uint8_t>((px >> 3) & 0xFC);
    out[2] = static_cast<uint8_t>((px << 3) & 0xF8);
}

esp_err_t set_window(int x0, int y0, int x1, int y1)
{
    const uint8_t caset[4] = {
        static_cast<uint8_t>(x0 >> 8),
        static_cast<uint8_t>(x0),
        static_cast<uint8_t>(x1 >> 8),
        static_cast<uint8_t>(x1),
    };
    const uint8_t raset[4] = {
        static_cast<uint8_t>(y0 >> 8),
        static_cast<uint8_t>(y0),
        static_cast<uint8_t>(y1 >> 8),
        static_cast<uint8_t>(y1),
    };
    esp_err_t err = cmd(LCD_CMD_CASET, caset, 4);
    if (err != ESP_OK) {
        return err;
    }
    return cmd(LCD_CMD_RASET, raset, 4);
}

void hardware_reset()
{
    gpio_reset_pin(static_cast<gpio_num_t>(Pins::kLcdReset));
    gpio_set_direction(static_cast<gpio_num_t>(Pins::kLcdReset), GPIO_MODE_OUTPUT);
    gpio_set_level(static_cast<gpio_num_t>(Pins::kLcdReset), 1);
    delay_ms(10);
    gpio_set_level(static_cast<gpio_num_t>(Pins::kLcdReset), 0);
    delay_ms(80);
    gpio_set_level(static_cast<gpio_num_t>(Pins::kLcdReset), 1);
    delay_ms(250);
}

}  // namespace

bool ili9488_ready() { return s_ready; }

esp_err_t ili9488_init()
{
    gpio_reset_pin(static_cast<gpio_num_t>(Pins::kLcdSclk));
    gpio_reset_pin(static_cast<gpio_num_t>(Pins::kLcdMosi));
    gpio_reset_pin(static_cast<gpio_num_t>(Pins::kLcdCs));
    gpio_reset_pin(static_cast<gpio_num_t>(Pins::kLcdDc));

    hardware_reset();

    spi_bus_config_t buscfg = {};
    buscfg.sclk_io_num = Pins::kLcdSclk;
    buscfg.mosi_io_num = Pins::kLcdMosi;
    buscfg.miso_io_num = Pins::kLcdMiso;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = static_cast<int>(kStripBytes);

    esp_err_t err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi bus: %s", esp_err_to_name(err));
        return err;
    }

    esp_lcd_panel_io_spi_config_t iocfg = {};
    iocfg.cs_gpio_num = static_cast<gpio_num_t>(Pins::kLcdCs);
    iocfg.dc_gpio_num = static_cast<gpio_num_t>(Pins::kLcdDc);
    iocfg.spi_mode = 0;
    iocfg.pclk_hz = kSpiHz;
    iocfg.trans_queue_depth = 1;
    iocfg.lcd_cmd_bits = 8;
    iocfg.lcd_param_bits = 8;
    err = esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(SPI2_HOST), &iocfg, &s_io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel io: %s", esp_err_to_name(err));
        return err;
    }

    s_strip[0] = static_cast<uint8_t*>(heap_caps_malloc(kStripBytes, MALLOC_CAP_DMA));
    s_strip[1] = static_cast<uint8_t*>(heap_caps_malloc(kStripBytes, MALLOC_CAP_DMA));
    if (s_strip[0] == nullptr) {
        s_strip[0] = static_cast<uint8_t*>(heap_caps_malloc(kStripBytes, MALLOC_CAP_8BIT));
    }
    if (s_strip[1] == nullptr) {
        s_strip[1] = static_cast<uint8_t*>(heap_caps_malloc(kStripBytes, MALLOC_CAP_8BIT));
    }
    if (s_strip[0] == nullptr || s_strip[1] == nullptr) {
        ESP_LOGE(TAG, "line buffer alloc failed");
        return ESP_ERR_NO_MEM;
    }

    cmd(LCD_CMD_SWRESET);
    delay_ms(250);
    const uint8_t c0[] = {0x17, 0x15};
    cmd(0xC0, c0, sizeof(c0));
    const uint8_t c1[] = {0x41};
    cmd(0xC1, c1, sizeof(c1));
    const uint8_t c5[] = {0x00, 0x12, 0x80};
    cmd(0xC5, c5, sizeof(c5));
    const uint8_t madctl = 0x28;  // MV|BGR landscape
    cmd(LCD_CMD_MADCTL, &madctl, 1);
    const uint8_t colmod = 0x66;  // 18-bit RGB666
    cmd(LCD_CMD_COLMOD, &colmod, 1);
    const uint8_t b0[] = {0x80};
    cmd(0xB0, b0, sizeof(b0));
    const uint8_t b1[] = {0xA0};
    cmd(0xB1, b1, sizeof(b1));
    const uint8_t b4[] = {0x02};
    cmd(0xB4, b4, sizeof(b4));
    const uint8_t b6[] = {0x02, 0x02, 0x3B};
    cmd(0xB6, b6, sizeof(b6));
    const uint8_t b7[] = {0xC6};
    cmd(0xB7, b7, sizeof(b7));
    const uint8_t f7[] = {0xA9, 0x51, 0x2C, 0x02};
    cmd(0xF7, f7, sizeof(f7));
    cmd(LCD_CMD_SLPOUT);
    delay_ms(250);
    cmd(LCD_CMD_INVOFF);
    cmd(LCD_CMD_DISPON);
    delay_ms(100);

    s_ready = true;
    ESP_LOGI(TAG, "ready 480x320 RGB666 SPI@40MHz CS=%d RST=%d DC=%d MOSI=%d SCK=%d", Pins::kLcdCs,
             Pins::kLcdReset, Pins::kLcdDc, Pins::kLcdMosi, Pins::kLcdSclk);
    return ESP_OK;
}

void ili9488_flush_rect(int x, int y, int w, int h, const uint16_t* fb)
{
    if (!s_ready || s_strip[0] == nullptr || s_strip[1] == nullptr || fb == nullptr || w <= 0 || h <= 0) {
        return;
    }
    x = artillery::clamp_int(x, 0, artillery::kWidth - 1);
    y = artillery::clamp_int(y, 0, artillery::kHeight - 1);
    if (x + w > artillery::kWidth) {
        w = artillery::kWidth - x;
    }
    if (y + h > artillery::kHeight) {
        h = artillery::kHeight - y;
    }

    // RGB666 DMA length is 3*w*rows; w multiple of 4 keeps the byte count 4-aligned.
    int x1 = x + w;
    x &= ~3;
    x1 = (x1 + 3) & ~3;
    if (x1 > artillery::kWidth) {
        x1 = artillery::kWidth;
    }
    w = x1 - x;
    if (w <= 0) {
        return;
    }

    if (set_window(x, y, x + w - 1, y + h - 1) != ESP_OK) {
        return;
    }

    const size_t row_bytes = static_cast<size_t>(w) * 3;
    int sent = 0;
    while (sent < h) {
        const int rows = (h - sent > kStripRows) ? kStripRows : (h - sent);
        uint8_t* dst = s_strip[s_phase];
        for (int row = 0; row < rows; ++row) {
            const uint16_t* src = fb + (y + sent + row) * artillery::kWidth + x;
            for (int col = 0; col < w; ++col) {
                rgb565_to_666(src[col], dst);
                dst += 3;
            }
        }
        const uint8_t ramwr = (sent == 0) ? LCD_CMD_RAMWR : LCD_CMD_RAMWRC;
        if (esp_lcd_panel_io_tx_color(s_io, ramwr, s_strip[s_phase],
                                     row_bytes * static_cast<size_t>(rows)) != ESP_OK) {
            return;
        }
        s_phase ^= 1;
        sent += rows;
    }
    cmd(LCD_CMD_NOP);
}

}  // namespace tc
