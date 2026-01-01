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

