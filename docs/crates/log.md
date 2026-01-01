# `log` (0.4.27)

## What it does in this repo

Provides the standard logging facade macros (`info!`, `warn!`, `error!`, etc.). The backend is supplied by `esp-println`.

Rust-on-ESP “Logging” explains `log` vs `defmt`:
https://docs.espressif.com/projects/rust/book/application-development/logging.html

## Key links

- API docs (docs.rs): https://docs.rs/log/0.4.27/log/
- Crate page: https://crates.io/crates/log

## APIs you’ll likely use first

- Macros: `trace!`, `debug!`, `info!`, `warn!`, `error!`
- (Occasionally) `log::LevelFilter` when configuring filters in custom setups

## How to use it (in this repo)

This repo wires a logger backend using `esp-println` (see `docs/crates/esp-println.md`), so your code generally just uses macros:

```rust
use log::{debug, error, info, trace, warn};

trace!("very chatty");
debug!("debug details");
info!("boot ok");
warn!("battery low");
error!("wifi connect failed");
```

## Complete API inventory

Full API index: https://docs.rs/log/0.4.27/log/all.html

### Macros (main user-facing API)
- **`debug!(...)`**: Emit a log record at the `Debug` level.
- **`error!(...)`**: Emit a log record at the `Error` level.
- **`info!(...)`**: Emit a log record at the `Info` level.
- **`log!(level, ...)`**: Emit a log record at a specified level.
- **`log_enabled!(level)`**: Check if logging is enabled for a level.
- **`trace!(...)`**: Emit a log record at the `Trace` level.
- **`warn!(...)`**: Emit a log record at the `Warn` level.

### Structs
- **`Metadata`**: Metadata about a log record (level, target, etc.).
- **`MetadataBuilder`**: Builder for `Metadata`.
- **`ParseLevelError`**: Error parsing a log level string.
- **`Record`**: A log record containing metadata and message.
- **`RecordBuilder`**: Builder for `Record`.
- **`SetLoggerError`**: Error setting the global logger.

### Enums
- **`Level`**: Log level (Trace, Debug, Info, Warn, Error).
- **`LevelFilter`**: Log level filter (same variants as `Level` plus `Off`).

### Traits
- **`Log`**: Trait implemented by log backends (what `esp-println` implements).

### Functions
- **`logger()`**: Get the global logger.
- **`max_level()`**: Get the maximum enabled log level.
- **`set_logger_racy(logger)`**: Set the global logger (racy, for single-threaded).
- **`set_max_level_racy(level)`**: Set the maximum log level (racy, for single-threaded).

### Constants
- **`STATIC_MAX_LEVEL`**: Compile-time maximum log level (feature-gated).

