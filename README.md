# waveshare-esp32-sandbox-1

ESP32-C3 Rust project using `no_std`, Embassy async runtime, and the `esp-rs` ecosystem.

## Quick Start

### Prerequisites

- Rust toolchain (see `rust-toolchain.toml`)
- ESP Rust toolchain (install via `espup` or follow [setup guide](./docs/setup.md))
- `cargo-espflash` or `espflash` for flashing (install with `make install-tools`)

### WSL2 USB Passthrough (Windows → Linux)

If you're developing in WSL2 but your ESP32 device is physically connected to Windows, you need to map the USB device from Windows to Linux using `usbipd-win`.

**Installation (Windows):**

```powershell
winget install usbipd
```

**Setup Steps:**

1. **Find your device BUSID** (run on Windows PowerShell):
   ```powershell
   usbipd list
   ```
   Look for your ESP32 device (e.g., "Silicon Labs CP210x USB to UART Bridge") and note the BUSID (e.g., `1-2`).

2. **Bind the device** (run once per Windows restart, requires admin):
   ```powershell
   usbipd bind --busid <BUSID>
   ```
   Replace `<BUSID>` with your actual bus ID (e.g., `1-2`).

3. **Attach to WSL** (run each time the device is plugged in):
   ```powershell
   usbipd attach --wsl --busid <BUSID>
   ```

**On Linux (WSL) side:**

After attaching from Windows, the device should appear in WSL. If it doesn't show up automatically:

1. **Load usbip kernel modules** (if not already loaded):
   ```bash
   sudo modprobe usbip-core
   sudo modprobe usbip-host
   ```

2. **Verify the device appears**:
   ```bash
   lsusb
   ```
   You should see your ESP32 device listed (e.g., "Silicon Labs CP210x USB to UART Bridge").

3. **Check for serial device**:
   ```bash
   ls -l /dev/tty* | grep -i usb
   # or
   ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
   ```
   The device typically appears as `/dev/ttyUSB0` or `/dev/ttyACM0`.

4. **Set permissions** (if needed):
   ```bash
   # Add your user to the dialout group (one-time setup)
   sudo usermod -aG dialout $USER
   # Then log out and back in, or run:
   newgrp dialout
   ```

5. **Use the device**:
   Once the device is visible, you can use it with `cargo-espflash` or `espflash`:
   ```bash
   make dev  # Build, flash, and monitor
   ```

**Notes:**
- Binding persists across device unplug/replug but not across Windows restarts
- You'll need to re-attach after each device unplug/replug
- If you use a third-party firewall, you may need to allow inbound TCP port `3240`
- See [docs/wsl2-usb.md](./docs/wsl2-usb.md) for more details

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
