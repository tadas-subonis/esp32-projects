# Build / flash / monitor

## Build

```bash
cargo build --release --target riscv32imc-unknown-none-elf
```

The output ELF ends up at:

```text
target/riscv32imc-unknown-none-elf/release/waveshare-esp32-sandbox-1
```

## Flash + monitor (recommended)

Rust-on-ESP notes that when using `espflash` and you don’t provide a bootloader or partition table, `espflash` uses defaults (good for getting started):
https://docs.espressif.com/projects/rust/book/application-development/bootloader.html

### Using cargo-espflash

```bash
cargo espflash flash --release --target riscv32imc-unknown-none-elf --chip esp32c3 --monitor
```

### Using espflash directly

```bash
espflash flash --chip esp32c3 --monitor target/riscv32imc-unknown-none-elf/release/waveshare-esp32-sandbox-1
```

## Custom bootloader / partitions (advanced)

If you need custom partitions or a custom ESP-IDF second stage bootloader, Rust-on-ESP describes the flow and flags:
https://docs.espressif.com/projects/rust/book/application-development/bootloader.html

`espflash` also supports a configuration file:
https://github.com/esp-rs/espflash/tree/main/espflash#configuration-file

