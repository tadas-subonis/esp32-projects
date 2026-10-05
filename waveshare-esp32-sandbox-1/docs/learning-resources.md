# Learning resources

This document provides a curated guide to learning ESP32 Rust development, organized by learning path and experience level. For a complete reference list, see [references.md](./references.md).

## Getting started

### Official documentation

1. **The Rust on ESP Book** - Start here for comprehensive coverage
   - https://docs.espressif.com/projects/rust/book/
   - Covers bootloader, configuration, logging, memory allocation, async, testing, and OTA

2. **A getting started guide to ESP32 no-std Rust development**
   - https://dev.to/esp-rs/a-getting-started-guide-to-esp32-no-std-rust-development-4b8f
   - Practical walkthrough for no_std development

### Training courses

- **Embedded Rust (no_std) on Espressif** - Structured training for ESP32-C3
  - https://github.com/ferrous-systems/embedded-rust-training/tree/main/esp32c3
  - Best for: Learning no_std embedded Rust patterns

- **Embedded Rust (std) on Espressif** - std-based training by Ferrous Systems
  - https://github.com/ferrous-systems/embedded-rust-training/tree/main/esp32c3-std
  - Best for: If you prefer std-based development

## Video courses and talks

### Beginner-friendly

- **Rust on ESP32-C3** - Video course playlist
  - https://www.youtube.com/playlist?list=PLX44HkctSkTewrL9frlDz8nc4qUaqqijG
  - Step-by-step video tutorials

- **Rust on ESP32 - Video series live coding esp32 examples**
  - https://www.youtube.com/playlist?list=...
  - Live coding examples

### Advanced topics

- **Rust embedded at Espressif @ Copenhagen Rust Community**
  - https://www.youtube.com/watch?v=...
  - Community talk covering ecosystem overview

- **Rust on Espressif chips - Scott Mabin - DevCon22**
  - https://www.youtube.com/watch?v=...
  - Conference talk on Rust for ESP32

- **Rust Bare-metal and Async - Scott Mabin, Juraj Sadel - DevCon23**
  - https://www.youtube.com/watch?v=...
  - Advanced async patterns and bare-metal development

## Blogs and articles

### Project walkthroughs

- **Making a Dino Light with the ESP32 and WS2812**
  - Part 1: https://blog.rahix.de/001-esp32-dino-light/
  - Part 2: https://blog.rahix.de/002-esp32-dino-light-2/
  - Complete project walkthrough with WS2812 LEDs

- **ESP32 with Embedded Rust at the HAL**
  - https://www.esp32.com/viewtopic.php?t=30000
  - Blog series learning Rust at the HAL level with ESP32-C3

### Specific topics

- **Programming ESP32 with Rust: OTA firmware update**
  - https://dev.to/esp-rs/programming-esp32-with-rust-ota-firmware-update-4a5k
  - OTA implementation guide

- **Securely sending DHT22 sensor data from an ESP32 board to PostgreSQL**
  - https://dev.to/esp-rs/securely-sending-dht22-sensor-data-from-an-esp32-board-to-postgresql-4a5k
  - IoT data collection and security patterns

### Regular updates

- **Scott Mabin - Rust on Espressif chips** - Quarterly updates
  - https://scottmabin.com/blog/tag/rust-on-espressif/
  - Stay current with ecosystem developments

## Example projects (no_std)

These projects demonstrate real-world patterns and can serve as reference implementations:

### Beginner examples

- **esp-examples** - Various esp-hal examples
  - https://github.com/esp-rs/esp-examples
  - Official examples covering common use cases

- **Comprehensive ESP32 Rust Examples** - Step-by-step examples with code
  - https://github.com/Vaishnav-Sabari-Girish/Embedded-Rust/tree/main/microcontrollers/esp32
  - Includes: Hello world, LED blink, button press, PWM, TFT display, sensor reading
  - Each example includes complete working code and explanations
  - Great for: Learning basic patterns and seeing complete implementations

- **Beginner Rust ESP32 development - Snake**
  - Snake game on ESP32 with OLED display and joystick
  - Good for: Learning display and input handling

### Intermediate examples

- **esp32c3-devkit-rust - Embassy, BLE, Sensors example**
  - https://github.com/esp-rs/esp32c3-devkit-rust
  - Medium complexity: RGB LED, I2C IMU, Temperature & Humidity, BLE
  - Good for: Learning async patterns, BLE, sensor integration

- **esp32c3-no-std-async-mqtt-demo**
  - https://github.com/esp-rs/esp32c3-no-std-async-mqtt-demo
  - Async MQTT with BMP180 sensor
  - Good for: Learning async networking and MQTT patterns

- **esp32-rust-nostd-temperature-logger**
  - https://github.com/esp-rs/esp32-rust-nostd-temperature-logger
  - MQTT temperature logger
  - Good for: IoT data logging patterns

### Advanced examples

- **esp32c3-ota-experiment**
  - https://github.com/esp-rs/esp32c3-ota-experiment
  - OTA firmware update implementation
  - Good for: Learning OTA patterns

- **SlimeVR-Rust/firmware**
  - https://github.com/SlimeVR/SlimeVR-Rust/tree/main/firmware
  - Async & no_std firmware for SlimeVR Full Body Tracking
  - Good for: Complex async state machines

- **esp32 wifi tank**
  - https://github.com/esp-rs/esp32-wifi-tank
  - WiFi-controlled car
  - Good for: WiFi control patterns

## Books

- **impl Rust on ESP32**
  - https://github.com/esp-rs/impl-rust-on-esp32
  - Book exploring various ESP32 projects
  - Covers multiple project types and patterns

## Tutorial series

- **Freenove ESP32 Tutorials in Rust**
  - https://github.com/esp-rs/freenove-esp32-tutorials-rust
  - Transforms traditional C and MicroPython-based Freenove lessons into idiomatic Rust
  - Good for: Following structured tutorial progression

## Community

- **Matrix room**: `#esp-rs:matrix.org`
  - https://matrix.to/#/#esp-rs:matrix.org
  - Active community including Espressif employees
  - Best place to ask questions and get help

## Learning path recommendations

### Complete beginner to embedded Rust

1. Read "A getting started guide to ESP32 no-std Rust development"
2. Follow the Rust on ESP Book chapters sequentially
3. Try the esp-examples repository
4. Build a simple project (e.g., LED blink, sensor reading)
5. Join the Matrix room for questions

### Experienced Rust developer new to embedded

1. Skim the Rust on ESP Book (focus on embedded-specific chapters)
2. Review esp-examples for patterns
3. Study an intermediate example project (e.g., esp32c3-devkit-rust)
4. Build a project combining multiple peripherals
5. Explore async patterns with Embassy

### Experienced embedded developer new to Rust

1. Complete "Embedded Rust (no_std) on Espressif" training
2. Study Rust async patterns (Rust async book + Embassy book)
3. Review example projects focusing on patterns you need
4. Build incrementally complex projects
5. Reference generated docs (`cargo doc`) for exact APIs

## Tools for learning

- **Wokwi Simulator** - Test code without hardware
  - https://wokwi.com/
  - Useful for: Rapid prototyping, testing logic, learning without hardware

- **esp-generate** - Bootstrap new projects
  - https://github.com/esp-rs/esp-generate
  - Useful for: Starting new projects, exploring project structure

- **Generated documentation** - Always check `cargo doc` output
  - Run: `cargo doc --target riscv32imc-unknown-none-elf --open`
  - Useful for: Understanding exact API signatures and trait requirements
