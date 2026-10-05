# Module LLM — Vision-Language Model (CoreS3 + Camera)

**Source:** https://docs.m5stack.com/en/stackflow/applications/vlm/chat  
**Retrieved:** 2026-06-22

---

Demonstrates **VLM** (vision-language model) with CoreS3 camera and Module LLM over UART.

## Preparation

1. Complete [Arduino quick start](./module-llm-arduino-quickstart.md); install `M5ModuleLLM`
2. Install VLM packages on Module LLM ([software update](./module-llm-software-update.md)):

```bash
apt install llm-vlm
apt install llm-model-internvl2.5-1b-364-ax630c
```

3. Hardware: **Module LLM Kit** stacked on **CoreS3**

## Full example (official)

```cpp
#include <Arduino.h>
#include <M5Unified.h>
#include <M5ModuleLLM.h>
#include "M5CoreS3.h"

M5ModuleLLM module_llm;
String vlm_work_id;

void setup()
{
    M5.begin();
    M5.Display.setTextSize(2);
    M5.Display.setTextScroll(true);

    CoreS3.Camera.begin();
    CoreS3.Camera.sensor->set_framesize(CoreS3.Camera.sensor, FRAMESIZE_QVGA);

    int rxd = M5.getPin(m5::pin_name_t::port_c_rxd);
    int txd = M5.getPin(m5::pin_name_t::port_c_txd);
    Serial2.begin(115200, SERIAL_8N1, rxd, txd);
    module_llm.begin(&Serial2);

    M5.Display.printf(">> Check ModuleLLM connection..\n");
    while (1) {
        if (module_llm.checkConnection()) break;
    }

    M5.Display.printf(">> Reset ModuleLLM..\n");
    module_llm.sys.reset();

    M5.Display.printf(">> Setup vlm..\n");
    vlm_work_id = module_llm.vlm.setup();
}

void loop()
{
    String question = "Describe the content of the image";

    M5.update();
    auto t = M5.Touch.getDetail();
    static int vlm_inference;

    if (t.wasClicked()) {
        static unsigned long lastClickTime = 0;
        unsigned long currentMillis = millis();
        if (currentMillis - lastClickTime < 800) {
            vlm_inference = 2;  // double-tap
        }
        lastClickTime = currentMillis;
    }

    if (t.wasFlicked()) {
        vlm_inference--;
    }

    if (CoreS3.Camera.get()) {
        if (vlm_inference == 2) {
            uint8_t* out_jpg   = NULL;
            size_t out_jpg_len = 0;
            frame2jpg(CoreS3.Camera.fb, 50, &out_jpg, &out_jpg_len);
            module_llm.vlm.inference(vlm_work_id, out_jpg, out_jpg_len);
            free(out_jpg);
            delay(10);
            M5.Lcd.setCursor(0, 0);
            module_llm.vlm.inferenceAndWaitResult(vlm_work_id, question.c_str(), [](String& result) {
                M5.Display.printf("%s", result.c_str());
            });
            vlm_inference--;
        } else if (vlm_inference == 1) {
            delay(10);
        } else {
            CoreS3.Display.pushImage(0, 0, CoreS3.Display.width(), CoreS3.Display.height(),
                                     (uint16_t*)CoreS3.Camera.fb->buf);
        }
        CoreS3.Camera.free();
    }
}
```

## Usage

1. Flash to **CoreS3** (not PaperColor)
2. Wait for init — live camera preview on LCD
3. **Double-tap** screen → capture + VLM inference → description on screen
4. **Swipe** to clear text

## Extending for PaperColor

After `inferenceAndWaitResult`, add:
- Wi-Fi HTTP POST of `out_jpg` + caption string to M5Paper Color
- PaperColor dithers JPEG to Spectra-6 and refreshes e-ink

See [architecture](../architecture-photo-frame-app.md).

## Performance notes

- JPEG quality 50 is official default; community reports quality **20–30** reduces UART latency
- VLM inference takes several seconds after image send
- Camera native QVGA; VLM may pad to square input — expect coordinate offsets for overlay tasks

## Related

- [CoreS3 camera](../guides/cores3-camera.md)
- [Module LLM hardware](../module-llm-kit.md)
