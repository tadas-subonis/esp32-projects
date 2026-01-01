# `critical-section` (1.2.0)

## What it does in this repo

Provides a portable “enter a critical section” API for `no_std` embedded contexts.

You’ll typically use it when protecting shared state accessed from interrupts/tasks.

## Key links

- API docs (docs.rs): https://docs.rs/critical-section/1.2.0/critical_section/
- Crate page: https://crates.io/crates/critical-section

## APIs you’ll likely use first

- `critical_section::with(|cs| { ... })`

## How to use it

Use `critical_section::with` to run code with interrupts (or the platform’s equivalent) masked, so you can safely access shared state.

```rust
use critical_section::Mutex;

static COUNTER: Mutex<core::cell::Cell<u32>> = Mutex::new(core::cell::Cell::new(0));

fn bump() {
    critical_section::with(|cs| {
        let v = COUNTER.borrow(cs).get();
        COUNTER.borrow(cs).set(v + 1);
    });
}
```

## API inventory

Full API index:
- https://docs.rs/critical-section/1.2.0/critical_section/all.html

- **Functions**
  - **`critical_section::with(|cs| ...)`**: enter a critical section and pass a token used to borrow protected data.

- **Types**
  - **`Mutex<T>`**: minimal mutex primitive for `no_std` critical-section-based locking.

