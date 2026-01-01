# Logging

This repo uses the Rust `log` facade (e.g. `info!`, `warn!`, `error!`) with an `esp-println` logger backend.

Rust-on-ESP “Logging” overview:
https://docs.espressif.com/projects/rust/book/application-development/logging.html

## How logging is wired in this repo

`src/bin/main.rs` calls:

- `esp_println::logger::init_logger_from_env();`

That means **log level selection is controlled via environment variables** (commonly `RUST_LOG`).

## Practical guidelines

- UART is slow: keep logs short and avoid logging in tight loops at high levels.
- Prefer consistent prefixes for repeated logs (e.g. `wifi:` / `net:`).

## Alternative: defmt + probe-rs

Rust-on-ESP highlights `defmt` as a compact binary-logging framework and recommends pairing it with `probe-rs`:
https://docs.espressif.com/projects/rust/book/application-development/logging.html

If you switch to `defmt`, you must also adjust linker scripts and runtime wiring (see `build.rs` hints for missing `_defmt_*` symbols).

