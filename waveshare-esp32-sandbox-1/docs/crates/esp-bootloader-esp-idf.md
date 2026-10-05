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

## Complete API inventory

Full API index: See local docs at `target/riscv32imc-unknown-none-elf/doc/esp_bootloader_esp_idf/all.html`

### Structs
- **`EspAppDesc`**: ESP-IDF app descriptor structure.
- **`ota::Ota`**: OTA (Over-The-Air) update manager.
- **`ota_updater::OtaUpdater`**: OTA updater handle.
- **`partitions::FlashRegion`**: Flash memory region.
- **`partitions::PartitionEntry`**: Partition table entry.
- **`partitions::PartitionTable`**: Partition table structure.

### Enums
- **`ota::OtaImageState`**: OTA image state (valid, invalid, etc.).
- **`partitions::AppPartitionSubType`**: Application partition subtype.
- **`partitions::BootloaderPartitionSubType`**: Bootloader partition subtype.
- **`partitions::DataPartitionSubType`**: Data partition subtype.
- **`partitions::Error`**: Partition table error.
- **`partitions::PartitionTablePartitionSubType`**: Partition table partition subtype.
- **`partitions::PartitionType`**: Partition type enum.
- **`partitions::RawPartitionType`**: Raw partition type.

### Macros
- **`esp_app_desc!()`**: Emits the ESP-IDF app descriptor metadata required by the 2nd-stage bootloader. This macro:
  - Generates the `esp_app_desc_t` structure.
  - Includes app version, project name, build date/time, etc.
  - Must be called at the crate root (typically in `main.rs`).

### Functions
- **`partitions::read_partition_table()`**: Reads the partition table from flash.

### Constants
- **`BUILD_DATE`**: Build date constant.
- **`BUILD_TIME`**: Build time constant.
- **`ESP_IDF_COMPATIBLE_VERSION`**: ESP-IDF compatible version.
- **`MMU_PAGE_SIZE`**: MMU page size.
- **`partitions::PARTITION_TABLE_MAX_LEN`**: Maximum partition table length.

