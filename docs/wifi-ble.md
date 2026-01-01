# Wi‑Fi / BLE (high-level)

This repo initializes the radio and Wi‑Fi controller via `esp-radio`.

## Where to look

- `src/bin/main.rs`: calls `esp_radio::init()` and then `esp_radio::wifi::new(...)`.
- The `esp-radio` crate is part of the esp-rs ecosystem; it integrates with `esp-hal` and can be used with async networking stacks.

For protocol-level networking, this repo includes:

- `embassy-net`
- `smoltcp`

## Practical development notes

- Keep buffers off stack (network buffers can be large).
- Be cautious with logging volume (Wi‑Fi stacks can be chatty).
- Prefer “bring-up in layers”:
  - radio init
  - Wi‑Fi init
  - join AP (DHCP)
  - then TCP/UDP services

## Example inspiration

The esp-rs ecosystem maintains examples you can adapt:
https://github.com/esp-rs/esp-hal/tree/main/examples

The curated list of ecosystem tools/projects:
https://github.com/esp-rs/awesome-esp-rust

