# AGENTS.md

This file documents **project-specific conventions and guardrails** for AI coding agents (and humans) working in this repository.

**IMPORTANT**: Always consult this file (AGENTS.md), the `docs/` folder, AND the generated Rust documentation in `target/riscv32imc-unknown-none-elf/doc/` when developing. The `docs/` folder contains detailed crate-specific documentation, architectural patterns, and implementation guides. The generated docs (via `cargo doc`) provide interactive HTML documentation with exact API signatures—explore them interactively for precise type information.

## Project summary (what this repo is)

- **Target**: ESP32-C3 (RISC‑V), `riscv32imc-unknown-none-elf` (see `rust-toolchain.toml`)
- **Runtime model**: `no_std` + async via Embassy, integrated via `esp-rtos`
- **HAL**: `esp-hal` `~1.0` (with `unstable` feature enabled)
- **Wi‑Fi/BLE controller**: `esp-radio`
- **Logging**: `log` facade via `esp-println` logger (see `src/bin/main.rs`)
- **Heap**: `esp-alloc` using reclaimed RAM (see `src/bin/main.rs`)
- **Bootloader/OTA support**: ESP-IDF 2nd stage bootloader support crate `esp-bootloader-esp-idf`

Helpful background reading:
- Rust-on-ESP “Application Development” chapters (bootloader, configuration, logging, alloc, async, testing, OTA): `https://docs.espressif.com/projects/rust/book/application-development/index.html`
- ESP Rust ecosystem index (tools, templates, examples): `https://github.com/esp-rs/awesome-esp-rust`

## Golden rules for agents (don’t break these)

- **Don’t accidentally add `std`**: This repo is intentionally `#![no_std]`. Only introduce `std` if explicitly requested.
- **Don’t remove/disable `build.rs`**: It wires up linker args (e.g. `linkall.x`) and prints friendlier link errors.
- **Be stingy with stack**: Keep buffers off stack; prefer static storage or heap where appropriate. Note we already deny `clippy::large_stack_frames`.
- **Keep the target stable**: Don’t change `riscv32imc-unknown-none-elf` / `esp32c3` without a clear reason and explicit approval.
- **Dependency upgrades are opt-in**: Don’t bump versions unless asked (embedded regressions are costly).

## Quickstart: build / flash / monitor

### Build

```bash
cargo build --release --target riscv32imc-unknown-none-elf
```

### Flash + serial monitor (recommended)

This repo expects an ESP-IDF-style bootloader/partition flow; `espflash`/`cargo-espflash` handle defaults and are commonly used.

- Install tooling (one-time):

```bash
cargo install espflash
# or
cargo install cargo-espflash
```

- Flash & monitor:

```bash
# cargo-espflash (nice UX)
cargo espflash flash --release --target riscv32imc-unknown-none-elf --chip esp32c3 --monitor

# espflash (direct)
espflash flash --chip esp32c3 --monitor target/riscv32imc-unknown-none-elf/release/waveshare-esp32-sandbox-1
```

Bootloader context and customizations (partition tables, custom bootloader builds, etc.) are described in:
`https://docs.espressif.com/projects/rust/book/application-development/bootloader.html`

## Logging conventions

We use the Rust `log` facade (macros like `info!`, `warn!`, `error!`) and wire a logger using `esp-println`.

- **Keep logs lightweight** (UART is slow).
- **Prefer structured/consistent prefixes** for repeated logs.
- **Log level selection**: this project calls `esp_println::logger::init_logger_from_env()`, so set `RUST_LOG` in the environment used for building/running.

Background: Rust-on-ESP “Logging” discusses `defmt` vs `log` and mentions `esp-println` as a `log` backend:
`https://docs.espressif.com/projects/rust/book/application-development/logging.html`

## Configuration (esp-config)

For settings that don’t fit cleanly into Cargo features, Espressif’s ecosystem uses `esp-config`.

- **Preferred approach for persistent config**: commit a `.cargo/config.toml` `[env]` section (this repo doesn’t currently have one).
- **CLI env vars win**: environment variables set on the command line override `.cargo/config.toml`.
- **After changing config**: do a clean rebuild.

Background:
`https://docs.espressif.com/projects/rust/book/application-development/configuration.html`

## Memory & allocation guidelines

This project enables heap allocation (`extern crate alloc`) and installs a global allocator via `esp-alloc`.

- **Use heap intentionally**: avoid long-lived fragmentation patterns; keep allocation lifetimes simple.
- **Prefer reclaimed RAM for heap** where appropriate (already used here).
- **One global allocator**: don’t introduce a second one.

Background (heap trade-offs, reclaimed RAM, PSRAM notes):
`https://docs.espressif.com/projects/rust/book/application-development/alloc.html`

## Async/Embassy guidelines

This project uses Embassy via `esp-rtos` integration.

- **Don’t block in async tasks**: use `embassy_time` timers, async I/O, or spawn background work.
- **`esp-hal` async drivers**: drivers start in blocking mode and can be converted with `into_async`.
- **Driver `Send` caveat**: Rust-on-ESP notes async drivers are not `Send` because interrupts are core-bound; on multicore chips, construct/convert on the correct core.

