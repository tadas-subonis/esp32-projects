# Troubleshooting

## Build/linker problems

### Don’t remove `build.rs`

This repo’s `build.rs` intentionally:

- adds linker args (including `linkall.x`)
- prints friendlier messages for common “undefined symbol” link failures

If you remove/disable it, you’ll typically get harder-to-debug link errors.

### Common hints from `build.rs`

`build.rs` prints targeted suggestions for:

- missing `defmt` linker script / runtime wiring (`_defmt_*`)
- missing `linkall.x` (`_stack_start`)
- `esp-radio` scheduler not enabled (`esp_rtos_*`)
- missing `embedded-test` linker script
- missing allocator/compat symbols (`malloc`, `free`, etc.)

## Flash/monitor issues

### “No serial port found”

- On Linux/WSL, ensure you have permission to access the `/dev/tty*` device.
- If using WSL2 + Windows-attached hardware, follow `usbipd-win` passthrough steps:
  - https://github.com/dorssel/usbipd-win

### Bootloader / partition table confusion

Rust-on-ESP notes `espflash` can use default bootloader + partition table if you don’t provide them (good for getting started):
https://docs.espressif.com/projects/rust/book/application-development/bootloader.html

For custom layouts, see the same chapter and `espflash` configuration file docs:
https://github.com/esp-rs/espflash/tree/main/espflash#configuration-file

## Async gotchas

Rust-on-ESP emphasizes:

- don’t block in async tasks
- esp-hal async drivers are not `Send` (core/interrupt binding)

Reference:
https://docs.espressif.com/projects/rust/book/application-development/async.html

