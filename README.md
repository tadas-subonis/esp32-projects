# waveshare-esp32-sandbox-1

ESP32-C3 Rust project using `no_std`, Embassy async runtime, and the `esp-rs` ecosystem.

## Quick Start

### Prerequisites

- Rust toolchain (see `rust-toolchain.toml`)
- ESP Rust toolchain (install via `espup` or follow [setup guide](./docs/setup.md))
- `cargo-espflash` or `espflash` for flashing (install with `make install-tools`)

### Build and Flash

The easiest way to get started:

```bash
make dev          # Build, flash, and start serial monitor
```

Or step by step:

```bash
make build        # Build release binary
make flash        # Flash to device
make monitor      # Start serial monitor
```

## Makefile Commands

This project includes a comprehensive Makefile to speed up development. Run `make help` to see all available commands.

### Build Commands

```bash
make build              # Build release (default)
make build-release      # Build release
make build-debug        # Build debug
```

### Flash & Monitor

```bash
make flash              # Build release and flash
make flash-debug        # Build debug and flash
make monitor            # Start serial monitor
make flash-monitor      # Flash and monitor in one command (recommended)
make dev                # Alias for flash-monitor
```

### Code Quality

```bash
make fmt                # Format code
make fmt-check          # Check formatting
make clippy             # Run clippy linter
make clippy-fix         # Run clippy with auto-fix
make check              # Run fmt-check + clippy
```

### Documentation

```bash
make doc                # Generate documentation
make doc-open           # Generate and open docs in browser
```

### Testing

```bash
make test               # Run all tests
make test-host          # Run host-only tests
```

### Utilities

```bash
make clean              # Clean build artifacts
make clean-all          # Deep clean
make verify             # Verify build succeeds
make size               # Show binary size analysis
make install-tools      # Install required tools (cargo-espflash)
```

### Environment Variables

You can customize behavior with environment variables:

```bash
# Set log level
make flash RUST_LOG=debug

# Use debug profile
make build PROFILE=debug
```

## Project Structure

- `src/` - Source code
- `docs/` - Detailed documentation (see [docs/README.md](./docs/README.md))
- `AGENTS.md` - Project conventions and guardrails for AI coding agents
- `Makefile` - Development commands (this file documents it)
- `.cargo/config.toml` - Cargo configuration (target, runner, rustflags)

## Documentation

Comprehensive documentation is available in the `docs/` folder:

- **[Setup Guide](./docs/setup.md)** - Environment setup and toolchain installation
- **[Build/Flash/Monitor](./docs/build-flash-monitor.md)** - Detailed build and deployment instructions
- **[Stack & Architecture](./docs/stack.md)** - System architecture overview
- **[Async & Embassy](./docs/async-embassy.md)** - Async programming patterns
- **[Crate API References](./docs/crates/README.md)** - Curated API documentation
- **[Troubleshooting](./docs/troubleshooting.md)** - Common issues and solutions

See [docs/README.md](./docs/README.md) for the complete documentation index.

## Project Details

- **Target**: ESP32-C3 (RISC‑V), `riscv32imc-unknown-none-elf`
- **Runtime**: `no_std` + async via Embassy, integrated via `esp-rtos`
- **HAL**: `esp-hal` ~1.0 (with `unstable` feature)
- **Wi‑Fi/BLE**: `esp-radio`
- **Logging**: `log` facade via `esp-println`
- **Heap**: `esp-alloc` using reclaimed RAM
- **Bootloader**: ESP-IDF 2nd stage bootloader support

## Resources

- [Rust-on-ESP Book](https://docs.espressif.com/projects/rust/book/) - Official documentation
- [ESP Rust Ecosystem](https://github.com/esp-rs/awesome-esp-rust) - Tools, templates, examples
- [Matrix Community](https://matrix.to/#/#esp-rs:matrix.org) - `#esp-rs:matrix.org`

## License

[Add your license here]
