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

## Complete API inventory

Full API index: https://docs.rs/critical-section/1.2.0/critical_section/all.html

### Structs
- **`CriticalSection`**: Token proving you're in a critical section.
- **`Mutex<T>`**: Minimal mutex primitive for `no_std` critical-section-based locking.
- **`RestoreState`**: State to restore interrupts after a critical section.

### Traits
- **`Impl`**: Platform-specific implementation trait (usually provided by HAL).

### Macros
- **`set_impl!(impl)`**: Set the platform-specific implementation (usually called by HAL).

### Functions
- **`acquire()`**: Acquire a critical section (low-level, returns restore state).
- **`release(restore_state)`**: Release a critical section (low-level).
- **`with(|cs| ...)`**: Enter a critical section and pass a token used to borrow protected data (most common API).

### Type Aliases
- **`RawRestoreState`**: Raw platform-specific restore state type.

