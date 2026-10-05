# AGENTS.md

This file documents **project-specific conventions and guardrails** for AI coding agents (and humans) working in this repository.

**IMPORTANT**: Always consult this file (AGENTS.md), the `docs/` folder, AND the generated Rust documentation in `target/riscv32imc-unknown-none-elf/doc/` when developing. The `docs/` folder contains detailed crate-specific documentation, architectural patterns, and implementation guides. The generated docs (via `cargo doc`) provide interactive HTML documentation with exact API signatures—explore them interactively for precise type information.

## Project summary (what this repo is)

- **Target**: ESP32-S3 (Xtensa), `xtensa-esp32s3-none-elf` (see `rust-toolchain.toml` and `.cargo/config.toml`)
- **Runtime model**: `no_std` + async via Embassy, integrated via `esp-rtos`
- **HAL**: `esp-hal` `~1.0` (with `unstable` feature enabled)
- **Wi‑Fi/BLE controller**: `esp-radio`
- **Logging**: `log` facade via `esp-println` logger (see `src/bin/main.rs`)
- **Heap**: `esp-alloc` using PSRAM (`esp_alloc::psram_allocator!`) for large buffers like the SH8601 framebuffer
- **Bootloader/OTA support**: ESP-IDF 2nd stage bootloader support crate `esp-bootloader-esp-idf`

## Default development board (hardware spec)

**Default board we develop against:** **Waveshare ESP32‑S3 Touch AMOLED 1.8"** (ESP32‑S3R8, SH8601 QSPI AMOLED, FT3168 touch, QMI8658C IMU, PCF85063A RTC, AXP2101 PMU, TCA9554 expander).

- **Before touching anything hardware-related** (pin mapping, buses, I²C addresses, reset/power sequencing, display/touch controllers), **consult the board spec** in:
  - `docs/devices/waveshare-esp32-s3-touch-amoled-1.8.md`
- **Also consult the vendor spec/wiki** (and match board revision):
  - `https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.8`

This is the canonical reference to avoid mixing ESP32-C3 vs ESP32-S3 assumptions and to prevent “guessed wiring” regressions.

Helpful background reading:
- Rust-on-ESP “Application Development” chapters (bootloader, configuration, logging, alloc, async, testing, OTA): `https://docs.espressif.com/projects/rust/book/application-development/index.html`
- ESP Rust ecosystem index (tools, templates, examples): `https://github.com/esp-rs/awesome-esp-rust`

## Golden rules for agents (don’t break these)

- **Don’t accidentally add `std`**: This repo is intentionally `#![no_std]`. Only introduce `std` if explicitly requested.
- **Don’t remove/disable `build.rs`**: It wires up linker args (e.g. `linkall.x`) and prints friendlier link errors.
- **Be stingy with stack**: Keep buffers off stack; prefer static storage or heap where appropriate. Note we already deny `clippy::large_stack_frames`.
- **Keep the target stable**: Don’t change `xtensa-esp32s3-none-elf` / `esp32s3` without a clear reason and explicit approval.
- **Dependency upgrades are opt-in**: Don’t bump versions unless asked (embedded regressions are costly).
- **Don’t guess hardware wiring**: For anything involving pin mapping, buses, I²C addresses, display/touch controllers, etc., consult the board docs in `docs/devices/` (especially `docs/devices/waveshare-esp32-s3-touch-amoled-1.8.md`) and verify against the schematic/board revision.

## Waveshare ESP32‑S3 Touch AMOLED 1.8" bring-up checklist (avoid “black screen” wandering)

When the firmware flashes but the screen is blank/hung, **do these in order**:

- **PSRAM mode (critical)**:
  - This board uses **octal PSRAM**. Ensure `.cargo/config.toml` includes:
    - `ESP_HAL_CONFIG_PSRAM_MODE="octal"`
  - Symptom of wrong PSRAM mode: PSRAM auto-detect logs show “size is 0” or the app panics/hangs early.

- **Display bus wiring (QSPI)**:
  - SH8601 uses ESP32-S3 `SPI2` in QSPI mode with `SIO0..3 = GPIO4..7`.
  - **CS/SCK are easy to swap**. If display init panics or writes do nothing, re-check the mapping against the board doc and the board’s schematic.

