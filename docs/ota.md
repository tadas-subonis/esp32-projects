# OTA (Over The Air Updates)

Rust-on-ESP “Over The Air Updates (OTA)”:
https://docs.espressif.com/projects/rust/book/application-development/ota.html

## What OTA depends on

Rust-on-ESP emphasizes OTA is **bootloader + partition-table dependent** and involves switching/rollback of firmware images.

Reference:
https://docs.espressif.com/projects/rust/book/application-development/ota.html

## Bootloader support crate

Rust-on-ESP currently points to ESP-IDF bootloader support via:

- `esp-bootloader-esp-idf`

Reference:
https://docs.espressif.com/projects/rust/book/application-development/ota.html

## Example

Rust-on-ESP links an OTA example in the `esp-hal` repository:
https://github.com/esp-rs/esp-hal/tree/main/examples/ota

That example also includes instructions for producing an OTA binary using `espflash`.

