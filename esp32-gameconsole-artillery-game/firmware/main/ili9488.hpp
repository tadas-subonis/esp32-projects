#pragma once

#include "esp_err.h"

#include <cstdint>

namespace tc {

esp_err_t ili9488_init();
void ili9488_flush_rect(int x, int y, int w, int h, const uint16_t* fb);
bool ili9488_ready();

}  // namespace tc
