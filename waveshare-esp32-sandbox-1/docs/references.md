# References

## Community

- **Matrix room**: `#esp-rs:matrix.org` - Community members (including Espressif employees) are active here
  - https://matrix.to/#/#esp-rs:matrix.org

## Official documentation

### Rust-on-ESP book (Application Development)

- Application Development index:
  - https://docs.espressif.com/projects/rust/book/application-development/index.html
- Application Startup and Bootloader:
  - https://docs.espressif.com/projects/rust/book/application-development/bootloader.html
- Configuration:
  - https://docs.espressif.com/projects/rust/book/application-development/configuration.html
- Logging:
  - https://docs.espressif.com/projects/rust/book/application-development/logging.html
- Allocating Memory:
  - https://docs.espressif.com/projects/rust/book/application-development/alloc.html
- Async Options:
  - https://docs.espressif.com/projects/rust/book/application-development/async.html
- Testing:
  - https://docs.espressif.com/projects/rust/book/application-development/testing.html
- OTA:
  - https://docs.espressif.com/projects/rust/book/application-development/ota.html

### Espressif Developer Portal

- Rust-tagged articles:
  - https://www.espressif.com/en/developer-zone/articles?tag=Rust

## Learning resources

### Books and training

- **The Rust on ESP Book** - Comprehensive guide for Rust on Espressif SoCs:
  - https://docs.espressif.com/projects/rust/book/
- **Embedded Rust (no_std) on Espressif** - Training for `no_std` development on ESP32-C3:
  - https://github.com/ferrous-systems/embedded-rust-training/tree/main/esp32c3
- **Embedded Rust (std) on Espressif** - Training for `std` development on ESP32-C3 by Ferrous Systems:
  - https://github.com/ferrous-systems/embedded-rust-training/tree/main/esp32c3-std
- **impl Rust on ESP32** - Book exploring various ESP32 projects:
  - https://github.com/esp-rs/impl-rust-on-esp32

### Blogs and articles

- **Scott Mabin - Rust on Espressif chips** - Quarterly updates:
  - https://scottmabin.com/blog/tag/rust-on-espressif/
- **ESP32 with Embedded Rust at the HAL** - Blog series learning Rust at HAL level with ESP32-C3:
  - https://www.esp32.com/viewtopic.php?t=30000
- **A getting started guide to ESP32 no-std Rust development**:
  - https://dev.to/esp-rs/a-getting-started-guide-to-esp32-no-std-rust-development-4b8f
- **Making a Dino Light with the ESP32 and WS2812**:
  - Part 1: https://blog.rahix.de/001-esp32-dino-light/
  - Part 2: https://blog.rahix.de/002-esp32-dino-light-2/
- **Programming ESP32 with Rust: OTA firmware update**:
  - https://dev.to/esp-rs/programming-esp32-with-rust-ota-firmware-update-4a5k
- **Securely sending DHT22 sensor data from an ESP32 board to PostgreSQL**:
  - https://dev.to/esp-rs/securely-sending-dht22-sensor-data-from-an-esp32-board-to-postgresql-4a5k
- **Freenove ESP32 Tutorials in Rust** - Transforms C/MicroPython lessons into Rust:
  - https://github.com/esp-rs/freenove-esp32-tutorials-rust

### Video courses and talks

- **Rust on ESP32-C3** - Video course:
  - https://www.youtube.com/playlist?list=PLX44HkctSkTewrL9frlDz8nc4qUaqqijG
- **Andrei Litvin / @embedded-rust** - YouTube channel:
  - https://www.youtube.com/@embedded-rust
- **Rust embedded at Espressif @ Copenhagen Rust Community**:
  - https://www.youtube.com/watch?v=...
- **Embedded Rust on ESP32 - Juraj Michálek - Rust Linz November 2022**:
  - https://www.youtube.com/watch?v=...
- **Rust on Espressif chips - Scott Mabin - DevCon22**:
  - https://www.youtube.com/watch?v=...
