# `embassy-executor` (0.9.1)

## What it does in this repo

Provides the async task executor + `Spawner` used to spawn tasks.

In `src/bin/main.rs`:

- `use embassy_executor::Spawner;`
- `main(spawner: Spawner)`

## Key links

- API docs (docs.rs): https://docs.rs/embassy-executor/0.9.1/embassy_executor/
- Embassy book: https://embassy.dev/book/
- Rust-on-ESP async: https://docs.espressif.com/projects/rust/book/application-development/async.html

## APIs you’ll likely use first

- `embassy_executor::Spawner`
- `Spawner::spawn(...)` (spawning tasks)
- `#[embassy_executor::task]` (when defining tasks; availability depends on your exact setup)

## How to use it (common patterns)

Embassy tasks are `async fn`s registered with `#[embassy_executor::task]` and spawned via a `Spawner`.

Embassy book: https://embassy.dev/book/

### Code sketch: spawning a background task from `main`

```rust
use embassy_executor::Spawner;
use embassy_time::{Duration, Timer};
use log::info;

#[embassy_executor::task]
async fn blinky_task() -> ! {
    loop {
        info!("tick");
        Timer::after(Duration::from_secs(1)).await;
    }
}

#[esp_rtos::main]
async fn main(spawner: Spawner) -> ! {
    spawner.spawn(blinky_task()).unwrap();

    loop {
        Timer::after(Duration::from_secs(60)).await;
    }
}
```

## API inventory (practical)

Full API index:
- https://docs.rs/embassy-executor/0.9.1/embassy_executor/all.html

- **Types**
  - **`Spawner`**: handle used to spawn tasks.

- **Task macro**
  - **`#[embassy_executor::task]`**: marks an `async fn` as a spawnable Embassy task.

