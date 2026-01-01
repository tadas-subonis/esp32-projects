# Testing

Rust-on-ESP “Testing”:
https://docs.espressif.com/projects/rust/book/application-development/testing.html

## Recommended approach

Rust-on-ESP strongly recommends:

- **Host tests first** where possible (fast, CI-friendly, avoids flash wear)
- **Hardware-in-loop (HIL)** when hardware is required

Reference:
https://docs.espressif.com/projects/rust/book/application-development/testing.html

## Hardware-in-loop testing (HIL)

Rust-on-ESP describes using:

- `embedded-test` framework
  - https://github.com/probe-rs/embedded-test
- `probe-rs` to flash/run tests
  - https://probe.rs

And warns that you must use the correct port (USB‑Serial‑JTAG where available) for Espressif devkits.

Reference:
https://docs.espressif.com/projects/rust/book/application-development/testing.html

## In this repo (current state)

This repository doesn’t currently include an `embedded-test` setup. If you want HIL tests here, we can add them later (with a clear hardware target + runner workflow).

