# Environment setup

This repo targets `riscv32imc-unknown-none-elf` (ESP32‑C3). The Rust toolchain is configured via `rust-toolchain.toml`.

## Install Rust prerequisites

- Install Rust with `rustup` (and ensure `rust-src` is installed for rust-analyzer).
- Ensure the target exists:

```bash
rustup target add riscv32imc-unknown-none-elf
rustup component add rust-src
```

## Install ESP Rust tooling

The esp-rs ecosystem commonly uses:

- `espflash` / `cargo-espflash` for flashing and serial monitoring (bootloader + partition defaults included)
  - https://docs.espressif.com/projects/rust/book/application-development/bootloader.html
- **`espup`** - Install and maintain ESP Rust toolchains (recommended for multi-machine setups)
  - Automatically installs and configures the required toolchains
  - Useful when working across multiple machines or setting up CI/CD
  - https://github.com/esp-rs/espup

  ```bash
  cargo install espup
  espup install
  ```
- **Wokwi Simulator** - Web-based simulator for testing without hardware
  - Useful for rapid prototyping and testing logic without physical hardware
  - Supports Rust on ESP32 chips
  - https://wokwi.com/
- **`esp-generate`** - Bootstrap new no_std projects
  - mentioned in Rust-on-ESP “Logging” and “Testing”
  - https://docs.espressif.com/projects/rust/book/application-development/logging.html
  - https://docs.espressif.com/projects/rust/book/application-development/testing.html

Install flash tooling:

```bash
cargo install espflash
# or
cargo install cargo-espflash
```

## Editor/IDE notes

- Make sure rust-analyzer is using the correct target (`riscv32imc-unknown-none-elf`) when checking code.
- `no_std` means many “normal” crates won’t work unless they support `no_std`.

