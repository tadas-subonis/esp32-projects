# Arduino Example Patterns for Waveshare ESP32-S3 Touch AMOLED 1.8"

**Source:** `ESP32-S3-Touch-AMOLED-1.8-Demo/Arduino-v3.3.5/examples/`

This document extracts useful patterns from the official Waveshare Arduino examples for reference when implementing Rust drivers.

---

## Pin Configuration (from `pin_config.h`)

```c
#pragma once

#define XPOWERS_CHIP_AXP2101

// Display QSPI interface
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK  11
#define LCD_CS    12
#define LCD_WIDTH 368
#define LCD_HEIGHT 448

// I2C bus (shared)
#define IIC_SDA 15
#define IIC_SCL 14

// Touch interrupt (direct GPIO, NOT via expander)
#define TP_INT 21

// I2S Audio (ES8311)
#define I2S_MCK_IO 16
#define I2S_BCK_IO 9
#define I2S_DI_IO  10   // Mic data IN (to ESP)
#define I2S_WS_IO  45
#define I2S_DO_IO  8    // Speaker data OUT (from ESP)
#define PA         46   // Power amplifier enable

// SD Card (SDMMC mode)
const int SDMMC_CLK  = 2;
const int SDMMC_CMD  = 1;
const int SDMMC_DATA = 3;
```

---

## I2C Device Addresses

| Device | Address | Constant Name |
|--------|---------|---------------|
| TCA9554 GPIO Expander | `0x20` | - |
| FT3168 Touch | `0x38` | `FT3168_DEVICE_ADDRESS` |
| QMI8658C IMU | `0x6A` | `QMI8658_L_SLAVE_ADDRESS` |
| PCF85063A RTC | `0x51` | - |
| AXP2101 PMU | `0x34` | `AXP2101_SLAVE_ADDRESS` |
| ES8311 Audio Codec | `0x18` | `ES8311_ADDRRES_0` |

---

## TCA9554 GPIO Expander Patterns

### EXIO Pin Mapping

| Pin | Signal | Direction | Usage |
|-----|--------|-----------|-------|
| EXIO0 | LCD_RESET | Output | Display reset (active low) |
| EXIO1 | DSI_PWR_EN | Output | Display power enable |
| EXIO2 | TP_RESET | Output | Touch panel reset |
| EXIO3 | QMI_INT2 | Input | IMU interrupt 2 |
| EXIO4 | Backlight button | Input | User input for backlight |
| EXIO5 | PMU_IRQ | Input | AXP2101 interrupt |
| EXIO6 | QMI_RST | Output | IMU reset (optional) |
| EXIO7 | SD_PWR | Output | SD card power |

### Initialization Pattern

```c
Wire.begin(IIC_SDA, IIC_SCL);
if (!expander.begin(0x20)) {
    Serial.println("Failed to find XCA9554 chip");
    while (1);
}

// Configure output pins
expander.pinMode(0, OUTPUT);  // LCD_RESET
expander.pinMode(1, OUTPUT);  // DSI_PWR_EN
expander.pinMode(2, OUTPUT);  // TP_RESET

// Reset sequence
expander.digitalWrite(0, LOW);
expander.digitalWrite(1, LOW);
expander.digitalWrite(2, LOW);
delay(20);
expander.digitalWrite(0, HIGH);
expander.digitalWrite(1, HIGH);
expander.digitalWrite(2, HIGH);
```

### Extended Configuration (some examples)

```c
// For SD card
expander.pinMode(7, OUTPUT);
expander.digitalWrite(7, HIGH);  // Enable SD power
delay(3000);

// For reading backlight button
expander.pinMode(4, INPUT);
int backlight_ctrl = expander.digitalRead(4);

// For PMU IRQ
expander.pinMode(5, INPUT);
int pmu_irq = expander.digitalRead(5);
```

---

## Display (SH8601) Patterns

### Bus Setup

