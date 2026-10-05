# `esp-alloc` (0.9.0)

## What it does in this repo

Provides a `no_std` heap allocator for Espressif targets.

In `src/bin/main.rs` we install the heap using:

- `esp_alloc::heap_allocator!(#[esp_hal::ram(reclaimed)] size: 66320);`

Rust-on-ESP explains reclaimed RAM heaps and allocator trade-offs:
https://docs.espressif.com/projects/rust/book/application-development/alloc.html

## Key links

- API docs (docs.rs): https://docs.rs/esp-alloc/0.9.0/esp_alloc/
- Crate page: https://crates.io/crates/esp-alloc
- Rust-on-ESP allocating memory: https://docs.espressif.com/projects/rust/book/application-development/alloc.html

## APIs you’ll likely use first

- `esp_alloc::heap_allocator!(...)` macro

## Gotchas

- You can only have **one** global allocator (Rust-on-ESP notes this explicitly).
- Be mindful of fragmentation; prefer a few stable allocations and reuse buffers.

## How to use it

### 1) Install the allocator (this repo’s approach)

This repo installs a heap in reclaimed RAM:

```rust
esp_alloc::heap_allocator!(#[esp_hal::ram(reclaimed)] size: 66320);
```

### 2) Use `alloc` types after heap init

Once the allocator is installed, you can use `alloc` collections:

```rust
extern crate alloc;

use alloc::vec::Vec;

let mut v: Vec<u8> = Vec::new();
v.extend_from_slice(b"hello");
```

## Complete API inventory

Full API index: See local docs at `target/riscv32imc-unknown-none-elf/doc/esp_alloc/all.html`

### Structs
- **`AnyMemory`**: Generic memory type that can represent any memory region.
- **`EspHeap`**: The ESP heap allocator implementation.
- **`ExternalMemory`**: External memory region (e.g., PSRAM).
- **`HeapRegion`**: A single heap region configuration.
- **`HeapStats`**: Heap statistics (used/free memory, etc.).
- **`InternalMemory`**: Internal memory region.
- **`RegionStats`**: Statistics for a specific memory region.

### Enums
- **`MemoryCapability`**: Memory capability flags (executable, writable, etc.).

### Macros
- **`heap_allocator!(#[esp_hal::ram(...)] size: N)`**: Installs the global allocator. Supports:
  - `#[esp_hal::ram(reclaimed)]`: Use reclaimed RAM (from 2nd stage bootloader).
  - `#[esp_hal::ram(psram)]`: Use PSRAM (if available).
  - `size`: Size of the heap in bytes.
- **`psram_allocator!(size: N)`**: Installs a PSRAM-specific allocator.

### Statics
- **`HEAP`**: Global heap instance (used internally).

