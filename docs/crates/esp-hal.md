# `esp-hal` (~1.0)

## What it does in this repo

Hardware abstraction layer for ESP32‑C3 peripherals + clocks/timers/interrupts.

Used directly in `src/bin/main.rs` for:

- `esp_hal::Config` (CPU clock config)
- `esp_hal::init(...)` (take peripherals)
- `esp_hal::timer::timg::TimerGroup` (timer used to start Embassy via `esp-rtos`)
- software interrupts (`esp_hal::interrupt::software::SoftwareInterruptControl`)

## Key links

- API docs (docs.rs): https://docs.rs/esp-hal/
- Espressif-hosted chip docs (handy for target-specific items): https://docs.espressif.com/projects/rust/
- Project repo: https://github.com/esp-rs/esp-hal
- Examples: https://github.com/esp-rs/esp-hal/tree/main/examples

Useful module docs on `docs.rs`:

- GPIO: https://docs.rs/esp-hal/latest/esp_hal/gpio/index.html
- Timers: https://docs.rs/esp-hal/latest/esp_hal/timer/index.html
- Clocks: https://docs.rs/esp-hal/latest/esp_hal/clock/index.html
- Interrupts: https://docs.rs/esp-hal/latest/esp_hal/interrupt/index.html

## APIs you’ll likely use first

- **Initialization**
  - `esp_hal::Config::default()`
  - `Config::with_cpu_clock(...)`
  - `esp_hal::init(config)` → `peripherals`
- **Clocks**
  - `esp_hal::clock::CpuClock::max()`
- **Timers**
  - `esp_hal::timer::timg::TimerGroup::new(peripherals.TIMG0)`
- **Interrupt plumbing**
  - `esp_hal::interrupt::software::SoftwareInterruptControl::new(peripherals.SW_INTERRUPT)`
- **RAM placement attributes** (used by `esp-alloc` macro)
  - `#[esp_hal::ram(reclaimed)]` (reclaimed RAM heap)

## Async note

Rust-on-ESP notes that many `esp-hal` drivers are constructed in blocking mode and can be converted to async via `into_async`; async drivers are often `!Send` due to core/interrupt binding:
https://docs.espressif.com/projects/rust/book/application-development/async.html

## How to use it (practical patterns)

### 1) Initialize peripherals

This is the standard `esp-hal` bring-up pattern used in this repo:

```rust
use esp_hal::clock::CpuClock;

let config = esp_hal::Config::default().with_cpu_clock(CpuClock::max());
let peripherals = esp_hal::init(config);
```

### 2) GPIO: create an output pin

From the GPIO module docs, the main “user-facing” types include `Io`, `Output`, and `Level`:
https://docs.rs/esp-hal/latest/esp_hal/gpio/index.html

```rust
use esp_hal::gpio::{Io, Level, Output};

let io = Io::new(peripherals.GPIO, peripherals.IO_MUX);
let mut led = Output::new(io.pins.gpio2, Level::Low); // pick the correct pin for your board

// led.set_high();
// led.set_low();
// led.toggle();
```

### 3) Timers: general-purpose timers (TIMG)

The timer module shows how to wrap a hardware timer into a one-shot or periodic timer:
https://docs.rs/esp-hal/latest/esp_hal/timer/index.html

```rust
use esp_hal::timer::{OneShotTimer, PeriodicTimer};
use esp_hal::timer::timg::TimerGroup;
use embassy_time::Duration;

let timg0 = TimerGroup::new(peripherals.TIMG0);

let mut one_shot = OneShotTimer::new(timg0.timer0);
one_shot.delay_millis(500);

let timg0 = TimerGroup::new(peripherals.TIMG0);
let mut periodic = PeriodicTimer::new(timg0.timer0);
periodic.start(Duration::from_secs(1));
loop {
    periodic.wait();
}
```

## API inventory (navigation-first)

`esp-hal` is a large crate. Instead of duplicating the full rustdoc listing here, use:

- Full API index (All Items): https://docs.rs/esp-hal/latest/esp_hal/all.html
- Module index: https://docs.rs/esp-hal/latest/esp_hal/#modules

Below is a “what you should click first” map:

- **Core entrypoints**
  - **`esp_hal::init(config)`**: takes ownership of peripherals and applies global configuration.
  - **`esp_hal::Config`**: global configuration (clocks, etc).
  - **`esp_hal::peripherals::Peripherals`**: the singleton peripheral container returned by `init`.

- **Concurrency/driver modes**
  - **`esp_hal::Blocking`**: marker for blocking driver mode.
  - **`esp_hal::Async`**: marker for async driver mode.

- **Modules you’ll use early**
  - **`esp_hal::gpio`**: digital I/O pins (`Io`, `Input`, `Output`, `Level`, pull/drive settings).
  - **`esp_hal::timer`**: timers (`PeriodicTimer`, `OneShotTimer`, `TimerGroup`, `systimer`, etc.).
  - **`esp_hal::clock`**: clock configuration (`CpuClock`, PLL/clock tree pieces).
  - **`esp_hal::interrupt`**: interrupt enabling/handlers + software interrupts.
  - **`esp_hal::uart`**: UART peripheral drivers.
  - **`esp_hal::spi`**: SPI peripheral drivers.
  - **`esp_hal::i2c`**: I2C peripheral drivers.

If you want, tell me the exact board/pinout you have and which peripheral you’re using next (UART/I2C/SPI/GPIO), and I’ll add a “golden example” section for that module using the exact types from `esp-hal` v1.0.


