# CoreS3 — GC0308 Camera

**Source:** https://docs.m5stack.com/en/arduino/m5cores3/camera  
**Retrieved:** 2026-06-22

---

## Requirements

- M5Stack Board Manager **>= 3.2.2**
- Board: **M5CoreS3**
- M5Unified **>= 0.2.11**

## Pin configuration (esp_camera)

```cpp
static camera_config_t camera_config = {
    .pin_pwdn     = -1,
    .pin_reset    = -1,
    .pin_xclk     = -1,
    .pin_sscb_sda = 12,
    .pin_sscb_scl = 11,
    .pin_d7       = 47,
    .pin_d6       = 48,
    .pin_d5       = 16,
    .pin_d4       = 15,
    .pin_d3       = 42,
    .pin_d2       = 41,
    .pin_d1       = 40,
    .pin_d0       = 39,
    .pin_vsync    = 46,
    .pin_href     = 38,
    .pin_pclk     = 45,
    .xclk_freq_hz = 20000000,
    .ledc_timer   = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_RGB565,
    .frame_size   = FRAMESIZE_QVGA,
    .jpeg_quality = 0,
    .fb_count     = 2,
    .fb_location  = CAMERA_FB_IN_PSRAM,
    .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
    .sccb_i2c_port = -1,
};
```

## Using M5CoreS3 wrapper (recommended)

```cpp
#include "M5CoreS3.h"

void setup() {
    auto cfg = M5.config();
    CoreS3.begin(cfg);

    if (!CoreS3.Camera.begin()) {
        CoreS3.Display.drawString("Camera Init Fail", ...);
    }
    CoreS3.Camera.sensor->set_framesize(CoreS3.Camera.sensor, FRAMESIZE_QVGA);
}

void loop() {
    if (CoreS3.Camera.get()) {
        CoreS3.Display.pushImage(0, 0, CoreS3.Display.width(), CoreS3.Display.height(),
                                 (uint16_t*)CoreS3.Camera.fb->buf);
        CoreS3.Camera.free();
    }
}
```

## JPEG export (for LLM / Wi-Fi transfer)

```cpp
uint8_t* out_jpg = NULL;
size_t out_jpg_len = 0;
frame2jpg(CoreS3.Camera.fb, 50, &out_jpg, &out_jpg_len);
// ... use out_jpg ...
free(out_jpg);
CoreS3.Camera.free();
```

**Quality guidance:** 20–50 for LLM VLM (smaller = faster UART transfer). Quality 255 for local preview via `drawJpg()`.

## Frame sizes

| Constant | Resolution | Use case |
|----------|------------|----------|
| FRAMESIZE_QVGA | 320×240 | Default; matches display; LLM input |
| FRAMESIZE_VGA | 640×480 | Higher res; more PSRAM/bandwidth |

## Official example repo

https://github.com/m5stack/M5CoreS3/blob/main/examples/Basic/camera/camera.ino

## Related

- [VLM + LLM example](../stackflow/vlm-cores3-example.md)
- [CoreS3 hardware](../cores3.md)