```c
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS /* CS */,
    LCD_SCLK /* SCK */,
    LCD_SDIO0 /* SDIO0 */,
    LCD_SDIO1 /* SDIO1 */,
    LCD_SDIO2 /* SDIO2 */,
    LCD_SDIO3 /* SDIO3 */
);

Arduino_SH8601 *gfx = new Arduino_SH8601(
    bus,
    GFX_NOT_DEFINED /* RST - handled via expander */,
    0 /* rotation */,
    LCD_WIDTH /* width */,
    LCD_HEIGHT /* height */
);
```

### Initialization

```c
gfx->begin();
gfx->fillScreen(RGB565_WHITE);
gfx->setBrightness(255);  // 0-255
```

### Brightness Fade Effect

```c
for (int i = 0; i <= 255; i++) {
    gfx->setBrightness(i);
    delay(3);
}
```

### Drawing Operations

```c
// Basic drawing
gfx->fillScreen(RGB565_BLACK);
gfx->setCursor(10, 10);
gfx->setTextColor(RGB565_RED);
gfx->setTextSize(4);
gfx->println("Hello World!");

// Draw bitmap
gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)image_data, LCD_WIDTH, LCD_HEIGHT);

// Draw shapes
gfx->fillCircle(x, y, radius, RGB565_BLUE);
gfx->drawLine(x0, y0, x1, y1, RGB565_WHITE);
gfx->fillRect(x, y, width, height, RGB565_GREEN);
```

---

## Touch (FT3168) Patterns

### Setup with Interrupt

```c
std::shared_ptr<Arduino_IIC_DriveBus> IIC_Bus =
    std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);

void Arduino_IIC_Touch_Interrupt(void);

std::unique_ptr<Arduino_IIC> FT3168(
    new Arduino_FT3x68(
        IIC_Bus,
        FT3168_DEVICE_ADDRESS,
        DRIVEBUS_DEFAULT_VALUE,
        TP_INT,
        Arduino_IIC_Touch_Interrupt
    )
);

void Arduino_IIC_Touch_Interrupt(void) {
    FT3168->IIC_Interrupt_Flag = true;
}
```

### Initialization

```c
while (FT3168->begin() == false) {
    Serial.println("FT3168 initialization fail");
    delay(2000);
}
Serial.println("FT3168 initialization successfully");

FT3168->IIC_Write_Device_State(
    FT3168->Arduino_IIC_Touch::Device::TOUCH_POWER_MODE,
    FT3168->Arduino_IIC_Touch::Device_Mode::TOUCH_POWER_MONITOR
);
```

### Reading Touch Data

```c
if (FT3168->IIC_Interrupt_Flag == true) {
    FT3168->IIC_Interrupt_Flag = false;
    
    int32_t touchX = FT3168->IIC_Read_Device_Value(
        FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X
    );
    int32_t touchY = FT3168->IIC_Read_Device_Value(
        FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y
    );
    uint8_t fingers = FT3168->IIC_Read_Device_Value(
        FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_FINGER_NUMBER
    );
    
    if (fingers > 0) {
        Serial.printf("Touch X:%d Y:%d\n", touchX, touchY);
    }
}
```

---

## IMU (QMI8658C) Patterns

### Initialization

```c
SensorQMI8658 qmi;
IMUdata acc;
IMUdata gyr;

if (!qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    Serial.println("Failed to find QMI8658 - check your wiring!");
    while (1) { delay(1000); }
}

// Configure accelerometer
qmi.configAccelerometer(
    SensorQMI8658::ACC_RANGE_4G,
    SensorQMI8658::ACC_ODR_1000Hz,
    SensorQMI8658::LPF_MODE_0
);
qmi.enableAccelerometer();
```

### Reading Data

```c
if (qmi.getDataReady()) {
    if (qmi.getAccelerometer(acc.x, acc.y, acc.z)) {
        Serial.printf("{ACCEL: %f, %f, %f}\n", acc.x, acc.y, acc.z);
    }
    if (qmi.getGyroscope(gyr.x, gyr.y, gyr.z)) {
        Serial.printf("{GYRO: %f, %f, %f}\n", gyr.x, gyr.y, gyr.z);
    }
}
```

