# `esp-println` (0.16.1)

## What it does in this repo

Provides low-level printing over UART and (optionally) a `log` facade backend.

In `src/bin/main.rs` we initialize logging via:

- `esp_println::logger::init_logger_from_env();`

## Key links

- API docs (docs.rs): https://docs.rs/esp-println/0.16.1/esp_println/
- Upstream repo (in esp-hal workspace): https://github.com/esp-rs/esp-hal/tree/main/esp-println
- Rust-on-ESP logging chapter: https://docs.espressif.com/projects/rust/book/application-development/logging.html

## APIs you’ll likely use first

- `esp_println::logger::init_logger_from_env()`

## Practical note

Because this uses the `log` facade, log filtering is typically controlled with `RUST_LOG` (how you set it depends on your build/flash workflow).

## How to use it

### 1) Initialize the logger (this repo’s approach)

```rust
esp_println::logger::init_logger_from_env();
```

### 2) Log via `log` macros

```rust
use log::{info, warn};

info!("boot");
warn!("something looks off");
```

## Complete API inventory

Full API index: https://docs.rs/esp-println/0.16.1/esp_println/all.html

### Structs
- **`Printer`**: Low-level UART printer (used internally by macros).

### Macros
- **`dbg!(...)`**: Debug print macro (like `std::dbg!` but for UART).
- **`print!(...)`**: Print to UART without newline.
- **`println!(...)`**: Print to UART with newline.

### Functions
- **`logger::init_logger(...)`**: Initialize logger with explicit configuration.
- **`logger::init_logger_from_env()`**: Initialize logger based on environment configuration (`RUST_LOG`).

### Modules
- **`esp_println::logger`**: Logger backend wiring for the `log` facade (what this repo uses).

