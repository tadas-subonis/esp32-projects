# `esp-bootloader-esp-idf` (0.4.0)

## What it does in this repo

Bootloader integration glue for Espressif’s **ESP-IDF 2nd-stage bootloader** flows.

In `src/bin/main.rs` we declare the app descriptor required by the ESP-IDF bootloader:

- `esp_bootloader_esp_idf::esp_app_desc!();`

## Key links

- API docs (docs.rs): https://docs.rs/esp-bootloader-esp-idf/0.4.0/esp_bootloader_esp_idf/
- Rust-on-ESP bootloader chapter: https://docs.espressif.com/projects/rust/book/application-development/bootloader.html
- Rust-on-ESP OTA chapter: https://docs.espressif.com/projects/rust/book/application-development/ota.html
- Upstream repo (in esp-hal workspace): https://github.com/esp-rs/esp-hal/blob/main/esp-bootloader-esp-idf

## APIs you’ll likely use first

- `esp_bootloader_esp_idf::esp_app_desc!()` (bootloader-required app descriptor)

## How to use it

### Declare the app descriptor (required by the ESP-IDF bootloader)

This repo does it near the top of `main.rs`:

```rust
esp_bootloader_esp_idf::esp_app_desc!();
```

This macro emits metadata expected by the ESP-IDF second-stage bootloader. See Rust-on-ESP for bootloader details:
https://docs.espressif.com/projects/rust/book/application-development/bootloader.html

## API inventory

Full API index:
- https://docs.rs/esp-bootloader-esp-idf/0.4.0/esp_bootloader_esp_idf/all.html

- **Macros**
  - **`esp_app_desc!()`**: emits the ESP-IDF app descriptor metadata required by the bootloader.

