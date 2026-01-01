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

## API inventory

Full API index:
- https://docs.rs/log/0.4.27/log/all.html

- **Macros** (main user-facing API)
  - **`trace!` / `debug!` / `info!` / `warn!` / `error!`**: emit a log record at the given level.

- **Core types**
  - **`Level` / `LevelFilter`**: severity levels + filter levels.
  - **`Metadata` / `Record`**: structured log record data passed to the logger.

- **Core traits/functions** (logger implementors)
  - **`Log`**: trait implemented by log backends.
  - **`set_logger(...)` / `set_max_level(...)`**: install global logger and configure max level.

