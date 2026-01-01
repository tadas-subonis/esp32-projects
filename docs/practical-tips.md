# Practical development tips and patterns

This document consolidates practical tips, common patterns, gotchas, and best practices gathered from the ESP32 Rust ecosystem, community resources, and real-world development experience.

## Table of contents

- [Project setup](#project-setup)
- [Async and Embassy patterns](#async-and-embassy-patterns)
- [Memory management](#memory-management)
- [Driver usage patterns](#driver-usage-patterns)
- [OTA (Over-The-Air) updates](#ota-over-the-air-updates)
- [Common gotchas](#common-gotchas)
- [Performance tips](#performance-tips)
- [Debugging tips](#debugging-tips)

## Project setup

### Using esp-generate

When creating new projects, `esp-generate` is the recommended approach:

```bash
cargo install esp-generate --locked
esp-generate --chip esp32c3 your_project_name
cd your_project_name
```

**Tips:**
- Enable "Embassy framework support" during setup
- If Embassy option is unavailable, first enable "Enable unstable HAL features"
- The generated project includes proper `build.rs`, `.cargo/config.toml`, and project structure

### Toolchain management with espup

For multi-machine setups or CI/CD, use `espup`:

```bash
cargo install espup
espup install
```

**Benefits:**
- Automatically installs and configures required toolchains
- Consistent setup across machines
- Easier CI/CD integration

### Project structure best practices

- Keep `build.rs` - it wires up linker args and provides better error messages
- Use `.cargo/config.toml` for persistent configuration (env vars override it)
- After changing config, do a clean rebuild: `cargo clean && cargo build`

## Async and Embassy patterns

### Basic task spawning pattern

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

### Task pool sizing

For tasks that may be spawned multiple times, use `pool_size`:

```rust
#[embassy_executor::task(pool_size = 4)]
async fn handle_connection() {
    // Can spawn up to 4 concurrent instances
}
```

**When to use:**
- Network connection handlers
- Sensor reading tasks that may run in parallel
- Any task that needs multiple concurrent instances

### Main function requirements

The `main` function with `#[esp_rtos::main]` must:
- Accept a single parameter: `spawner: embassy_executor::Spawner`
- Be declared as `async`
- Not use generics
- Return `!` (never returns) or `()` depending on your setup

### Don't block in async tasks

**Bad:**
```rust
#[embassy_executor::task]
async fn bad_task() {
    loop {
        // Blocking delay - DON'T DO THIS
        blocking_delay(Duration::from_secs(1));
    }
}
```

**Good:**
```rust
#[embassy_executor::task]
async fn good_task() {
    loop {
        // Use async timer
        Timer::after(Duration::from_secs(1)).await;
    }
}
```

### Converting blocking drivers to async

Drivers start in blocking mode and can be converted:

```rust
// Blocking driver
let uart = Uart::new(peripherals.UART0, config);

// Convert to async
let uart = uart.into_async();
```

**Important:** Async drivers are **not `Send`** because interrupts are core-bound. On multicore chips, construct/convert on the correct core.

## Memory management

### Heap allocation patterns

**Prefer static buffers for large data:**
```rust
static mut BUFFER: [u8; 4096] = [0; 4096];
```

**Use heap for dynamic but long-lived allocations:**
```rust
extern crate alloc;
use alloc::vec::Vec;

// Allocate once, reuse
let mut buffer: Vec<u8> = Vec::with_capacity(1024);
```

**Avoid patterns that cause fragmentation:**
- Many small allocations/deallocations
- Allocating in tight loops
- Mixing short-lived and long-lived allocations

### Reclaimed RAM

This project uses reclaimed RAM for the heap (configured in `src/bin/main.rs`):

```rust
esp_alloc::heap_allocator!(#[esp_hal::ram(reclaimed)] size: 66320);
```

**Benefits:**
- Uses RAM that's only needed during boot
- Maximizes available heap for runtime
- Recommended approach for most applications

### Stack usage guidelines

- Keep buffers off the stack for large data (>100 bytes)
- This project denies `clippy::large_stack_frames` to enforce this
- Prefer static storage or heap for network frames, image buffers, etc.

### One global allocator rule

**Critical:** You can only have **one** global allocator. Don't introduce a second one.

## Driver usage patterns

### Proper driver construction

**Use `PeripheralRef` API** instead of `free(self) -> Self` methods:

```rust
// Good: Using PeripheralRef
let mut led = Output::new(peripherals.GPIO2, Level::High, OutputConfig::default());
```

### Driver destruction

Drivers should implement `Drop` to reset peripherals to idle state. **Never use `core::mem::forget`** on HAL types - it prevents proper cleanup.

Consider adding to your project:
```rust
#![deny(clippy::mem_forget)]
```

### Library development with esp-hal

When creating libraries that depend on `esp-hal`:

```toml
[dependencies]
esp-hal = { version = "~1.0", default-features = false }
```

**Why:**
- Prevents unintended feature activation
- Allows final application to select chip features
- Use `requires-unstable` feature for better error messages if unstable features are needed

## OTA (Over-The-Air) updates

### Partition table requirements

For OTA updates, your partition table must include:
- At least two `app` partitions (`ota_0` and `ota_1`)
- An `ota_data` partition for tracking which partition to boot

### Rust crates for OTA

**`esp-ota`** - Transport-agnostic OTA updates:
- Works with any transport mechanism (WiFi, Bluetooth, etc.)
- Requires partition table with `ota_0` and `ota_1`
- Provides safe Rust API for OTA operations

**`esp-hal-ota`** - For `no_std` environments with `esp-hal`:
- Dynamically reads partition tables
- Checks currently booted partition
- Performs CRC32 verification for data integrity

**`esp-bootloader-esp-idf`** - ESP-IDF second-stage bootloader support:
- Used in this project
- Enables OTA partition switching
- See `docs/ota.md` for details

### OTA implementation pattern

1. **Partition configuration**: Ensure partition table has OTA partitions
2. **Firmware transfer**: Implement transport (WiFi, Bluetooth, etc.)
3. **Write to inactive partition**: Use OTA crate to write new firmware
4. **Verification**: Verify integrity (CRC32 checks)
5. **Update boot partition**: Update OTA data partition to boot from new firmware
6. **Reboot**: Device boots from new partition on next restart

### OTA safety considerations

- Always verify firmware before switching partitions
- Keep old partition as fallback
- Implement rollback mechanism if verification fails
- Test OTA updates thoroughly before deployment

**Reference:** See `docs/ota.md` and Rust-on-ESP OTA chapter for detailed implementation.

## Common gotchas

### 1. Accidentally adding `std`

This repo is intentionally `#![no_std]`. Only introduce `std` if explicitly requested.

**Check:**
- Dependencies must support `no_std`
- Don't use `std::` imports
- Use `core::` or `alloc::` instead

### 2. Changing target without reason

Don't change `riscv32imc-unknown-none-elf` / `esp32c3` without explicit approval. Embedded regressions are costly.

### 3. Dependency upgrades

Dependency upgrades are opt-in. Don't bump versions unless asked - embedded regressions are costly.

### 4. Blocking in async contexts

Always use async timers (`Timer::after().await`) or async I/O, never blocking delays or busy loops.

### 5. Driver `Send` trait

Async drivers are not `Send` because interrupts are core-bound. On multicore chips:
- Construct blocking driver on source core
- Move blocking driver to destination core
- Convert to async on destination core

### 6. Memory fragmentation

Avoid patterns that cause fragmentation:
- Many small allocations
- Frequent allocate/deallocate cycles
- Mixing allocation lifetimes

### 7. Stack overflow

Large buffers on the stack can cause stack overflow. Use static storage or heap for:
- Network buffers (>100 bytes)
- Image buffers
- Large arrays
- String buffers

### 8. Configuration changes require clean rebuild

After changing `esp-config` settings:
```bash
cargo clean
cargo build --release
```

## Performance tips

### 1. Use release builds for deployment

Always use `--release` for final builds:
```bash
cargo build --release --target riscv32imc-unknown-none-elf
```

### 2. CPU clock configuration

Configure CPU clock appropriately:
```rust
let config = esp_hal::Config::default()
    .with_cpu_clock(CpuClock::max()); // Or specific frequency
```

### 3. Minimize logging in production

UART is slow. Keep logs lightweight:
- Use structured prefixes
- Log at appropriate levels (`RUST_LOG` env var)
- Consider disabling debug logs in release

### 4. Reuse buffers

Instead of allocating new buffers:
```rust
// Bad: Allocates every time
let mut buffer = Vec::new();

// Good: Reuse static buffer
static mut BUFFER: [u8; 1024] = [0; 1024];
```

### 5. Use async I/O

Always prefer async drivers over blocking ones for better concurrency.

## Debugging tips

### 1. Use Wokwi Simulator

Test code without hardware:
- https://wokwi.com/
- Supports Rust on ESP32
- Great for rapid prototyping

### 2. Serial monitor

Use `espflash` or `cargo-espflash` with `--monitor`:
```bash
cargo espflash flash --release --target riscv32imc-unknown-none-elf --chip esp32c3 --monitor
```

### 3. Log levels

Control logging with `RUST_LOG`:
```bash
RUST_LOG=debug cargo espflash flash --monitor
RUST_LOG=info cargo espflash flash --monitor
```

### 4. Generated documentation

Always check generated docs for exact API signatures:
```bash
cargo doc --target riscv32imc-unknown-none-elf --open
```

### 5. Hardware-in-loop testing

For hardware testing, use `embedded-test` + `probe-rs`:
- Use correct debug/USB port (USB-Serial-JTAG where available)
- See `docs/testing.md` for details

### 6. Common error patterns

**Linker errors:**
- Check `build.rs` is present
- Verify target is correct
- Clean and rebuild

**Panic on startup:**
- Check heap allocator is initialized
- Verify stack size is sufficient
- Check for stack overflow (large buffers on stack)

**Tasks not running:**
- Verify `spawner.spawn()` is called
- Check task is properly marked with `#[embassy_executor::task]`
- Ensure main function is `async` and uses `#[esp_rtos::main]`

## Code quality

### 1. Consistent API design

- Implement common Rust traits where applicable
- Use wrapper functions instead of direct logging macros
- Follow Rust naming conventions

### 2. Minimize unsafe code

- Limit scope of `unsafe` blocks
- Leverage safe abstractions from HAL
- Document why `unsafe` is necessary

### 3. Clippy lints

This project already denies:
- `clippy::large_stack_frames` - prevents stack overflow

Consider adding:
- `clippy::mem_forget` - prevents forgetting HAL types

## References

- [Rust-on-ESP Book - Async Options](https://docs.espressif.com/projects/rust/book/application-development/async.html)
- [Rust-on-ESP Book - Allocating Memory](https://docs.espressif.com/projects/rust/book/application-development/alloc.html)
- [Embassy Book](https://embassy.dev/book/)
- [esp-hal API Guidelines](https://docs.espressif.com/projects/rust/esp-hal/1.0.0-rc.1/esp32/esp_hal/index.html)
