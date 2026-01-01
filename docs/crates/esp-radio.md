# `esp-radio` (0.17.0)

## What it does in this repo

Provides the Wi‑Fi/BLE controller stack for Espressif chips in the esp-rs ecosystem.

In `src/bin/main.rs` we:

- call `esp_radio::init()` to initialize the radio subsystem
- create a Wi‑Fi controller using `esp_radio::wifi::new(...)`

## Key links

- API docs (docs.rs): https://docs.rs/esp-radio/0.17.0/esp_radio/
- Crate page: https://crates.io/crates/esp-radio
- Upstream repo/issues: https://github.com/esp-rs/esp-hal (esp-rs workspace)

Note: docs.rs currently fails to build the crate documentation for `esp-radio` 0.17.0, so the authoritative hosted API docs are here (ESP32‑C3):
https://docs.espressif.com/projects/rust/esp-radio/0.17.0/esp32c3/esp_radio/index.html

## APIs you’ll likely use first

- `esp_radio::init() -> Result<_, _>`
- `esp_radio::wifi::new(&radio_init, peripherals.WIFI, config)`

## Gotchas

- `esp-radio` needs an async/time/scheduler integration. In this repo that is provided by `esp-rtos` (see `esp_rtos::start(...)`).
- Be mindful of large buffers (keep them off stack; this repo denies `clippy::large_stack_frames`).

## How to use it (ESP32‑C3)

The ESP32‑C3 docs list:

- `esp_radio::init` (function): initialize Wi‑Fi and/or BLE
- `esp_radio::wifi` module: Wi‑Fi API surface
- `esp_radio::Controller` (struct): controller for the radio driver

Reference (ESP32‑C3 docs): https://docs.espressif.com/projects/rust/esp-radio/0.17.0/esp32c3/esp_radio/index.html

### Minimal bring-up (what this repo already does)

```rust
use log::info;

// After esp-hal init + allocator + esp-rtos start...
let radio_init = esp_radio::init().expect("Failed to initialize Wi-Fi/BLE controller");
let (mut wifi_controller, _interfaces) =
    esp_radio::wifi::new(&radio_init, peripherals.WIFI, Default::default())
        .expect("Failed to initialize Wi-Fi controller");

info!("Wi-Fi controller created: {:?}", core::any::type_name::<_>());
let _ = wifi_controller;
```

### Important build note (from esp-radio docs)

The ESP32‑C3 docs warn that Wi‑Fi often requires higher optimization levels; for debug builds, it recommends:

```toml
[profile.dev.package.esp-radio]
opt-level = 3
```

Reference (Optimization Level section): https://docs.espressif.com/projects/rust/esp-radio/0.17.0/esp32c3/esp_radio/index.html

### Configuration via env / .cargo/config.toml

The ESP32‑C3 docs list tunables that can be set via environment variables (or Cargo `[env]` in `.cargo/config.toml`), e.g. MTU and queue sizes.

Reference (Additional configuration table): https://docs.espressif.com/projects/rust/esp-radio/0.17.0/esp32c3/esp_radio/index.html

## API inventory (ESP32‑C3)

Full API index (All Items, ESP32‑C3):
- https://docs.espressif.com/projects/rust/esp-radio/0.17.0/esp32c3/esp_radio/all.html

- **Modules**
  - **`esp_radio::wifi`**: Wi‑Fi APIs (station/AP, config, power save, etc.).
  - **`esp_radio::esp_now`**: ESP‑NOW APIs (feature-gated).
  - **`esp_radio::ble`**: BLE HCI interface (feature-gated / unstable).

- **Core structs**
  - **`Controller`**: controller for the ESP radio driver (top-level handle).

- **Core enums**
  - **`InitializationError`**: error returned by `init`.

- **Core functions**
  - **`init()`**: initialize radio subsystem for Wi‑Fi and/or BLE.
  - **`phy_calibration_data()`**: get PHY calibration blob.
  - **`set_phy_calibration_data(...)`**: set PHY calibration blob.
  - **`wifi_set_log_verbose()`**: enable verbose Wi‑Fi driver logging (feature-gated).


