#include "device_hal.hpp"

#include "ili9488.hpp"

#include "driver/gpio.h"
#include "esp_log.h"

namespace tc {
namespace {

const char* TAG = "hal";

constexpr int kButtonPins[] = {
    Pins::kBtnUp, Pins::kBtnDown, Pins::kBtnLeft, Pins::kBtnRight, Pins::kBtnA, Pins::kBtnB,
};

artillery::Buttons read_one()
{
    artillery::Buttons b;
    auto down = [](int pin) {
        return gpio_get_level(static_cast<gpio_num_t>(pin)) == 0;
    };
    b.up = down(Pins::kBtnUp);
    b.down = down(Pins::kBtnDown);
    b.left = down(Pins::kBtnLeft);
    b.right = down(Pins::kBtnRight);
    b.a = down(Pins::kBtnA);
    b.b = down(Pins::kBtnB);
    return b;
}

}  // namespace

esp_err_t DeviceHal::init()
{
    for (int pin : kButtonPins) {
        gpio_reset_pin(static_cast<gpio_num_t>(pin));
    }

    gpio_config_t io = {};
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    io.pin_bit_mask = (1ULL << Pins::kBtnUp) | (1ULL << Pins::kBtnDown) | (1ULL << Pins::kBtnLeft) |
                      (1ULL << Pins::kBtnRight) | (1ULL << Pins::kBtnA) | (1ULL << Pins::kBtnB);
    const esp_err_t btn_err = gpio_config(&io);
    if (btn_err != ESP_OK) {
        ESP_LOGW(TAG, "button gpio: %s", esp_err_to_name(btn_err));
    }

    const esp_err_t lcd_err = ili9488_init();
    if (lcd_err != ESP_OK) {
        ESP_LOGW(TAG, "LCD init failed (%s) — console-only mode", esp_err_to_name(lcd_err));
        display_ready_ = false;
        return ESP_OK;
    }
    display_ready_ = true;
    return ESP_OK;
}

artillery::Buttons DeviceHal::read_buttons() const { return read_one(); }

void DeviceHal::flush(const uint16_t* fb, const artillery::DirtyList& dirty)
{
    if (!display_ready_) {
        return;
    }
    for (int i = 0; i < dirty.count; ++i) {
        const artillery::Rect r = dirty.rects[i];
        ili9488_flush_rect(r.x, r.y, r.w, r.h, fb);
    }
}

void DeviceHal::backlight(bool)
{
    // LED is wired to 3.3 V on this POC. Do not drive a backlight GPIO.
}

}  // namespace tc
