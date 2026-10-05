# Stack & architecture (this repo)

## What you’re building

This repository targets **ESP32‑C3** (RISC‑V) using **bare-metal Rust**:

- `#![no_std]` (no Rust standard library)
- async concurrency via **Embassy**
- hardware access via **esp-hal**
- Wi‑Fi/BLE controller via **esp-radio**
- logging via Rust `log` + `esp-println`
- heap allocator via `esp-alloc` (using reclaimed RAM)

See Rust-on-ESP “Application Development” overview:
https://docs.espressif.com/projects/rust/book/application-development/index.html

## Key repo files

- `Cargo.toml`: dependencies and chip/feature selection (`esp32c3`, `log-04`, `unstable`)
- `rust-toolchain.toml`: pins toolchain components/targets (`riscv32imc-unknown-none-elf`)
- `build.rs`: adds linker args and prints nicer link errors (don’t remove)
- `src/bin/main.rs`: main entry point (`#[esp_rtos::main]`), logger init, allocator init, Embassy start, Wi‑Fi init

## Runtime model

### Boot

Espressif devices typically use a ROM bootloader (1st stage) + optional 2nd-stage bootloader that sets up memory, partitions, and enables OTA flows.

Rust-on-ESP “Application Startup and Bootloader”:
https://docs.espressif.com/projects/rust/book/application-development/bootloader.html

### Async tasks

Embassy uses a fixed set of statically allocated tasks; you spawn tasks using a `Spawner`.

Rust-on-ESP “Async Options”:
https://docs.espressif.com/projects/rust/book/application-development/async.html

