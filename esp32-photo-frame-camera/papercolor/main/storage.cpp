#include "storage.hpp"

#include <dirent.h>
#include <sys/stat.h>

#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal.hpp"
#include "sdmmc_cmd.h"

static const char* TAG = "storage";

static sdmmc_card_t* s_card    = nullptr;
static bool          s_mounted = false;

esp_err_t photo_storage_init()
{
    if (s_mounted) {
        ESP_LOGI(TAG, "SD already mounted at %s", PHOTO_STORAGE_BASE_PATH);
        return ESP_OK;
    }

    const bool inserted = g_hal.sd_inserted();
    ESP_LOGI(TAG, "SD init start card_inserted=%d", (int)inserted);
    if (!inserted) {
        ESP_LOGW(TAG, "SD card not detected (CARD_DEC high) — skipping mount");
        return ESP_ERR_NOT_FOUND;
    }

    // M5GFX already powers PY_SD_PWR_EN during board autodetect; give the card time
    // to settle after our HAL re-asserts the rail.
    vTaskDelay(pdMS_TO_TICKS(50));

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files              = 8,
        .allocation_unit_size   = 16 * 1024,
    };

    // PaperColor: EPD + microSD share SPI2 (M5GFX sets bus_shared=true).
    const spi_host_device_t spi_host = SPI2_HOST;
    sdmmc_host_t host                = SDSPI_HOST_DEFAULT();
    host.slot                        = spi_host;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = GPIO_NUM_13,
        .miso_io_num     = GPIO_NUM_14,
        .sclk_io_num     = GPIO_NUM_15,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t spi_err = spi_bus_initialize(spi_host, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (spi_err == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "SPI2 already initialized (shared with e-ink) — reusing bus");
    } else if (spi_err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(spi_err));
        return spi_err;
    } else {
        ESP_LOGI(TAG, "SPI2 initialized for SD (MOSI=13 MISO=14 SCLK=15 CS=47)");
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs               = GPIO_NUM_47;
    slot_config.host_id               = spi_host;

    ESP_LOGI(TAG, "mounting FAT at %s (format_if_fail=%d)", PHOTO_STORAGE_BASE_PATH,
             (int)mount_config.format_if_mount_failed);
    esp_err_t ret =
        esp_vfs_fat_sdspi_mount(PHOTO_STORAGE_BASE_PATH, &host, &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed: %s (card_inserted=%d)", esp_err_to_name(ret), (int)inserted);
        return ret;
    }

    sdmmc_card_print_info(stdout, s_card);
    s_mounted = true;
    ESP_LOGI(TAG, "SD mounted at %s capacity=%lluMB", PHOTO_STORAGE_BASE_PATH,
             s_card ? ((unsigned long long)s_card->csd.capacity * s_card->csd.sector_size / (1024 * 1024))
                    : 0ULL);
    return ESP_OK;
}

bool photo_storage_is_mounted()
{
    return s_mounted;
}
