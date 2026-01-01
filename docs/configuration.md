# Configuration (esp-config)

Rust-on-ESP “Configuration”:
https://docs.espressif.com/projects/rust/book/application-development/configuration.html

## What esp-config is for

The `esp-config` crate provides a way to manage configuration settings that don’t fit into Cargo features, for `esp-*` crates.

Reference:
https://docs.espressif.com/projects/rust/book/application-development/configuration.html

## How to set configuration

Rust-on-ESP describes two primary methods:

1) **Environment variables** (e.g. `ESP_HAL_CONFIG_*`)
2) **`.cargo/config.toml`** `[env]` section (persistent, committed)

Rust-on-ESP recommends doing a **clean build** after changing `.cargo/config.toml`.

Reference:
https://docs.espressif.com/projects/rust/book/application-development/configuration.html

## Multi-config projects (optional pattern)

If you want multiple build configurations (different boards/targets), Rust-on-ESP suggests:

- a baseline `.cargo/config.toml`
- additional config files under `.cargo/`
- (recommended) Cargo aliases to select configs

Reference:
https://docs.espressif.com/projects/rust/book/application-development/configuration.html

