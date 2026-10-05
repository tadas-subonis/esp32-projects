# `embedded-io` (0.7.1)

## What it does in this repo

Defines `no_std` I/O traits (read/write) commonly used by embedded networking and driver crates.

## Key links

- API docs (docs.rs): https://docs.rs/embedded-io/0.7.1/embedded_io/
- Crate page: https://crates.io/crates/embedded-io

## APIs you’ll likely use first

- Traits like `Read`, `Write`, and related error types (depends on what networking/peripheral layers you add next)

## How to use it

`embedded-io` is a `no_std` replacement for `std::io` traits, designed for embedded. It explicitly notes:

- `Error` is an associated type (no allocation required)
- `Read`/`Write` are always blocking; “readiness” is split into `ReadReady`/`WriteReady`

Crate root: https://docs.rs/embedded-io/0.7.1/embedded_io/

## Key traits and enums

- `embedded_io::ErrorType` (defines `type Error`)
- `embedded_io::Read`, `embedded_io::Write`
- `embedded_io::ReadReady`, `embedded_io::WriteReady`
- `embedded_io::ErrorKind`

## Code sketch: implement `Read`/`Write` for your driver wrapper

```rust
use core::convert::Infallible;
use embedded_io::{ErrorType, Read, Write};

struct MyUart;

impl ErrorType for MyUart {
    type Error = Infallible;
}

impl Read for MyUart {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, Self::Error> {
        // Fill `buf` with bytes from the hardware (blocking).
        // Return number of bytes read.
        Ok(0)
    }
}

impl Write for MyUart {
    fn write(&mut self, buf: &[u8]) -> Result<usize, Self::Error> {
        // Write some/all bytes (blocking).
        Ok(buf.len())
    }

    fn flush(&mut self) -> Result<(), Self::Error> {
        Ok(())
    }
}
```

## Complete API inventory

Full API index: https://docs.rs/embedded-io/0.7.1/embedded_io/all.html

### Traits
- **`BufRead`**: Buffered reader trait (read lines, etc.).
- **`Error`**: Error trait implemented by error types.
- **`ErrorType`**: Base trait defining the associated `Error` type.
- **`Read`**: Blocking read trait (always blocking).
- **`ReadReady`**: Readiness trait for non-blocking read patterns.
- **`Seek`**: Seek within streams.
- **`Write`**: Blocking write trait (always blocking).
- **`WriteReady`**: Readiness trait for non-blocking write patterns.

### Enums
- **`ErrorKind`**: Common error classification (NotFound, PermissionDenied, etc.).
- **`ReadExactError`**: Error from `Read::read_exact` (contains partial read count).
- **`SeekFrom`**: Seek origin for `Seek` (Start, End, Current).
- **`SliceWriteError`**: Error for writing into `&mut [u8]`.
- **`WriteFmtError`**: Error from `Write::write_fmt`.