### Auto-Rotation Pattern

```c
if (qmi.getDataReady()) {
    if (qmi.getAccelerometer(acc.x, acc.y, acc.z)) {
        float angleX = acc.x;
        float angleY = acc.y;
        
        if (angleX > 0.8 && !rotation) {
            lv_disp_set_rotation(NULL, LV_DISP_ROT_NONE);
            rotation = true;
        } else if (angleX < -0.8 && !rotation) {
            lv_disp_set_rotation(NULL, LV_DISP_ROT_180);
            rotation = true;
        } else if (angleY < -0.8 && !rotation) {
            lv_disp_set_rotation(NULL, LV_DISP_ROT_90);
            rotation = true;
        } else if (angleY > 0.8 && !rotation) {
            lv_disp_set_rotation(NULL, LV_DISP_ROT_270);
            rotation = true;
        }
        
        // Reset rotation flag when device is flat
        if ((angleX <= 0.8 && angleX >= -0.8) && 
            (angleY <= 0.8 && angleY >= -0.8)) {
            rotation = false;
        }
    }
}
```

---

## RTC (PCF85063A) Patterns

### Initialization

```c
SensorPCF85063 rtc;

if (!rtc.begin(Wire, IIC_SDA, IIC_SCL)) {
    Serial.println("Failed to find PCF8563 - check your wiring!");
    while (1) { delay(1000); }
}
```

### Set Time

```c
uint16_t year = 2024;
uint8_t month = 9;
uint8_t day = 24;
uint8_t hour = 11;
uint8_t minute = 24;
uint8_t second = 30;

rtc.setDateTime(year, month, day, hour, minute, second);
```

### Read Time

```c
RTC_DateTime datetime = rtc.getDateTime();

char timeString[20];
sprintf(timeString, "%04d-%02d-%02d %02d:%02d:%02d",
    datetime.getYear(),
    datetime.getMonth(),
    datetime.getDay(),
    datetime.getHour(),
    datetime.getMinute(),
    datetime.getSecond()
);
```

---

## PMU (AXP2101) Patterns

### Initialization

```c
XPowersPMU power;

bool result = power.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
if (result == false) {
    Serial.println("PMU is not online...");
    while (1) delay(50);
}
```

### ADC Configuration

```c
void adcOn() {
    power.enableTemperatureMeasure();
    power.enableBattDetection();
    power.enableVbusVoltageMeasure();
    power.enableBattVoltageMeasure();
    power.enableSystemVoltageMeasure();
}

void adcOff() {
    power.disableTemperatureMeasure();
    power.disableBattDetection();
    power.disableVbusVoltageMeasure();
    power.disableBattVoltageMeasure();
    power.disableSystemVoltageMeasure();
}
```

### IRQ Configuration

```c
power.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
power.setChargeTargetVoltage(3);
power.clearIrqStatus();
power.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);  // Power key press
```

### Reading Status

```c
String info = "";
info += "Temperature: " + String(power.getTemperature()) + "°C\n";
info += "isCharging: " + String(power.isCharging() ? "YES" : "NO") + "\n";
info += "isDischarge: " + String(power.isDischarge() ? "YES" : "NO") + "\n";
info += "isStandby: " + String(power.isStandby() ? "YES" : "NO") + "\n";
info += "isVbusIn: " + String(power.isVbusIn() ? "YES" : "NO") + "\n";
info += "isVbusGood: " + String(power.isVbusGood() ? "YES" : "NO") + "\n";
info += "Battery Voltage: " + String(power.getBattVoltage()) + "mV\n";
info += "Vbus Voltage: " + String(power.getVbusVoltage()) + "mV\n";
info += "System Voltage: " + String(power.getSystemVoltage()) + "mV\n";

if (power.isBatteryConnect()) {
    info += "Battery Percent: " + String(power.getBatteryPercent()) + "%\n";
}

// Charge status
uint8_t charge_status = power.getChargerStatus();
switch (charge_status) {
    case XPOWERS_AXP2101_CHG_TRI_STATE:   // tri_charge
    case XPOWERS_AXP2101_CHG_PRE_STATE:   // pre_charge
    case XPOWERS_AXP2101_CHG_CC_STATE:    // constant charge
    case XPOWERS_AXP2101_CHG_CV_STATE:    // constant voltage
    case XPOWERS_AXP2101_CHG_DONE_STATE:  // charge done
    case XPOWERS_AXP2101_CHG_STOP_STATE:  // not charging
}
```

