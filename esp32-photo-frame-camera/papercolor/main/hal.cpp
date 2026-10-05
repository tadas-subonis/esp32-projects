#include "hal.hpp"

#include <cstdio>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "hal";

// Panel pins per M5GFX board_M5PaperColor autodetect. Read-only here; the panel
// driver configures and drives them.
static constexpr gpio_num_t EPD_BUSY_PIN = GPIO_NUM_11;
static constexpr gpio_num_t EPD_RST_PIN  = GPIO_NUM_12;
static constexpr int        PMIC_SDA_PIN = 3;
static constexpr int        PMIC_SCL_PIN = 2;

DeviceHal g_hal;

void DeviceHal::init()
{
    bus_mu_ = xSemaphoreCreateMutex();
    if (!bus_mu_) {
        ESP_LOGE(TAG, "bus mutex create failed");
    }

    bring_up_power_rails();

    auto cfg          = M5.config();
    cfg.clear_display = false;
    const int64_t begin_t0 = esp_timer_get_time();
    M5.begin(cfg);
    ESP_LOGI(TAG, "M5.begin took %lldms busy_pin=%d",
             (long long)((esp_timer_get_time() - begin_t0) / 1000),
             (int)gpio_get_level(EPD_BUSY_PIN));

    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    M5.Display.setRotation(3);

    canvas = new M5Canvas(&M5.Display);
    canvas->createSprite(M5.Display.width(), M5.Display.height());

    M5.Speaker.begin();
    M5.Speaker.setVolume(200);

    ESP_LOGI(TAG, "HAL ready board=%d display %dx%d busy_pin=%d card_inserted=%d",
             (int)M5.getBoard(), M5.Display.width(), M5.Display.height(),
             (int)gpio_get_level(EPD_BUSY_PIN), (int)sd_inserted());
}

void DeviceHal::bring_up_power_rails()
{
    // I2C must come up by hand: the PMIC has to be configured before M5.begin().
    if (!M5.In_I2C.begin(I2C_NUM_0, PMIC_SDA_PIN, PMIC_SCL_PIN)) {
        ESP_LOGW(TAG, "In_I2C.begin failed — PMIC may already be initialised");
    }

    const m5pm1_err_t pm_err = pm1.begin(&M5.In_I2C, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K);
    if (pm_err != M5PM1_OK) {
        ESP_LOGE(TAG, "M5PM1 begin failed err=%d — display/SD will not work", (int)pm_err);
        return;
    }

    // PWR_CFG (reg 0x06) auto-clears on every reset, so re-enable all four rails.
    // BOOST is the e-paper high-voltage rail: without it the panel never releases BUSY.
    pm1.setChargeEnable(true);
    pm1.setDcdcEnable(true);
    pm1.setLdoEnable(true);
    pm1.setBoostEnable(true);

    pm1.pinMode(M5PM1_GPIO_NUM_0, OUTPUT);
    pm1.digitalWrite(M5PM1_GPIO_NUM_0, HIGH);     // PY_EPD_EN
    pm1.pinMode(M5PM1_GPIO_NUM_3, OUTPUT);
    pm1.digitalWrite(M5PM1_GPIO_NUM_3, HIGH);     // PY_SD_PWR_EN
    pm1.pinMode(M5PM1_GPIO_NUM_4, OUTPUT);
    pm1.digitalWrite(M5PM1_GPIO_NUM_4, HIGH);     // PY_SD_DET_EN
    pm1.pinMode(M5PM1_GPIO_NUM_1, INPUT_PULLUP);  // CARD_DEC (active low)

    // Hold the rails so unplugging USB does not drop the panel/SD mid-operation.
    pm1.ldoSetPowerHold(true);
    pm1.gpioSetPowerHold(M5PM1_GPIO_NUM_0, true);
    pm1.gpioSetPowerHold(M5PM1_GPIO_NUM_3, true);

    vTaskDelay(pdMS_TO_TICKS(100));  // let the rails settle before the panel reset

    ESP_LOGI(TAG, "PMIC rails up (chg/dcdc/ldo/boost + EPD/SD) busy_pin=%d card_inserted=%d",
             (int)gpio_get_level(EPD_BUSY_PIN), (int)sd_inserted());

    reset_epd_panel();
}

