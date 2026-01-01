# `embedded-io-async` (0.7.0)

## What it does in this repo

Async equivalents of `embedded-io` traits, used by async driver/network stacks.

## Key links

- API docs (docs.rs): https://docs.rs/embedded-io-async/0.7.0/embedded_io_async/
- Crate page: https://crates.io/crates/embedded-io-async

## APIs you’ll likely use first

- async I/O traits used by higher-level crates (exact ones depend on what you build next)

## How to use it

This crate provides async equivalents of the `embedded-io` traits:

- Crate root: https://docs.rs/embedded-io-async/0.7.0/embedded_io_async/
- It defines `ErrorType`, `Read`, `Write`, `ReadReady`, `WriteReady`, etc (async versions).

In practice you’ll most often use it *indirectly*:

- `embassy-net` TCP sockets implement the `embedded-io-async` traits (crate docs mention this explicitly):
  - https://docs.rs/embassy-net/0.7.1/embassy_net/

## Code sketch: using an async `Read`/`Write` implementor

```rust
use embedded_io_async::{Read, Write};

async fn echo_once<RW: Read + Write>(io: &mut RW) {
    let mut buf = [0u8; 64];
    // let n = io.read(&mut buf).await.unwrap();
    // io.write_all(&buf[..n]).await.unwrap();
}
```

## API inventory

Full API index:
- https://docs.rs/embedded-io-async/0.7.0/embedded_io_async/all.html

- **Traits** (async equivalents of `embedded-io`)
  - **`ErrorType`**: base trait defining the associated `Error` type.
  - **`Error`**: error trait implemented by error types.
  - **`Read` / `Write`**: async read/write traits.
  - **`ReadReady` / `WriteReady`**: readiness traits.
  - **`BufRead`**: async buffered reader.
  - **`Seek`**: async seek.

- **Enums**
  - **`ErrorKind`**: common error classification.
  - **`ReadExactError`**: error from `Read::read_exact`.
  - **`SeekFrom`**: seek origin for `Seek`.

