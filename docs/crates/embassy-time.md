# `embassy-time` (0.5.0)

## What it does in this repo

Provides async-friendly timers/sleep/yield primitives.

In `src/bin/main.rs` we use:

- `embassy_time::{Duration, Timer};`
- `Timer::after(Duration::from_secs(1)).await;`

## Key links

- API docs (docs.rs): https://docs.rs/embassy-time/0.5.0/embassy_time/
- Embassy book: https://embassy.dev/book/

## APIs you’ll likely use first

- `embassy_time::Timer::after(...)`
- `embassy_time::Duration`

## How to use it

`embassy-time` is the “sleep/timer” crate for Embassy-based async code.

- API docs: https://docs.rs/embassy-time/0.5.0/embassy_time/

### Code sketch: periodic async loop

```rust
use embassy_time::{Duration, Timer};
use log::info;

async fn periodic() -> ! {
    loop {
        info!("hello");
        Timer::after(Duration::from_secs(1)).await;
    }
}
```

## Complete API inventory

Full API index: https://docs.rs/embassy-time/0.5.0/embassy_time/all.html

### Structs
- **`Delay`**: Type implementing async delays and blocking `embedded-hal` delays.
- **`Duration`**: Represents the difference between two `Instant`s.
- **`Instant`**: An instant in time, based on the MCU's clock ticks since startup.
- **`Ticker`**: Asynchronous stream that yields every `Duration`, indefinitely.
- **`TimeoutError`**: Error returned by `with_timeout` and `with_deadline` on timeout.
- **`Timer`**: A future that completes at a specified `Instant`.

### Constants
- **`TICK_HZ`**: Ticks per second of the global timebase.

### Traits
- **`WithTimeout`**: Provides functions to run a given future with a timeout or a deadline.

### Functions
- **`block_for(duration)`**: Blocks for at least `duration`.
- **`with_deadline(deadline, future)`**: Runs a given future with a deadline time.
- **`with_timeout(timeout, future)`**: Runs a given future with a timeout.