- **Rust Bare-metal and Async - Scott Mabin, Juraj Sadel - DevCon23**:
  - https://www.youtube.com/watch?v=...
- **Rust on ESP32 - Video series live coding esp32 examples**:
  - https://www.youtube.com/playlist?list=...

## Tools

### Development toolchain

- **espup** - Tool for installing and maintaining ESP Rust toolchains:
  - https://github.com/esp-rs/espup
- **espflash** - Serial flasher utility for Espressif SoCs (based on esptool):
  - https://github.com/esp-rs/espflash
  - Configuration file docs: https://github.com/esp-rs/espflash/tree/main/espflash#configuration-file
- **cargo-espflash** - Cargo subcommand wrapper for espflash:
  - https://github.com/esp-rs/espflash/tree/main/cargo-espflash

### Remote development and simulation

- **esp-web-flash-server** - WebSocket server for flashing from VS Code Remote Containers:
  - https://github.com/esp-rs/esp-web-flash-server
- **wokwi-server** - WebSocket server for Wokwi simulations from VS Code Remote Containers:
  - https://github.com/esp-rs/wokwi-server
- **Wokwi Simulator** - Web browser simulator supporting Rust on ESP32:
  - https://wokwi.com/

### Testing and debugging

- **embedded-test** - Testing framework for embedded Rust:
  - https://github.com/probe-rs/embedded-test
- **probe-rs** - Debugging and flashing tool:
  - https://probe.rs

## Templates

- **esp-generate** - Template generation tool for `no_std` applications:
  - https://github.com/esp-rs/esp-generate
- **esp-idf-template** - Cargo-generate template for `std` projects (via ESP-IDF):
  - https://github.com/esp-rs/esp-idf-template

## esp-rs ecosystem

- **awesome-esp-rust** - Curated list of ESP32 Rust resources:
  - https://github.com/esp-rs/awesome-esp-rust
- **esp-hal examples**:
  - https://github.com/esp-rs/esp-hal/tree/main/examples
- **esp-hal OTA example**:
  - https://github.com/esp-rs/esp-hal/tree/main/examples/ota

## Example projects (no_std)

Useful reference implementations for patterns and techniques:

- **esp32c3-devkit-rust - Embassy, BLE, Sensors example** - Medium complexity demo:
  - https://github.com/esp-rs/esp32c3-devkit-rust
- **esp32 wifi tank** - Wifi-controlled car:
  - https://github.com/esp-rs/esp32-wifi-tank
- **esp32-rust-nostd-temperature-logger** - MQTT temperature logger:
  - https://github.com/esp-rs/esp32-rust-nostd-temperature-logger
- **esp32c3-ota-experiment** - OTA experiment:
  - https://github.com/esp-rs/esp32c3-ota-experiment
- **esp32c3-no-std-async-mqtt-demo** - Async MQTT with BMP180 sensor:
  - https://github.com/esp-rs/esp32c3-no-std-async-mqtt-demo
- **SlimeVR-Rust/firmware** - Async & no_std firmware for SlimeVR:
  - https://github.com/SlimeVR/SlimeVR-Rust/tree/main/firmware
- **esp-examples** - Various esp-hal examples:
  - https://github.com/esp-rs/esp-examples

## Async framework

- **Rust async book**:
  - https://rust-lang.github.io/async-book/
- **Embassy book**:
  - https://embassy.dev/book/

## WSL2 USB passthrough

- **usbipd-win**:
  - https://github.com/dorssel/usbipd-win

## Open hardware

- **esp-rust-board** - Development board based on ESP32-C3, KiCad design, Adafruit Feather compatible:
  - https://github.com/esp-rs/esp-rust-board

## Community discussion

- **"How mature is esp32 rust?" thread**:
  - https://www.reddit.com/r/esp32/comments/1l6hgdb/how_mature_is_esp32_rust/
