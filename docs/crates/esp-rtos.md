# `esp-rtos` (0.2.0)

## What it does in this repo

Provides the runtime integration that ties `esp-hal` to Embassy:

- `#[esp_rtos::main]` attribute macro for an async `main`
- scheduler/time-driver wiring via `esp_rtos::start(...)`

In `src/bin/main.rs`, we:

- declare `#[esp_rtos::main] async fn main(spawner: Spawner) -> !`
- call `esp_rtos::start(timg0.timer0, sw_interrupt.software_interrupt0);`

## Key links

- API docs (docs.rs): https://docs.rs/esp-rtos/0.2.0/esp_rtos/
- Crate page: https://crates.io/crates/esp-rtos
- Rust-on-ESP async overview: https://docs.espressif.com/projects/rust/book/application-development/async.html

## APIs you’ll likely use first

- `#[esp_rtos::main]` (entrypoint macro)
- `esp_rtos::start(timer, software_interrupt)` (start Embassy integration)

## Gotchas

- If you see link errors mentioning `esp_rtos_*` (and `esp-radio` “no scheduler enabled”), it often means the runtime wasn’t started correctly (see the hint printed by `build.rs`).

## How to use it (what matters in practice)

### 1) Use the `#[esp_rtos::main]` entrypoint

This macro gives you an async `main` and a `Spawner`:

```rust
use embassy_executor::Spawner;

#[esp_rtos::main]
async fn main(_spawner: Spawner) -> ! {
    loop {}
}
```

### 2) Start the integration (timer + software interrupt)

This repo uses TIMG0 + SW interrupt 0 to start Embassy integration:

```rust
use esp_hal::timer::timg::TimerGroup;

let timg0 = TimerGroup::new(peripherals.TIMG0);
let sw_interrupt =
    esp_hal::interrupt::software::SoftwareInterruptControl::new(peripherals.SW_INTERRUPT);

esp_rtos::start(timg0.timer0, sw_interrupt.software_interrupt0);
```

## Complete API inventory

Full API index: See local docs at `target/riscv32imc-unknown-none-elf/doc/esp_rtos/all.html`

### Structs
- **`CurrentThreadHandle`**: Handle to the current thread.
- **`embassy::Executor`**: Embassy executor integration.
- **`embassy::InterruptExecutor`**: Embassy interrupt executor integration.
- **`semaphore::Semaphore`**: Semaphore for synchronization.

### Traits
- **`TimerSource`**: Trait for timer sources used by the runtime.
- **`embassy::Callbacks`**: Callback trait for Embassy integration.

### Attribute Macros
- **`#[esp_rtos::main]`**: Defines the async entrypoint and provides a `Spawner`. This macro:
  - Sets up the async runtime.
  - Provides a `Spawner` parameter to your `main` function.
  - Handles the `!` return type (infinite loop).

### Functions
- **`start(timer, software_interrupt)`**: Boots the Embassy integration using:
  - `timer`: A hardware timer (e.g., `timg0.timer0` from `esp-hal`).
  - `software_interrupt`: A software interrupt (e.g., `sw_interrupt.software_interrupt0`).
- **`start_with_idle_hook(timer, software_interrupt, idle_hook)`**: Same as `start` but with an idle hook callback.