void DeviceHal::reset_epd_panel()
{
    // M5Unified maps cfg.clear_display=false to Display.init_without_reset(false), so
    // M5GFX's PaperColor branch calls _pin_reset(GPIO12, false) and never pulses RST.
    // A panel left asleep from a previous run then holds BUSY low forever, which makes
    // every M5GFX _wait_busy() burn its full 20s timeout (~40s in M5.begin() alone).
    // This state survives an ESP32 soft reset, so pulse RST ourselves before init.
    gpio_set_direction(EPD_RST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(EPD_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(EPD_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(EPD_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "EPD RST pulsed busy_pin=%d (1=ready)", (int)gpio_get_level(EPD_BUSY_PIN));
}

bool DeviceHal::epd_busy() const
{
    return gpio_get_level(EPD_BUSY_PIN) == 0;
}

bool DeviceHal::lock_bus(TickType_t ticks)
{
    if (!bus_mu_) {
        return true;
    }
    return xSemaphoreTake(bus_mu_, ticks) == pdTRUE;
}

void DeviceHal::unlock_bus()
{
    if (bus_mu_) {
        xSemaphoreGive(bus_mu_);
    }
}

void DeviceHal::show_status_screen(const char* line1, const char* line2, const char* line3)
{
    if (!canvas) {
        return;
    }

    if (!lock_bus(pdMS_TO_TICKS(60000))) {
        ESP_LOGE(TAG, "status screen: bus lock failed");
        return;
    }

    const int w = canvas->width();
    const int h = canvas->height();

    canvas->fillScreen(TFT_WHITE);
    canvas->setTextDatum(middle_center);
    canvas->setTextColor(TFT_BLACK, TFT_WHITE);
    canvas->setTextSize(2);
    canvas->drawString("Photo Frame", w / 2, h / 4);
    canvas->setTextSize(1);
    if (line1 && line1[0]) {
        canvas->drawString(line1, w / 2, h / 2 - 24);
    }
    if (line2 && line2[0]) {
        canvas->drawString(line2, w / 2, h / 2);
    }
    if (line3 && line3[0]) {
        canvas->drawString(line3, w / 2, h / 2 + 24);
    }

    ESP_LOGI(TAG, "status screen refresh begin busy_pin=%d", (int)gpio_get_level(EPD_BUSY_PIN));
    const int64_t t0 = esp_timer_get_time();
    canvas->pushSprite(0, 0);  // _auto_display: endWrite() triggers the panel refresh
    const int64_t dt_ms = (esp_timer_get_time() - t0) / 1000;
    unlock_bus();
    ESP_LOGI(TAG, "status screen refreshed in %lldms busy_pin=%d", (long long)dt_ms,
             (int)gpio_get_level(EPD_BUSY_PIN));
}

int64_t DeviceHal::show_test_pattern()
{
    if (!canvas) {
        return -1;
    }
    if (!lock_bus(pdMS_TO_TICKS(60000))) {
        ESP_LOGE(TAG, "test pattern: bus lock failed");
        return -1;
    }

    const int w = canvas->width();
    const int h = canvas->height();
    static constexpr uint32_t bars[] = {TFT_BLACK, TFT_WHITE, TFT_RED,
                                        TFT_GREEN, TFT_BLUE,  TFT_YELLOW};
    const int bar_h = h / (int)(sizeof(bars) / sizeof(bars[0]));
    for (int i = 0; i < (int)(sizeof(bars) / sizeof(bars[0])); ++i) {
        canvas->fillRect(0, i * bar_h, w, bar_h, bars[i]);
    }
    canvas->setTextColor(TFT_BLACK, TFT_WHITE);
    canvas->setTextSize(2);
    canvas->setTextDatum(middle_center);
    canvas->drawString("EPD TEST", w / 2, h / 2);

    ESP_LOGI(TAG, "test pattern refresh begin busy_pin=%d", (int)gpio_get_level(EPD_BUSY_PIN));
    const int64_t t0 = esp_timer_get_time();
    canvas->pushSprite(0, 0);
    const int64_t dt_ms = (esp_timer_get_time() - t0) / 1000;
    unlock_bus();
    ESP_LOGI(TAG, "test pattern refreshed in %lldms busy_pin=%d", (long long)dt_ms,
             (int)gpio_get_level(EPD_BUSY_PIN));
    return dt_ms;
}

bool DeviceHal::sd_inserted()
{
    return pm1.digitalRead(M5PM1_GPIO_NUM_1) == LOW;
}