### Power Key IRQ Handling

```c
// Read PMU IRQ via expander
int pmu_irq = expander.digitalRead(5);
if (pmu_irq == 1) {
    pmu_flag = true;
}

// In loop
if (pmu_flag) {
    pmu_flag = false;
    uint32_t status = power.getIrqStatus();
    
    if (power.isPekeyShortPressIrq()) {
        // Handle short press
    }
    
    power.clearIrqStatus();
}
```

---

## Audio (ES8311) Patterns

### I2S Setup

```c
#include "ESP_I2S.h"
I2SClass i2s;

pinMode(PA, OUTPUT);
digitalWrite(PA, HIGH);  // Enable amplifier

i2s.setPins(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO, I2S_DI_IO, I2S_MCK_IO);
if (!i2s.begin(I2S_MODE_STD, EXAMPLE_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, 
               I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    Serial.println("Failed to initialize I2S bus!");
    return;
}
```

### Codec Initialization

```c
#define EXAMPLE_SAMPLE_RATE 16000
#define EXAMPLE_VOICE_VOLUME 85  // 0-100

esp_err_t es8311_codec_init(void) {
    es8311_handle_t es_handle = es8311_create(I2C_NUM, ES8311_ADDRRES_0);
    
    const es8311_clock_config_t es_clk = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = EXAMPLE_SAMPLE_RATE * 256,
        .sample_frequency = EXAMPLE_SAMPLE_RATE
    };
    
    es8311_init(es_handle, &es_clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
    es8311_sample_frequency_config(es_handle, es_clk.mclk_frequency, es_clk.sample_frequency);
    es8311_microphone_config(es_handle, false);
    es8311_voice_volume_set(es_handle, EXAMPLE_VOICE_VOLUME, NULL);
    es8311_microphone_gain_set(es_handle, EXAMPLE_MIC_GAIN);
    
    return ESP_OK;
}
```

### Audio Playback

```c
// Play audio buffer
i2s.write((uint8_t *)audio_pcm, audio_pcm_len);
```

### Echo/Loopback

```c
#define EXAMPLE_RECV_BUF_SIZE 10000
static uint8_t mic_data[EXAMPLE_RECV_BUF_SIZE];

void loop() {
    size_t bytes_read = i2s.readBytes((char *)mic_data, EXAMPLE_RECV_BUF_SIZE);
    if (!bytes_read) return;
    
    size_t bytes_write = i2s.write((const uint8_t *)mic_data, bytes_read);
    if (!bytes_write) return;
}
```

---

## SD Card Patterns

### Initialization

```c
#include <SD_MMC.h>

// Set pins for 1-bit SDMMC mode
SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);

// Enable SD power via expander
expander.pinMode(7, OUTPUT);
expander.digitalWrite(7, HIGH);
delay(3000);

// Mount
if (!SD_MMC.begin("/sdcard", true)) {  // true = 1-bit mode
    Serial.println("Card Mount Failed");
    return;
}
```

### Card Info

```c
uint8_t cardType = SD_MMC.cardType();
if (cardType == CARD_NONE) {
    Serial.println("No SD_MMC card attached");
    return;
}

uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
Serial.printf("SD Card Size: %lluMB\n", cardSize);
```

### Directory Listing

```c
String listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
    String content = "Listing: " + String(dirname) + "\n";
    
    File root = fs.open(dirname);
    if (!root || !root.isDirectory()) {
        return "Failed to open directory\n";
    }
    
    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            content += "  DIR: " + String(file.name()) + "\n";
            if (levels) {
                content += listDir(fs, file.path(), levels - 1);
            }
        } else {
            content += "  FILE: " + String(file.name()) + 
                      " SIZE: " + String(file.size()) + "\n";
        }
        file = root.openNextFile();
    }
    return content;
}
```

