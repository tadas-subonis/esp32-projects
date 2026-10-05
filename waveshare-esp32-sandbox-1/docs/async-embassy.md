# Async & Embassy

Rust-on-ESP “Async Options” (recommended reading):
https://docs.espressif.com/projects/rust/book/application-development/async.html

Embassy book:
https://embassy.dev/book/

Rust async book (language-level async):
https://rust-lang.github.io/async-book/

## Key concepts (for this repo)

- Your `main` is `async` (`#[esp_rtos::main] async fn main(...) -> !`).
- Spawn background tasks using the provided `Spawner`.
- Avoid blocking in async contexts: use `embassy_time` timers or async drivers.

## esp-hal blocking vs async drivers

Rust-on-ESP notes:

- drivers are constructed in **blocking mode** by default
- convert to async mode via `into_async`
- async drivers are **not `Send`** (interrupts are core-bound); if you must move a driver between cores, move the blocking version and convert on the destination core

Reference:
https://docs.espressif.com/projects/rust/book/application-development/async.html

## Repo rules that matter

- This project denies `clippy::large_stack_frames`: prefer static storage or heap for large buffers.
- Don’t do CPU-bound busy loops; always `await`/sleep/yield appropriately.

