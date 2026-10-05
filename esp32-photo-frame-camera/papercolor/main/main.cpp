#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "hal.hpp"
#include "http_photo_server.hpp"
#include "photo_gallery.hpp"
#include "photo_frame_wifi.h"
#include "storage.hpp"
#include "wifi_ap.hpp"
#include "console.hpp"

static const char* TAG = "papercolor";

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "photo-frame receiver boot");

    g_hal.init();
    ESP_LOGI(TAG, "card_inserted=%d before SD mount", (int)g_hal.sd_inserted());

    const esp_err_t sd_err = photo_storage_init();
    if (sd_err != ESP_OK) {
        ESP_LOGW(TAG, "SD not ready (%s) inserted=%d — gallery disabled until reboot with card",
                 esp_err_to_name(sd_err), (int)g_hal.sd_inserted());
    } else {
        ESP_LOGI(TAG, "SD ready — gallery enabled");
    }

    ESP_ERROR_CHECK(wifi_ap_start());
    ESP_ERROR_CHECK(http_photo_server_start());
    ESP_ERROR_CHECK(console_start());

    // Always push a frame after boot. An empty SD used to skip refresh and leave
    // the e-ink stuck on whatever was left from the previous session / M5.begin.
    char ap_line[64];
    snprintf(ap_line, sizeof(ap_line), "AP: %s", PHOTO_FRAME_AP_SSID);
    if (photo_storage_is_mounted()) {
        g_gallery.display_most_recent();
        if (g_gallery.photo_count() == 0) {
            ESP_LOGI(TAG, "gallery empty — showing status screen");
            g_hal.show_status_screen("photo_frame_receiver", ap_line, "SD empty — waiting for photo");
        }
    } else {
        const char* sd_line =
            g_hal.sd_inserted() ? "SD mount failed — POST still works" : "No SD — POST /api/v1/photo";
        g_hal.show_status_screen("photo_frame_receiver", ap_line, sd_line);
    }

    while (true) {
        g_gallery.service();
        g_gallery.handle_buttons();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
