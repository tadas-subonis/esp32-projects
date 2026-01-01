# Memory & heap allocation

Rust-on-ESP “Allocating Memory”:
https://docs.espressif.com/projects/rust/book/application-development/alloc.html

## no_std + alloc

In `no_std`, you can still use Rust collections like `Vec`/`Box` by enabling the `alloc` crate and providing a global allocator.

This repo already:

- enables `extern crate alloc;`
- installs a global allocator via `esp-alloc`

## Reclaimed RAM heap

Rust-on-ESP explains that some RAM used during the boot process can later be reclaimed and used as heap via `#[ram(reclaimed)]`:
https://docs.espressif.com/projects/rust/book/application-development/alloc.html

This repo uses reclaimed RAM in the allocator declaration in `src/bin/main.rs`.

## Heap trade-offs (practical)

Rust-on-ESP highlights the main costs:

- **Fragmentation**: many small allocations can prevent later large allocations
- **Runtime overhead**: allocation/free costs CPU and metadata

Reference:
https://docs.espressif.com/projects/rust/book/application-development/alloc.html

## Guidelines for this repo

- Prefer stack for small fixed-size data.
- For large buffers (network frames, image buffers, etc.), prefer:
  - static buffers (if lifetime is effectively `'static`), or
  - a small number of long-lived heap allocations, re-used over time.
- Keep allocation patterns simple to reduce fragmentation.

