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
- `espup` to install/maintain the ESP Rust toolchains (handy when working across multiple machines)
  - curated via https://github.com/esp-rs/awesome-esp-rust
- `esp-generate` to bootstrap new no_std projects (if you want a fresh template)
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