Background:
`https://docs.espressif.com/projects/rust/book/application-development/async.html`

## Testing expectations

- **Host tests first** where possible (fast, CI-friendly, avoids flash wear).
- **Hardware-in-loop (HIL)** when hardware is required; the Rust-on-ESP book describes using `embedded-test` + `probe-rs`, and stresses using the correct debug/USB port (USB‑Serial‑JTAG where available).

Background:
`https://docs.espressif.com/projects/rust/book/application-development/testing.html`

## OTA notes

OTA depends on the second-stage bootloader and partition table layout. Rust-on-ESP currently describes OTA support via the ESP-IDF bootloader support crate `esp-bootloader-esp-idf`, and points to an `esp-hal` OTA example.

Background:
`https://docs.espressif.com/projects/rust/book/application-development/ota.html`

## WSL2 USB passthrough (if you develop in WSL)

If you’re flashing from inside WSL2 but the device is physically attached to Windows, `usbipd-win` is a common setup:
`https://github.com/dorssel/usbipd-win`

Typical flow (run on Windows):

```powershell
winget install usbipd
usbipd list
usbipd bind --busid=<BUSID>      # admin (persistent share)
usbipd attach --wsl --busid=<BUSID>
```

Notes:
- If you use a third-party firewall, `usbipd-win` notes you may need to allow inbound TCP port `3240`.
- Device attach is non-persistent; you may need to re-attach after reboots/unplug events.

## Ecosystem pointers (useful tools/templates/examples)

From the ESP Rust “awesome list”:
- **Tooling**: `espup`, `espflash`
- **Templates**: `esp-generate` (no_std), `esp-idf-template` (std via ESP-IDF)

Reference:
`https://github.com/esp-rs/awesome-esp-rust`

## Practical maturity expectations (from community discussion)

Community feedback often recommends starting with the `esp-rs` ecosystem; people report that configuration can be the trickiest part at first, but once set up, day-to-day development is smooth. The same discussions call out that `esp-generate` helps bootstrap projects quickly, and that many common peripherals (I2C/SPI sensors) already have crate support.

Reference discussion:
`https://www.reddit.com/r/esp32/comments/1l6hgdb/how_mature_is_esp32_rust/`

## Documentation structure and usage

This repository maintains detailed documentation in the `docs/` folder. **Agents should proactively reference these docs** when:

- **Working with specific crates**: Check `docs/crates/` for API patterns, examples, and gotchas
  - `docs/crates/esp-hal.md` - HAL usage patterns
  - `docs/crates/esp-radio.md` - Wi-Fi/BLE controller usage
  - `docs/crates/embassy-*.md` - Async executor and time patterns
  - `docs/crates/smoltcp.md` - Network stack usage
  - See `docs/crates/README.md` for full list

- **Implementing features**: Review relevant topic docs:
  - `docs/async-embassy.md` - Async patterns and Embassy integration
  - `docs/memory-alloc.md` - Heap allocation guidelines
  - `docs/logging.md` - Logging patterns beyond AGENTS.md basics
  - `docs/wifi-ble.md` - Network connectivity patterns

- **Troubleshooting**: Start with `docs/troubleshooting.md`, then check crate-specific docs

- **Understanding architecture**: Read `docs/stack.md` for overall system design

The `docs/` folder complements AGENTS.md by providing:
- Detailed API usage examples
- Crate-specific patterns and best practices
- Troubleshooting guides
- Implementation details that don't fit in AGENTS.md's convention-focused format

### Generated Rust documentation (cargo doc)

**Always explore the generated documentation interactively** in `target/riscv32imc-unknown-none-elf/doc/` for precise API details:

- **Generate docs**: `cargo doc --target riscv32imc-unknown-none-elf`
- **Open in browser**: `cargo doc --target riscv32imc-unknown-none-elf --open`
- **Or navigate directly**: Open `target/riscv32imc-unknown-none-elf/doc/index.html` in a browser

**Use generated docs to:**
- Find exact type signatures, method parameters, and return types
- Verify trait bounds, associated types, and required implementations
- Search for specific APIs, types, or functions across all crates
- Explore trait implementations and see what types implement which traits
- Understand feature flags and conditional compilation (`#[cfg(...)]`)
- Read example code from doc comments
- Cross-reference between related types and modules
- Debug type mismatches by understanding exact API contracts

**When to use generated docs:**
- Need precise type information (exact signatures, generic parameters)
- Verifying correct API usage when compiler errors occur
- Exploring the full API surface of a crate
- Finding all available methods on a type
- Understanding trait requirements and bounds
- Checking feature-gated APIs

**Workflow**: 
1. Read AGENTS.md for constraints and conventions
2. Check `docs/crates/*.md` for patterns and examples
3. **Explore `target/riscv32imc-unknown-none-elf/doc/` interactively** for exact API details
4. Use generated docs to verify types, find methods, and understand trait requirements

The generated docs are the **source of truth** for exact API contracts, while `docs/crates/*.md` provide higher-level patterns and gotchas.

