# `static_cell` (2.1.1)

## What it does in this repo

`StaticCell` is a helper for building `'static` storage without putting large buffers on the stack (very useful in embedded async code).

This aligns with this repo’s guardrail of keeping stack usage small (`clippy::large_stack_frames` is denied).

## Key links

- API docs (docs.rs): https://docs.rs/static_cell/2.1.1/static_cell/
- Crate page: https://crates.io/crates/static_cell

## APIs you’ll likely use first

- `static_cell::StaticCell<T>`
- `StaticCell::init(value)` (initialize once, get `&'static mut T`)

## How to use it (typical embedded pattern)

`StaticCell` is ideal for “allocate once at startup, then hand out `'static` references” patterns, especially for buffers and `StackResources`-style values.

```rust
use static_cell::StaticCell;

static BUF: StaticCell<[u8; 1024]> = StaticCell::new();

fn init_buf() -> &'static mut [u8; 1024] {
    BUF.init([0u8; 1024])
}
```

## Complete API inventory

Full API index: https://docs.rs/static_cell/2.1.1/static_cell/all.html

### Structs
- **`ConstStaticCell<T>`**: Const-constructible `StaticCell` (can be initialized in `const` context).
- **`StaticCell<T>`**: Single-assignment storage that returns `&'static mut T` on initialization.

### Macros
- **`make_static!(value)`**: Macro to create a `'static` reference from a value (uses `StaticCell` internally).

