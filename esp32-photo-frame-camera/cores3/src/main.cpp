/*
 * CoreS3 photo-frame sender — tap screen or PWR to capture and POST to PaperColor.
 */
#include <Arduino.h>
#include <M5CoreS3.h>
#include <esp_camera.h>
#include "img_converters.h"

#include "photo_frame_protocol.h"
#include "photo_sender.h"
#include "pf_log.h"
#include "secrets.h"
#include "serial_console.h"

static const uint32_t CAPTURE_DEBOUNCE_MS = 2000;
static const uint8_t SHUTTER_VOLUME = 24;  // M5 default master volume is 64
static const char* TAG = "cores3";

enum class UiState : uint8_t {
    Preview,
    Flash,
    Sending,
    WaitFrame,
    Ok,
};

static UiState s_ui        = UiState::Preview;
static uint32_t s_last_capture_ms = 0;
static uint32_t s_flash_until_ms  = 0;

static void play_shutter_tone()
{
    CoreS3.Speaker.setVolume(SHUTTER_VOLUME);
    CoreS3.Speaker.tone(880, 60);
    delay(65);
    CoreS3.Speaker.tone(660, 45);
}

static void draw_status(const char* line1, const char* line2, uint16_t color)
{
    CoreS3.Display.fillRect(0, CoreS3.Display.height() - 48, CoreS3.Display.width(), 48, TFT_BLACK);
    CoreS3.Display.setTextSize(1);
    CoreS3.Display.setTextDatum(top_center);
    CoreS3.Display.setTextColor(color);
    CoreS3.Display.drawString(line1, CoreS3.Display.width() / 2, CoreS3.Display.height() - 44);
    if (line2 && line2[0]) {
        CoreS3.Display.drawString(line2, CoreS3.Display.width() / 2, CoreS3.Display.height() - 28);
    }
}

static bool capture_requested()
{
    M5.update();
    if (CoreS3.BtnPWR.wasClicked()) {
        return true;
    }
    if (CoreS3.Touch.getCount() > 0) {
        auto t = CoreS3.Touch.getDetail();
        if (t.wasClicked()) {
            return true;
        }
    }
    return false;
}

bool cores3_capture_and_send(int* http_code_out, int* w_out, int* h_out, size_t* jpeg_len_out, uint32_t* ms_out)
{
    const uint32_t start_ms = millis();

    if (!CoreS3.Camera.get()) {
        draw_status("Camera busy", "", TFT_YELLOW);
        PF_LOGW(TAG, "camera busy");
        return false;
    }

    uint8_t* jpg     = nullptr;
    size_t   jpg_len = 0;
    if (!frame2jpg(CoreS3.Camera.fb, PHOTO_FRAME_JPEG_QUALITY, &jpg, &jpg_len)) {
        CoreS3.Camera.free();
        draw_status("Capture failed", "", TFT_YELLOW);
        PF_LOGW(TAG, "frame2jpg failed");
        return false;
    }

    const int w = CoreS3.Camera.fb->width;
    const int h = CoreS3.Camera.fb->height;
    CoreS3.Camera.free();

    play_shutter_tone();
    s_flash_until_ms = millis() + 120;
    s_ui             = UiState::Flash;

    draw_status("Sending...", "", TFT_WHITE);
    s_ui = UiState::Sending;

    const int http_code = photo_frame_send(jpg, jpg_len, w, h);
    free(jpg);

    if (http_code == 202) {
        draw_status("Sent!", "Frame updating", TFT_GREEN);
        s_ui = UiState::Ok;
    } else if (http_code == 503) {
        draw_status("Frame busy", "Try again", TFT_YELLOW);
        s_ui = UiState::WaitFrame;
    } else {
        draw_status("Send failed", "Check frame WiFi", TFT_YELLOW);
        s_ui = UiState::WaitFrame;
    }

    s_last_capture_ms = millis();

    if (http_code_out) {
        *http_code_out = http_code;
    }
    if (w_out) {
        *w_out = w;
    }
    if (h_out) {
        *h_out = h;
    }
    if (jpeg_len_out) {
        *jpeg_len_out = jpg_len;
    }
    if (ms_out) {
        *ms_out = millis() - start_ms;
    }

    PF_LOGI(TAG, "capture sent http=%d jpeg=%lu %dx%d ms=%lu", http_code, (unsigned long)jpg_len, w, h, (unsigned long)(millis() - start_ms));
    return http_code == 202;
}

void setup()
{
    disableLoopWDT();
    disableCore0WDT();

    Serial.begin(115200);
    delay(500);
    PF_LOGI(TAG, "early boot");

    auto cfg = M5.config();
    CoreS3.begin(cfg);

    PF_LOGI(TAG, "boot");

    CoreS3.Display.setTextSize(2);
    CoreS3.Display.setTextDatum(middle_center);
    CoreS3.Display.fillScreen(TFT_BLACK);
    CoreS3.Display.setTextColor(TFT_WHITE);
    CoreS3.Display.drawString("Photo Frame", CoreS3.Display.width() / 2, 40);
    CoreS3.Display.setTextSize(1);
    CoreS3.Display.drawString("Tap or PWR to capture", CoreS3.Display.width() / 2, 70);

    M5.In_I2C.release();
    if (!CoreS3.Camera.begin()) {
        CoreS3.Display.drawString("Camera init failed", CoreS3.Display.width() / 2, 100);
        PF_LOGE(TAG, "camera init failed");
        return;
    }
    CoreS3.Camera.sensor->set_framesize(CoreS3.Camera.sensor, FRAMESIZE_QVGA);
    PF_LOGI(TAG, "camera ready QVGA");

    draw_status("WiFi...", "", TFT_WHITE);
    if (!wifi_photo_frame_connect()) {
        draw_status("WiFi failed", PHOTO_FRAME_AP_SSID, TFT_YELLOW);
        PF_LOGW(TAG, "wifi connect failed");
    } else {
        draw_status("Ready", PAPERCOLOR_HOST, TFT_GREEN);
        PF_LOGI(TAG, "ready paper=%s", PAPERCOLOR_HOST);
    }
}

void loop()
{
    const uint32_t now = millis();

    serial_console_poll();

    if (s_ui == UiState::Flash && now < s_flash_until_ms) {
        CoreS3.Display.fillScreen(TFT_WHITE);
        delay(10);
        return;
    }

    if (s_ui == UiState::Flash) {
        s_ui = UiState::Preview;
    }

    if (capture_requested() && (now - s_last_capture_ms >= CAPTURE_DEBOUNCE_MS)) {
        (void)cores3_capture_and_send(nullptr, nullptr, nullptr, nullptr, nullptr);
    }

    if (!CoreS3.Camera.get()) {
        delay(10);
        return;
    }

    if (s_ui == UiState::Preview || s_ui == UiState::Ok || s_ui == UiState::WaitFrame) {
        CoreS3.Display.pushImage(0, 0, CoreS3.Display.width(), CoreS3.Display.height(),
                                 (uint16_t*)CoreS3.Camera.fb->buf);
        if (s_ui == UiState::Ok && now - s_last_capture_ms > 3000) {
            s_ui = UiState::Preview;
            draw_status("Ready", PAPERCOLOR_HOST, TFT_GREEN);
        }
    }

    CoreS3.Camera.free();
    delay(10);
}
