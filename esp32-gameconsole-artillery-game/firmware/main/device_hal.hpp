#pragma once

#include "esp_err.h"

#include <cstdint>

#include "artillery/render.hpp"

namespace tc {

struct Pins {
    static constexpr int kLcdSclk = 32;
    static constexpr int kLcdMosi = 36;
    static constexpr int kLcdMiso = -1;
    static constexpr int kLcdCs = 22;
    static constexpr int kLcdDc = 4;
    static constexpr int kLcdReset = 5;

    static constexpr int kBtnUp = 20;
    static constexpr int kBtnDown = 6;
    static constexpr int kBtnLeft = 3;
    static constexpr int kBtnRight = 2;
    static constexpr int kBtnA = 33;
    static constexpr int kBtnB = 26;
};

class DeviceHal {
public:
    esp_err_t init();
    bool display_ready() const { return display_ready_; }
    artillery::Buttons read_buttons() const;
    void flush(const uint16_t* fb, const artillery::DirtyList& dirty);
    void backlight(bool on);

private:
    bool display_ready_ = false;
};

extern DeviceHal g_hal;

}  // namespace tc
