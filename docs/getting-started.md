# Getting Started with ESP32-S3 Rust

This guide covers basic patterns for getting started with ESP32-S3 development in Rust, based on proven examples and best practices.

## Table of contents

- [Project structure](#project-structure)
- [Hello World](#hello-world)
- [LED blink](#led-blink)
- [GPIO basics](#gpio-basics)
- [Button input](#button-input)
- [Next steps](#next-steps)

## Project structure

### Essential project files

Every ESP32 Rust project should have:

- **`Cargo.toml`**: Project dependencies and metadata
- **`rust-toolchain.toml`**: Rust toolchain version (for ESP32-S3: `xtensa-esp32s3-none-elf`)
- **`.cargo/config.toml`**: Build configuration (PSRAM mode, linker args, etc.)
- **`build.rs`**: Linker configuration and build-time setup
- **`src/bin/main.rs`**: Main application entry point

### Required attributes

Every `main.rs` should start with:

```rust
#![no_std]
#![no_main]
#![deny(
    clippy::mem_forget,
    reason = "mem::forget is generally not safe to do with esp_hal types, especially those \
    holding buffers for the duration of a data transfer."
)]
```

**Why:**
- `#![no_std]`: This is an embedded project, no standard library
- `#![no_main]`: We use `#[main]` attribute instead
- `#![deny(clippy::mem_forget)]`: Prevents unsafe memory management with HAL types

### Required imports

```rust
use esp_backtrace as _;  // Panic handler with backtrace
use esp_hal::main;        // Main function attribute
use esp_println::println; // Println macro
esp_bootloader_esp_idf::esp_app_desc!(); // Bootloader metadata
```

## Hello World

The simplest ESP32 program:

```rust
#![no_std]
#![no_main]

use esp_backtrace as _;
use esp_hal::{
    clock::CpuClock,
    delay::Delay, 
    main
};
use esp_println::println;
esp_bootloader_esp_idf::esp_app_desc!();

#[main]
fn main() -> ! {
    let config = esp_hal::Config::default().with_cpu_clock(CpuClock::max());
    let _peripherals = esp_hal::init(config);
    let delay = Delay::new();
    
    loop {
        println!("Hello World");
        delay.delay_millis(500);
    }
}
```

**Key points:**
- Initialize HAL with `esp_hal::init(config)`
- Use `CpuClock::max()` for maximum CPU performance
- Use `Delay::new()` for blocking delays
- `main()` returns `!` (never returns) - it runs forever

## LED blink

Blinking an LED is the "Hello World" of embedded systems:

```rust
#![no_std]
#![no_main]
#![deny(clippy::mem_forget)]

use esp_backtrace as _;
use esp_hal::{
    delay::Delay,
    gpio::{Level, Output, OutputConfig},
    main,
};
use esp_println::println;
esp_bootloader_esp_idf::esp_app_desc!();

#[main]
fn main() -> ! {
    let peripherals = esp_hal::init(esp_hal::Config::default());
    
    println!("Hello World");
    
    let mut led = Output::new(peripherals.GPIO2, Level::Low, OutputConfig::default());
    let delay = Delay::new();
    
    loop {
        led.set_high();
        println!("LED HIGH");
        delay.delay_millis(1000);
        
        led.set_low();
        println!("LED LOW");
        delay.delay_millis(1000);
    }
}
```

**Key points:**
- `Output::new()` creates a GPIO output pin
- `Level::Low` sets initial state (LED off)
- `set_high()` / `set_low()` control the pin state
- **Note:** GPIO pin numbers vary by board. Check your board's pinout!

### Finding the correct LED pin

- **ESP32-S3 DevKit**: Usually GPIO2 or GPIO8
- **Waveshare ESP32-S3 Touch AMOLED**: Check `docs/devices/waveshare-esp32-s3-touch-amoled-1.8.md`
- **Custom boards**: Check schematic or board documentation

## GPIO basics

### Output pins

```rust
use esp_hal::gpio::{Level, Output, OutputConfig};

// Create output pin (LED, relay, etc.)
let mut led = Output::new(
    peripherals.GPIO2,
    Level::Low,              // Initial state
    OutputConfig::default()  // Default config
);

// Control the pin
led.set_high();  // Set to HIGH (3.3V)
led.set_low();   // Set to LOW (0V)
led.toggle();    // Toggle state
```

### Input pins

```rust
use esp_hal::gpio::{Input, InputConfig, Pull};

// Create input pin (button, sensor, etc.)
let button = Input::new(
    peripherals.GPIO0,
    InputConfig::default()  // Uses internal pull-up by default
);

// Read pin state
if button.is_low() {
    // Pin is LOW (0V)
}
if button.is_high() {
    // Pin is HIGH (3.3V)
}
```

### Pull-up and pull-down resistors

Most buttons are **active-low** (LOW when pressed):

```rust
use esp_hal::gpio::{Input, InputConfig, Pull};

// Pull-up (most common for buttons)
let button = Input::new(
    peripherals.GPIO0,
    InputConfig::default().with_pull(Pull::Up)
);

// Pull-down (less common)
let button = Input::new(
    peripherals.GPIO0,
    InputConfig::default().with_pull(Pull::Down)
);

// No pull (external resistor)
let button = Input::new(
    peripherals.GPIO0,
    InputConfig::default().with_pull(Pull::None)
);
```

**When to use:**
- **Pull::Up**: Active-low buttons (button connects to GND when pressed)
- **Pull::Down**: Active-high buttons (button connects to VCC when pressed)
- **Pull::None**: External pull resistor or open-drain configuration

## Button input

Simple button press detection:

```rust
#![no_std]
#![no_main]
#![deny(clippy::mem_forget)]

use esp_backtrace as _;
use esp_hal::{
    delay::Delay,
    gpio::Level,
    gpio::{Input, InputConfig},
    gpio::{Output, OutputConfig},
    main
};
use esp_println::println;
esp_bootloader_esp_idf::esp_app_desc!();

#[main]
fn main() -> ! {
    let peripherals = esp_hal::init(esp_hal::Config::default());
    let mut led = Output::new(peripherals.GPIO2, Level::Low, OutputConfig::default());
    let button = Input::new(peripherals.GPIO0, InputConfig::default());
    let delay = Delay::new();

    loop {
        if button.is_low() {
            led.set_high();
            println!("Button Pressed");
            delay.delay_millis(50);  // Debounce delay
        } else {
            led.set_low();
        }
    }
}
```

**Key points:**
- Button is active-low (LOW when pressed)
- `delay.delay_millis(50)` provides simple debouncing
- LED turns on when button is pressed

**Note:** This is a simple pattern. For production code with long-press detection, sleep/wake patterns, and proper debouncing, see **[Button Handling Guide](./button-handling.md)**.

### Debouncing

Mechanical buttons "bounce" when pressed, causing multiple rapid state changes. Simple debouncing:

```rust
if button.is_low() {
    delay.delay_millis(50);  // Wait for bounce to settle
    if button.is_low() {
        // Button is actually pressed
        handle_button_press();
    }
}
```

For more robust debouncing (consecutive polls, edge detection), see the [Button Handling Guide](./button-handling.md).

## Next steps

Now that you have the basics, explore:

1. **[PWM and LEDC](./pwm-ledc.md)**: Fade LEDs, control servos, generate tones
2. **[Display and Graphics](./display-graphics.md)**: TFT displays, text rendering, images
3. **[Sensor Reading](./sensor-reading.md)**: I2C sensors, DHT22, BME280, etc.
4. **[Button Handling](./button-handling.md)**: Advanced button patterns, debouncing, sleep/wake
5. **[Async and Embassy](./async-embassy.md)**: Non-blocking code, tasks, timers
6. **[Wi-Fi and BLE](./wifi-ble.md)**: Network connectivity

## Common patterns

### Blocking delay

```rust
use esp_hal::delay::Delay;

let delay = Delay::new();
delay.delay_millis(1000);  // Wait 1 second
delay.delay_micros(500);   // Wait 500 microseconds
```

**Note:** In async code, use `Timer::after().await` instead. See [Async and Embassy](./async-embassy.md).

### CPU clock configuration

```rust
use esp_hal::clock::CpuClock;

// Maximum clock speed (recommended for most applications)
let config = esp_hal::Config::default().with_cpu_clock(CpuClock::max());

// Specific frequency (if needed)
let config = esp_hal::Config::default().with_cpu_clock(CpuClock::Max240MHz);
```

### Logging

```rust
use esp_println::println;

println!("Hello, world!");
println!("Value: {}", 42);
println!("Temperature: {} C", temp);
```

**Note:** For structured logging with levels, see [Logging Guide](./logging.md).

## Troubleshooting

### "LED doesn't blink"

- **Check pin number**: GPIO2 might not be the LED on your board
- **Check wiring**: LED might need a current-limiting resistor
- **Check polarity**: Some LEDs are directional
- **Check board docs**: See `docs/devices/` for board-specific pinouts

### "Button doesn't work"

- **Check pull configuration**: Most buttons need `Pull::Up`
- **Check wiring**: Button should connect GPIO to GND when pressed
- **Add debouncing**: Mechanical buttons need debouncing
- **Check pin number**: GPIO0 might not be a button on your board

### "Program doesn't compile"

- **Check target**: Ensure `xtensa-esp32s3-none-elf` is installed
- **Check dependencies**: All crates must support `no_std`
- **Check imports**: Use `esp_hal::` not `std::`
- **Clean build**: Try `cargo clean && cargo build`

### "Program panics on startup"

- **Check heap allocator**: If using heap, ensure allocator is initialized
- **Check stack size**: Large buffers on stack can cause overflow
- **Check panic handler**: Ensure `esp_backtrace` is included

## References

- [ESP-HAL GPIO Documentation](https://docs.rs/esp-hal/)
- [Rust-on-ESP Book - Getting Started](https://docs.espressif.com/projects/rust/book/getting-started/index.html)
- [Example Projects](https://github.com/esp-rs/esp-hal/tree/main/examples)
- [Comprehensive ESP32 Rust Examples](https://github.com/Vaishnav-Sabari-Girish/Embedded-Rust/tree/main/microcontrollers/esp32) - Hello world, LED blink, button press, and more
- Board-specific documentation: `docs/devices/`
