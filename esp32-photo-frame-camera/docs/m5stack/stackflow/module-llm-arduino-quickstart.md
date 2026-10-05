# Module LLM — Arduino Quick Start

**Source:** https://docs.m5stack.com/en/stackflow/module_llm/arduino  
**Retrieved:** 2026-06-22

---

## Overview

Module LLM works with M5 **Core** series controllers (including **CoreS3**) via the `M5ModuleLLM` Arduino library over UART.

## Environment setup

1. Install **Arduino IDE**
2. Install **M5Stack board manager**; select your host board (e.g. `M5CoreS3`)
3. Install **`M5ModuleLLM`** library (+ dependency `M5Unified`)

## First test: KWS + ASR

Open example **`kws_asr`** from the M5ModuleLLM library → Upload.

- Wake word: **"HELLO"**
- Wait for module init (green status LED on LLM module)
- Speak after wake word

## Library examples

| Example | Pipeline |
|---------|----------|
| `kws_asr` | KWS → ASR (speech to text) |
| `text_assistant` | LLM text in → text out |
| `tts` | Text → speech |
| `voice_assistant` | KWS → ASR → LLM → TTS |

## CoreS3 serial wiring (code)

```cpp
#include <M5ModuleLLM.h>
M5ModuleLLM module_llm;

int rxd = M5.getPin(m5::pin_name_t::port_c_rxd);  // G18
int txd = M5.getPin(m5::pin_name_t::port_c_txd);  // G17
Serial2.begin(115200, SERIAL_8N1, rxd, txd);
module_llm.begin(&Serial2);

while (!module_llm.checkConnection()) { delay(100); }
module_llm.sys.reset();
```

## Related links

- [M5Module-LLM GitHub](https://github.com/m5stack/M5Module-LLM)
- [Library API docs](https://docs.m5stack.com/en/arduino/m5modulellm/m5modulellm)
- [Software update (install models)](./module-llm-software-update.md)
- [VLM + camera](./vlm-cores3-example.md)
