#pragma once

#include <M5GFX.h>
#include <M5PM1.h>
#include <M5Unified.hpp>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class DeviceHal {
public:
    M5PM1 pm1;
    M5Canvas* canvas = nullptr;

    void init();
    bool sd_inserted();

    /**
     * Raise every PMIC rail the panel and SD card need. MUST run before
     * M5.begin(): PWR_CFG auto-clears on reset and the EPD rail is off by
     * default, and an unpowered panel holds BUSY low forever.
     */
    void bring_up_power_rails();

    /** Pulse the panel's RST line (GPIO12) so it wakes out of deep sleep. */
    void reset_epd_panel();
    void show_status_screen(const char* line1, const char* line2, const char* line3);

    /**
     * Draw a colour-bar test pattern and refresh, returning the refresh duration
     * in ms (negative on failure). Exercises the panel without SD or JPEG decode.
     */
    int64_t show_test_pattern();

    /**
     * Raw Spectra BUSY pin level, for diagnostics only. Read-only: the panel
     * driver owns this pin's configuration (input_pullup) and does its own
     * bounded BUSY wait, so firmware MUST NOT gate work on this.
     */
    bool epd_busy() const;

    /**
     * Serialize e-ink SPI transfers vs SD card access (shared SPI2). LovyanGFX
     * drives the panel with direct register access and does not participate in
     * the IDF SPI driver's arbitration with sdspi, so every SD access and every
     * panel transfer must be wrapped in this lock.
     */
    bool lock_bus(TickType_t ticks = portMAX_DELAY);
    void unlock_bus();

private:
    SemaphoreHandle_t bus_mu_ = nullptr;
};

extern DeviceHal g_hal;
