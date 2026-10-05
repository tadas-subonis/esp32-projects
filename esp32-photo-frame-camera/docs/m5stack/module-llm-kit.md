# Module LLM Kit

**SKU:** K144  
**Source:** https://docs.m5stack.com/en/module/Module%20LLM%20Kit  
**Retrieved:** 2026-06-22

---

## Description

Module LLM Kit integrates **Module LLM** and **Module13.2 LLM Mate** for offline AI inference and data communication.

**Module LLM** runs StackFlow on the **AiXin AX630C** SoC — a separate Linux computer (not ESP32). Delivers 3.2 TOPS @ INT8, ~1.5 W full load. Pre-installed with Qwen2.5-0.5B; supports KWS, ASR, LLM, TTS, VLM via `apt` packages.

**Module13.2 LLM Mate** adds CH340N USB-serial, Type-C log output, RJ45 100 Mbps Ethernet, FPC-8P to Module LLM, and HT3.96×9P DIY pads.

Compatible hosts: **CoreS3**, Core2, CoreMP135. Communicates with host over **serial 115200 8N1** (default).

## Specifications

| Specification | Parameter |
|---------------|-----------|
| SoC | AX630C — Dual Cortex-A53 @ 1.2 GHz |
| NPU | 3.2 TOPS @ INT8; max 12.8 TOPS @ INT4 |
| RAM | 4 GB LPDDR4 (1 GB system + 3 GB HW accel) |
| Storage | 32 GB eMMC 5.1 |
| Communication | Serial 115200@8N1 (configurable) |
| Microphone | MSM421A |
| Speaker | 8 Ω @ 1 W, 2014 cavity |
| Built-in | KWS, ASR, LLM, TTS |
| RGB LED | 3× @ LP5562 |
| Power | Idle 5 V @ 0.5 W; full load 5 V @ 1.5 W |
| OS | Ubuntu (StackFlow) |
| Temp range | 0–40 °C |
| Module LLM size | 54.0 × 54.0 × 13.0 mm |
| Mate size | 54 × 54 × 19.7 mm |

## Status LEDs

| Color | Meaning |
|-------|---------|
| Red | Device initializing |
| Green | Initialization complete |
| Blue blink | Application package updating |
| Red (upgrade) | Package upgrade failed |
| Green (upgrade) | Package upgrade successful |

## Connection (CoreS3)

1. Stack Module LLM on CoreS3 M5-Bus
2. UART via CoreS3 **Port C** (G17 TX, G18 RX)
3. Mate board: move speaker aside to expose FPC socket; use included FPC-8P cable

## M5-Bus pin notes

Module LLM has **pin-switching solder pads** (`NT` = Net-Tie) for hosts with pin conflicts. See official schematic before modifying.

Key bus signals: `TRM_TXD`, `TRM_RXD`, `SoC_SCL`, `SoC_SDA`.

## Model format warning

Models must be in **AXERA-specific format** processed for Module LLM. Generic Hugging Face weights cannot be dropped in directly. Install via M5Stack `apt` repository.

## Pre-installed capabilities

- **KWS** — keyword spotting
- **ASR** — speech recognition
- **LLM** — text generation (Qwen2.5-0.5B default)
- **TTS** — text-to-speech
- **OpenAI API plugin** — chat, completions, STT, TTS compatible endpoints

Available models (via apt): deepseek-r1-distill-qwen-1.5b, InternVL2_5-1B-MPO, Llama-3.2-1B, Qwen2.5 variants, yolo11, whisper, melotts, etc.

## Software resources (official)

- [Arduino Quick Start](https://docs.m5stack.com/en/stackflow/module_llm/arduino)
- [ADB / UART / SSH](https://docs.m5stack.com/en/stackflow/module_llm/config)
- [Software Update (apt)](https://docs.m5stack.com/en/stackflow/module_llm/software)
- [Image / Firmware Flash](https://docs.m5stack.com/en/stackflow/module_llm/image)
- [M5ModuleLLM Arduino library](https://github.com/m5stack/M5Module-LLM)
- [StackFlow GitHub packages](https://github.com/m5stack/StackFlow)

## Related docs in this repo

- [Arduino quick start](./stackflow/module-llm-arduino-quickstart.md)
- [Terminal access](./stackflow/module-llm-adb-uart-ssh.md)
- [Software update](./stackflow/module-llm-software-update.md)
- [Firmware flash](./stackflow/module-llm-image-firmware-update.md)
- [VLM + CoreS3](./stackflow/vlm-cores3-example.md)