---

## LVGL Integration Patterns

### Configuration (lv_conf.h)

```c
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_MEM_SIZE (48U * 1024U)
#define LV_DISP_DEF_REFR_PERIOD 10
#define LV_INDEV_DEF_READ_PERIOD 10
#define LV_DPI_DEF 130
```

### Display Driver

```c
#define EXAMPLE_LVGL_TICK_PERIOD_MS 2

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[LCD_WIDTH * LCD_HEIGHT / 10];

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
#if (LV_COLOR_16_SWAP != 0)
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif
    
    lv_disp_flush_ready(disp);
}

void example_increase_lvgl_tick(void *arg) {
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}
```

### Touch Input Driver

```c
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    int32_t touchX = FT3168->IIC_Read_Device_Value(
        FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X
    );
    int32_t touchY = FT3168->IIC_Read_Device_Value(
        FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y
    );
    
    if (FT3168->IIC_Interrupt_Flag == true) {
        FT3168->IIC_Interrupt_Flag = false;
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}
```

### Setup

```c
void setup() {
    // ... display and touch init ...
    
    lv_init();
    
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, LCD_WIDTH * LCD_HEIGHT / 10);
    
    // Display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_WIDTH;
    disp_drv.ver_res = LCD_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    // Touch driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
    
    // Tick timer
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000);
}

void loop() {
    lv_timer_handler();
    delay(5);
}
```

### Double Buffer for Better Performance

```c
// Allocate DMA-capable buffers
lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(
    screenWidth * screenHeight / 4 * sizeof(lv_color_t), MALLOC_CAP_DMA
);
lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(
    screenWidth * screenHeight / 4 * sizeof(lv_color_t), MALLOC_CAP_DMA
);

lv_disp_draw_buf_init(&draw_buf, buf1, buf2, screenWidth * screenHeight / 4);

// Enable software rotation
disp_drv.sw_rotate = 1;
```

---

## WiFi + NTP Patterns

```c
#include <WiFi.h>

void wifi_init() {
    WiFi.begin(SSID, PASSWORD);
    
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(500);
        Serial.print(".");
    }
    
    Serial.println("WiFi connected");
    configTime((const long)(8 * 3600), 0, ntpServer);  // UTC+8
}

void display_time() {
    struct tm timeinfo;
    
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }
    
    int year = timeinfo.tm_year + 1900;
    int month = timeinfo.tm_mon + 1;
    int day = timeinfo.tm_mday;
    int hour = timeinfo.tm_hour;
    int min = timeinfo.tm_min;
    int sec = timeinfo.tm_sec;
}
```

---

## Brightness Control Pattern

```c
bool backlight_on = true;
int brightness = 255;

void toggleBacklight() {
    if (backlight_on) {
        for (int i = 255; i >= 0; i--) {
            gfx->setBrightness(i);
            delay(3);
        }
    } else {
        for (int i = 0; i <= 255; i++) {
            gfx->setBrightness(i);
            delay(3);
        }
    }
    backlight_on = !backlight_on;
}

// Check backlight button via expander
int backlight_ctrl = expander.digitalRead(4);
if (backlight_ctrl == HIGH) {
    while (expander.digitalRead(4) == HIGH) {
        delay(50);
    }
    toggleBacklight();
}
```

---

## Summary: Key Differences from Common Assumptions

1. **LCD CS/SCLK are often confused:**
   - CS = GPIO12
   - SCLK = GPIO11

2. **TCA9554 address is 0x20**, not 0x24

3. **Touch interrupt (TP_INT) is GPIO21 directly**, not via expander

4. **I2S WS is GPIO45**, not GPIO8

5. **QMI8658C is at 0x6A** (low address)

6. **PMU IRQ is via EXIO5**, not a direct GPIO
