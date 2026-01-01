# ESP32 Rust development docs (this repo)

These docs are **project-specific** and assume this repository’s stack: **ESP32‑C3**, `no_std`, `esp-hal` v1, Embassy async via `esp-rtos`, Wi‑Fi via `esp-radio`.

If you’re new to this ecosystem, start with the Rust-on-ESP “Application Development” chapter list:
https://docs.espressif.com/projects/rust/book/application-development/index.html

## Contents

- [Stack & architecture](./stack.md)
- [Environment setup](./setup.md)
- [Build / flash / monitor](./build-flash-monitor.md)
- [Logging](./logging.md)
- [Async & Embassy](./async-embassy.md)
- [Memory & heap allocation](./memory-alloc.md)
- [Wi‑Fi / BLE (high-level)](./wifi-ble.md)
- [Configuration (esp-config)](./configuration.md)
- [Testing](./testing.md)
- [OTA](./ota.md)
- [WSL2 USB passthrough (usbipd-win)](./wsl2-usb.md)
- [Troubleshooting](./troubleshooting.md)
- [Crate API references (curated)](./crates/README.md)
- [References](./references.md)

