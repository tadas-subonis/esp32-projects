# ESP32 Rust development docs (this repo)

These docs are **project-specific** and assume this repository’s stack: **ESP32‑C3**, `no_std`, `esp-hal` v1, Embassy async via `esp-rtos`, Wi‑Fi via `esp-radio`.

If you’re new to this ecosystem, start with the Rust-on-ESP “Application Development” chapter list:
https://docs.espressif.com/projects/rust/book/application-development/index.html

## Contents

- [Getting started](./getting-started.md) - Hello world, LED blink, GPIO basics
- [Stack & architecture](./stack.md)
- [Device / board references](./devices/README.md)
- [Environment setup](./setup.md)
- [Build / flash / monitor](./build-flash-monitor.md)
- [Logging](./logging.md)
- [Async & Embassy](./async-embassy.md)
- [Memory & heap allocation](./memory-alloc.md)
- [Wi‑Fi / BLE (high-level)](./wifi-ble.md)
- [Display and graphics](./display-graphics.md)
- [Button handling](./button-handling.md)
- [PWM and LEDC](./pwm-ledc.md) - LED fading, servo control, tone generation
- [Sensor reading](./sensor-reading.md) - I2C sensors, DHT22, ultrasonic, touch
- [Configuration (esp-config)](./configuration.md)
- [Testing](./testing.md)
- [OTA](./ota.md)
- [WSL2 USB passthrough (usbipd-win)](./wsl2-usb.md)
- [Troubleshooting](./troubleshooting.md)
- [Practical tips and patterns](./practical-tips.md)
- [Learning resources](./learning-resources.md)
- [Crate API references (curated)](./crates/README.md)
- [References](./references.md)

