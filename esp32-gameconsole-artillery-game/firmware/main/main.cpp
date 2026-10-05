#include "console.hpp"
#include "device_hal.hpp"
#include "game_app.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include <cstdio>

static const char* TAG = "artillery";

namespace tc {
GameApp g_app;
DeviceHal g_hal;
}

extern "C" void app_main(void)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "artillery duel boot");
    ESP_ERROR_CHECK(tc::g_hal.init());
    tc::g_app.init(&tc::g_hal);
    ESP_ERROR_CHECK(console_start());
    ESP_LOGI(TAG, "ready — Type-C UART console, buttons, LCD");

    int64_t last = esp_timer_get_time();
    while (true) {
        const int64_t now = esp_timer_get_time();
        uint32_t dt_ms = static_cast<uint32_t>((now - last) / 1000);
        if (dt_ms < 16) {
            vTaskDelay(pdMS_TO_TICKS(16 - dt_ms));
            continue;
        }
        if (dt_ms > 100) {
            dt_ms = 100;
        }
        last = now;
        tc::g_app.tick(dt_ms);
    }
}