- **GPIO expander gating reset/power (TCA9554)**:
  - The display reset/power lines are behind the expander (EXIO0 LCD_RESET, EXIO1 DSI_PWR_EN, etc.).
  - The expander address is **strap-dependent**: commonly `0x20` or `0x24`.
  - **Don’t hardcode one address**; probe/scan and log which one ACKs.

- **I²C must never hang the whole boot**:
  - Always configure **I²C timeouts** (bus + software timeout) so a missing/stuck device returns an error instead of wedging the firmware during bring-up.

- **Panic/backtrace tooling**:
  - Prefer `esp-backtrace` (`panic-handler` + `println`) during hardware bring-up so failures produce actionable output instead of a silent hang.

- **Known-good reference implementation (sanity check)**:
  - The public Waveshare example in [`georgik/esp32-conways-game-of-life-rs`](https://github.com/georgik/esp32-conways-game-of-life-rs/tree/main/waveshare-esp32-s3-touch-amoled-1_8) is a good cross-check for pin mapping and init sequencing when troubleshooting.

## Quickstart: build / flash / monitor

### Build

```bash
cargo build --release --target xtensa-esp32s3-none-elf
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
cargo espflash flash --release --target xtensa-esp32s3-none-elf --chip esp32s3 --monitor

# espflash (direct)
espflash flash --chip esp32s3 --monitor target/xtensa-esp32s3-none-elf/release/waveshare-esp32-sandbox-1
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

- **Preferred approach for persistent config**: commit a `.cargo/config.toml` `[env]` section (this repo does).
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

## Community and support

- **Matrix room**: `#esp-rs:matrix.org` - Active community including Espressif employees
  - https://matrix.to/#/#esp-rs:matrix.org
- **Learning resources**: See `docs/references.md` for comprehensive list of books, blogs, video courses, and example projects

## Ecosystem pointers (useful tools/templates/examples)

From the ESP Rust “awesome list”:
- **Tooling**: 
  - `espup` - Install and maintain ESP Rust toolchains (useful for multi-machine setups)
  - `espflash` / `cargo-espflash` - Serial flasher and monitor
  - `Wokwi Simulator` - Web-based simulator for testing without hardware (https://wokwi.com/)
  - `esp-web-flash-server` / `wokwi-server` - Remote development support for VS Code containers
- **Templates**: 
  - `esp-generate` (no_std) - Bootstrap new no_std projects
  - `esp-idf-template` (std via ESP-IDF) - For std-based projects
- **Example projects**: See `docs/references.md` for curated list of no_std examples demonstrating patterns (MQTT, OTA, BLE, sensors, etc.)

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
  - `docs/display-graphics.md` - TFT display and graphics programming

- **Working with real hardware**: Start from `docs/devices/` for board-specific wiring (pinouts, buses, controller ICs, I²C addresses) and only then map those onto the crate APIs used by this repo. Be especially careful not to mix **ESP32-C3 (RISC-V)** vs **ESP32-S3 (Xtensa)** assumptions.

- **Learning and reference**: 
  - `docs/learning-resources.md` - Curated learning materials, example projects, and learning paths
  - `docs/references.md` - Comprehensive reference list of resources

- **Practical development**: See `docs/practical-tips.md` for patterns, gotchas, and best practices
- **Troubleshooting**: Start with `docs/troubleshooting.md`, then check crate-specific docs

- **Understanding architecture**: Read `docs/stack.md` for overall system design

The `docs/` folder complements AGENTS.md by providing:
- Detailed API usage examples
- Crate-specific patterns and best practices
- Troubleshooting guides
- Implementation details that don't fit in AGENTS.md's convention-focused format

### Generated Rust documentation (cargo doc)

**Always explore the generated documentation interactively** in `target/xtensa-esp32s3-none-elf/doc/` for precise API details:

 - **Generate docs**: `cargo doc --target xtensa-esp32s3-none-elf`
 - **Open in browser**: `cargo doc --target xtensa-esp32s3-none-elf --open`
- **Or navigate directly**: Open `target/xtensa-esp32s3-none-elf/doc/index.html` in a browser

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

