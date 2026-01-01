# Display and graphics programming

This document provides comprehensive guidance for working with TFT displays and graphics on ESP32 using Rust. It covers SPI display setup, text rendering, image display, and common patterns.

## Table of contents

- [Hardware setup](#hardware-setup)
- [Display driver selection](#display-driver-selection)
- [SPI configuration](#spi-configuration)
- [Display initialization](#display-initialization)
- [Text rendering](#text-rendering)
- [Image rendering](#image-rendering)
- [Input handling](#input-handling)
- [Common patterns](#common-patterns)
- [Gotchas and best practices](#gotchas-and-best-practices)
- [Complete examples](#complete-examples)

## Hardware setup

### Typical TFT display connections

Most TFT displays use SPI for communication and require several control pins:

- **SCK (Serial Clock)**: SPI clock pin
- **MOSI (Master Out Slave In)**: SPI data pin
- **CS (Chip Select)**: Selects the display on SPI bus
- **DC (Data/Command)**: Distinguishes data from commands
- **RST (Reset)**: Hardware reset pin
- **VCC**: Power (usually 3.3V or 5V)
- **GND**: Ground

### Pin assignment example (ESP32)

Common pin assignments (adjust for your board):

```rust
let sck = peripherals.GPIO18;   // SPI clock
let mosi = peripherals.GPIO23;  // SPI data
let cs = peripherals.GPIO15;    // Chip select
let dc = peripherals.GPIO2;     // Data/command
let reset = peripherals.GPIO4;  // Reset
```

**Note:** Pin numbers vary by board. Check your board's pinout diagram.

## Display driver selection

### ili9341 crate (older approach)

The `ili9341` crate provides direct support for ILI9341 displays:

**Pros:**
- Simple API
- Good for basic use cases
- Well-established

**Cons:**
- Limited to ILI9341 displays
- Less flexible

**Usage:**
```rust
use ili9341::{DisplaySize240x320, Ili9341, Orientation};
use display_interface_spi::SPIInterface;
```

### mipidsi crate (recommended)

The `mipidsi` crate supports multiple display controllers and is more flexible:

**Pros:**
- Supports multiple display controllers (ILI9341, ILI9342C, ST7789, etc.)
- More modern API
- Better performance
- Active development

**Cons:**
- Slightly more complex setup
- Requires buffer allocation

**Usage:**
```rust
use mipidsi::{
    Builder,
    models::ILI9341Rgb565,  // or ILI9342CRgb565, etc.
    options::{Orientation, Rotation},
    interface::SpiInterface,
};
```

**Recommendation:** Use `mipidsi` for new projects as it's more flexible and actively maintained.

## SPI configuration

### Basic SPI setup

```rust
use esp_hal::spi::master::{Spi, Config as SpiConfig};
use esp_hal::spi::Mode as SpiMode;
use esp_hal::time::Rate;
use embedded_hal_bus::spi::ExclusiveDevice;

// Create SPI peripheral
let spi = Spi::new(
    peripherals.SPI2,  // Use SPI2 or SPI3
    SpiConfig::default()
        .with_frequency(Rate::from_mhz(4))  // 4 MHz (adjust for your display)
        .with_mode(SpiMode::_0),            // SPI mode 0
)
.unwrap()
.with_sck(peripherals.GPIO18)   // Clock pin
.with_mosi(peripherals.GPIO23); // Data pin
```

### SPI frequency considerations

- **Lower frequencies (1-4 MHz)**: More reliable, works with longer wires
- **Higher frequencies (8-40 MHz)**: Faster updates, requires shorter wires
- **Start with 4 MHz**: Good balance of speed and reliability

### SPI mode

Most displays use **SPI Mode 0**:
- Clock idle low
- Data sampled on rising edge

Check your display datasheet if you encounter issues.

### Creating SPI device

```rust
use esp_hal::gpio::{Level, Output, OutputConfig};

let cs = Output::new(peripherals.GPIO15, Level::Low, OutputConfig::default());
let spi_dev = ExclusiveDevice::new_no_delay(spi, cs).unwrap();
```

**Note:** `ExclusiveDevice` ensures only one device uses the SPI bus at a time.

## Display initialization

### Using ili9341 crate

```rust
use display_interface_spi::SPIInterface;
use ili9341::{DisplaySize240x320, Ili9341, Orientation};
use esp_hal::delay::Delay;

let cs = Output::new(peripherals.GPIO15, Level::Low, OutputConfig::default());
let dc = Output::new(peripherals.GPIO2, Level::Low, OutputConfig::default());
let reset = Output::new(peripherals.GPIO4, Level::Low, OutputConfig::default());

let spi_dev = ExclusiveDevice::new_no_delay(spi, cs).unwrap();
let interface = SPIInterface::new(spi_dev, dc);

let mut display = Ili9341::new(
    interface,
    reset,
    &mut Delay::new(),
    Orientation::Landscape,
    DisplaySize240x320,
)
.unwrap();
```

### Using mipidsi crate (recommended)

```rust
use mipidsi::{
    Builder,
    models::ILI9341Rgb565,  // or ILI9342CRgb565 for ILI9342C
    options::{Orientation, Rotation},
    interface::SpiInterface,
};

let cs = Output::new(peripherals.GPIO5, Level::Low, OutputConfig::default());
let dc = Output::new(peripherals.GPIO2, Level::Low, OutputConfig::default());
let reset = Output::new(peripherals.GPIO4, Level::Low, OutputConfig::default());

// Buffer for SPI transfers (size depends on display)
let mut buffer = [0u8; 2048];  // 2048 bytes is common

let spi_dev = ExclusiveDevice::new_no_delay(spi, cs).unwrap();
let interface = SpiInterface::new(spi_dev, dc, &mut buffer);

let mut display = Builder::new(ILI9341Rgb565, interface)
    .reset_pin(reset)
    .init(&mut Delay::new())
    .unwrap();
```

### Buffer sizing for mipidsi

The buffer size affects performance:
- **512 bytes**: Minimum, slower updates
- **2048 bytes**: Good balance (recommended)
- **Larger buffers**: Faster but uses more RAM

Choose based on available RAM and performance requirements.

### Display orientation

```rust
// For mipidsi
display.set_orientation(
    Orientation::default().rotate(Rotation::Deg270)
).unwrap();

// For ili9341, orientation is set during initialization
let mut display = Ili9341::new(
    interface,
    reset,
    &mut Delay::new(),
    Orientation::Landscape,  // or Portrait
    DisplaySize240x320,
)
.unwrap();
```

## Text rendering

### Using embedded_graphics

The `embedded_graphics` crate provides text rendering capabilities:

```rust
use embedded_graphics::{
    mono_font::MonoTextStyle,
    prelude::*,
    text::{Baseline, Text},
};
use embedded_graphics::pixelcolor::Rgb565;
use profont::PROFONT_24_POINT;  // or other font sizes
```

### Basic text rendering

```rust
// Define text style
let text_style = MonoTextStyle::new(&PROFONT_24_POINT, Rgb565::GREEN);

// Render text
Text::with_baseline(
    "Hello ESP32",
    Point::new(60, 80),  // x, y position
    text_style,
    Baseline::Top,
)
.draw(&mut display)
.unwrap();
```

### Multiple text styles

```rust
// Large blue text
let large_style = MonoTextStyle::new(&PROFONT_24_POINT, Rgb565::BLUE);
Text::with_baseline(
    "Title",
    Point::new(50, 150),
    large_style,
    Baseline::Top,
)
.draw(&mut display)
.unwrap();

// Smaller red text
let small_style = MonoTextStyle::new(&PROFONT_18_POINT, Rgb565::RED);
Text::with_baseline(
    "Subtitle",
    Point::new(60, 180),
    small_style,
    Baseline::Top,
)
.draw(&mut display)
.unwrap();
```

### Available fonts (profont)

Common font sizes:
- `PROFONT_18_POINT`
- `PROFONT_24_POINT`
- `PROFONT_36_POINT`
- `PROFONT_48_POINT`

### Text positioning

- **Point coordinates**: `Point::new(x, y)` where (0, 0) is top-left
- **Baseline options**: `Baseline::Top`, `Baseline::Bottom`, `Baseline::Middle`
- **Color**: Use `Rgb565` color constants or create custom colors

### Clearing display before text

```rust
display.clear(Rgb565::BLACK).unwrap();  // Clear to black
// or
display.clear(Rgb565::WHITE).unwrap();  // Clear to white
```

## Image rendering

### Loading BMP images

```rust
use embedded_graphics::image::Image;
use embedded_graphics::pixelcolor::Rgb565;
use embedded_graphics::prelude::*;
use tinybmp::Bmp;

// Include image at compile time
let bmp_data = include_bytes!("../../image.bmp");
let bmp = Bmp::from_slice(bmp_data).unwrap();

// Create image and draw
let image = Image::new(&bmp, Point::new(10, 0));
image.draw(&mut display).unwrap();
```

### Image file preparation

1. **Format**: Use BMP format (24-bit or less)
2. **Size**: Keep images small to fit in flash
3. **Location**: Place in project root or `src/` directory
4. **Include**: Use `include_bytes!()` macro for compile-time embedding

### Image positioning

```rust
// Top-left corner
let image = Image::new(&bmp, Point::new(0, 0));

// Centered (example for 240x320 display)
let image = Image::new(&bmp, Point::new(10, 0));

// Custom position
let image = Image::new(&bmp, Point::new(x, y));
```

### Image size considerations

- **Memory**: Images are embedded in flash, not RAM
- **Flash usage**: Large images consume significant flash space
- **Performance**: Larger images take longer to render
- **Recommendation**: Optimize images before embedding

## Input handling

### Button input pattern

```rust
use esp_hal::gpio::{Input, InputConfig, Level};

let button = Input::new(peripherals.GPIO0, InputConfig::default());

loop {
    if button.is_low() {
        // Button pressed (assuming pull-up, active low)
        println!("Button Pressed");
        // Handle button press
        delay.delay_millis(50);  // Debounce delay
    }
}
```

### Debouncing

Always add debounce delay to prevent multiple triggers:

```rust
if button.is_low() {
    delay.delay_millis(50);  // 50ms debounce
    // Handle press
}
```

### Button configuration

```rust
// Pull-up configuration (button connects to GND when pressed)
let button = Input::new(
    peripherals.GPIO0,
    InputConfig::default()  // Uses internal pull-up
);

// For active-high buttons, you may need external pull-down
```

## Common patterns

### Complete display setup pattern

```rust
#![no_std]
#![no_main]
#![deny(clippy::mem_forget)]

use esp_hal::clock::CpuClock;
use esp_hal::main;
use embedded_graphics::prelude::*;
use embedded_graphics::pixelcolor::Rgb565;

#[main]
fn main() -> ! {
    let config = esp_hal::Config::default().with_cpu_clock(CpuClock::max());
    let peripherals = esp_hal::init(config);

    // 1. Setup SPI
    let spi = Spi::new(
        peripherals.SPI2,
        SpiConfig::default()
            .with_frequency(Rate::from_mhz(4))
            .with_mode(SpiMode::_0),
    )
    .unwrap()
    .with_sck(peripherals.GPIO18)
    .with_mosi(peripherals.GPIO23);

    // 2. Setup GPIO pins
    let cs = Output::new(peripherals.GPIO15, Level::Low, OutputConfig::default());
    let dc = Output::new(peripherals.GPIO2, Level::Low, OutputConfig::default());
    let reset = Output::new(peripherals.GPIO4, Level::Low, OutputConfig::default());

    // 3. Create SPI device and interface
    let mut buffer = [0u8; 2048];
    let spi_dev = ExclusiveDevice::new_no_delay(spi, cs).unwrap();
    let interface = SpiInterface::new(spi_dev, dc, &mut buffer);

    // 4. Initialize display
    let mut display = Builder::new(ILI9341Rgb565, interface)
        .reset_pin(reset)
        .init(&mut Delay::new())
        .unwrap();

    // 5. Clear and set orientation
    display.clear(Rgb565::BLACK).unwrap();
    display.set_orientation(
        Orientation::default().rotate(Rotation::Deg270)
    ).unwrap();

    // 6. Your drawing code here
    
    loop {
        // Main loop
    }
}
```

### Display update pattern

```rust
// Clear display
display.clear(Rgb565::BLACK).unwrap();

// Draw text
let text_style = MonoTextStyle::new(&PROFONT_24_POINT, Rgb565::WHITE);
Text::with_baseline(
    "Status: OK",
    Point::new(10, 10),
    text_style,
    Baseline::Top,
)
.draw(&mut display)
.unwrap();

// Draw image
let bmp_data = include_bytes!("../../image.bmp");
let bmp = Bmp::from_slice(bmp_data).unwrap();
let image = Image::new(&bmp, Point::new(10, 50));
image.draw(&mut display).unwrap();
```

### Button-controlled display update

```rust
let mut led = Output::new(peripherals.GPIO2, Level::Low, OutputConfig::default());
let button = Input::new(peripherals.GPIO0, InputConfig::default());
let delay = Delay::new();

loop {
    if button.is_low() {
        led.set_high();
        
        // Update display
        display.clear(Rgb565::BLACK).unwrap();
        // ... draw updated content
        
        delay.delay_millis(50);  // Debounce
    } else {
        led.set_low();
    }
}
```

## Gotchas and best practices

### 1. Deny mem_forget

**Always add this deny:**
```rust
#![deny(
    clippy::mem_forget,
    reason = "mem::forget is generally not safe to do with esp_hal types, especially those \
    holding buffers for the duration of a data transfer."
)]
```

**Why:** Display drivers hold SPI buffers during transfers. Forgetting them can cause undefined behavior.

### 2. Buffer lifetime

For `mipidsi`, the buffer must live as long as the display:

```rust
// Good: Buffer lives long enough
let mut buffer = [0u8; 2048];
let interface = SpiInterface::new(spi_dev, dc, &mut buffer);
let mut display = Builder::new(ILI9341Rgb565, interface).init(...).unwrap();
// buffer must not be dropped while display exists
```

### 3. SPI frequency tuning

- Start with 4 MHz
- Increase gradually if display supports it
- Watch for display artifacts (corruption, flickering)
- Reduce if you see issues

### 4. Display initialization order

1. Create SPI peripheral
2. Create GPIO pins (CS, DC, RST)
3. Create SPI device
4. Create interface
5. Initialize display
6. Clear display
7. Set orientation (if needed)

### 5. Color format

- Use `Rgb565` (16-bit color) for most displays
- Some displays support other formats (check driver docs)
- Color constants: `Rgb565::BLACK`, `Rgb565::WHITE`, `Rgb565::RED`, etc.

### 6. Memory usage

- **Flash**: Images embedded with `include_bytes!()` consume flash
- **RAM**: Display buffers consume RAM (especially with `mipidsi`)
- **Stack**: Keep large buffers off stack (use static or heap)

### 7. Error handling

Always handle `unwrap()` results in production code:

```rust
// Consider proper error handling
match display.clear(Rgb565::BLACK) {
    Ok(()) => {},
    Err(e) => {
        // Log error, handle gracefully
    }
}
```

### 8. Display refresh

- Clearing and redrawing is the standard update method
- Partial updates may be possible (check driver docs)
- Full screen updates are simpler but slower

### 9. Font selection

- Larger fonts use more flash space
- Use appropriate font size for readability
- Consider font licensing for commercial projects

### 10. Image optimization

- Convert images to appropriate size for display
- Use indexed color if possible
- Compress images before embedding
- Consider using image conversion tools

## Complete examples

### Example 1: Simple text display

```rust
#![no_std]
#![no_main]
#![deny(clippy::mem_forget)]

use esp_hal::clock::CpuClock;
use esp_hal::main;
use embedded_graphics::{
    mono_font::MonoTextStyle,
    pixelcolor::Rgb565,
    prelude::*,
    text::{Baseline, Text},
};
use profont::PROFONT_24_POINT;
use mipidsi::{
    Builder,
    models::ILI9341Rgb565,
    options::{Orientation, Rotation},
    interface::SpiInterface,
};
use embedded_hal_bus::spi::ExclusiveDevice;
use esp_hal::{
    delay::Delay,
    gpio::{Level, Output, OutputConfig},
    spi::master::{Spi, Config as SpiConfig},
    spi::Mode as SpiMode,
    time::Rate,
};

esp_bootloader_esp_idf::esp_app_desc!();

#[main]
fn main() -> ! {
    let config = esp_hal::Config::default().with_cpu_clock(CpuClock::max());
    let peripherals = esp_hal::init(config);

    // SPI setup
    let spi = Spi::new(
        peripherals.SPI2,
        SpiConfig::default()
            .with_frequency(Rate::from_mhz(4))
            .with_mode(SpiMode::_0),
    )
    .unwrap()
    .with_sck(peripherals.GPIO18)
    .with_mosi(peripherals.GPIO23);

    // GPIO setup
    let cs = Output::new(peripherals.GPIO5, Level::Low, OutputConfig::default());
    let dc = Output::new(peripherals.GPIO2, Level::Low, OutputConfig::default());
    let reset = Output::new(peripherals.GPIO4, Level::Low, OutputConfig::default());

    // Display initialization
    let mut buffer = [0u8; 2048];
    let spi_dev = ExclusiveDevice::new_no_delay(spi, cs).unwrap();
    let interface = SpiInterface::new(spi_dev, dc, &mut buffer);

    let mut display = Builder::new(ILI9341Rgb565, interface)
        .reset_pin(reset)
        .init(&mut Delay::new())
        .unwrap();

    display.clear(Rgb565::BLACK).unwrap();
    display.set_orientation(
        Orientation::default().rotate(Rotation::Deg270)
    ).unwrap();

    // Text rendering
    let text_style = MonoTextStyle::new(&PROFONT_24_POINT, Rgb565::GREEN);
    Text::with_baseline(
        "Hello ESP32",
        Point::new(60, 80),
        text_style,
        Baseline::Top,
    )
    .draw(&mut display)
    .unwrap();

    loop {
        // Main loop
    }
}
```

### Example 2: Image display

```rust
#![no_std]
#![no_main]
#![deny(clippy::mem_forget)]

use esp_hal::clock::CpuClock;
use esp_hal::main;
use embedded_graphics::{
    image::Image,
    pixelcolor::Rgb565,
    prelude::*,
};
use tinybmp::Bmp;
use mipidsi::{
    Builder,
    models::ILI9342CRgb565,
    options::{Orientation, Rotation},
    interface::SpiInterface,
};
// ... (SPI and GPIO setup same as above)

#[main]
fn main() -> ! {
    // ... (display initialization same as above)

    display.clear(Rgb565::BLACK).unwrap();
    display.set_orientation(
        Orientation::default().rotate(Rotation::Deg270)
    ).unwrap();

    // Image rendering
    let bmp_data = include_bytes!("../../image.bmp");
    let bmp = Bmp::from_slice(bmp_data).unwrap();
    let image = Image::new(&bmp, Point::new(10, 0));
    image.draw(&mut display).unwrap();

    loop {
        // Main loop
    }
}
```

### Example 3: Button-controlled display

```rust
#![no_std]
#![no_main]
#![deny(clippy::mem_forget)]

use esp_hal::{
    clock::CpuClock,
    delay::Delay,
    gpio::{Input, InputConfig, Level, Output, OutputConfig},
    main,
};
// ... (display setup)

#[main]
fn main() -> ! {
    // ... (display initialization)

    let mut led = Output::new(peripherals.GPIO2, Level::Low, OutputConfig::default());
    let button = Input::new(peripherals.GPIO0, InputConfig::default());
    let delay = Delay::new();

    loop {
        if button.is_low() {
            led.set_high();
            
            // Update display
            display.clear(Rgb565::BLACK).unwrap();
            let text_style = MonoTextStyle::new(&PROFONT_24_POINT, Rgb565::GREEN);
            Text::with_baseline(
                "Button Pressed!",
                Point::new(50, 100),
                text_style,
                Baseline::Top,
            )
            .draw(&mut display)
            .unwrap();
            
            delay.delay_millis(50);  // Debounce
        } else {
            led.set_low();
        }
    }
}
```

## Dependencies

Add these to your `Cargo.toml`:

```toml
[dependencies]
# Display drivers (choose one)
ili9341 = "0.4"  # For ili9341 displays
mipidsi = "0.9"  # Recommended, supports multiple displays

# Graphics
embedded-graphics = "0.8"
embedded-graphics-core = "0.3"

# Fonts
profont = "0.3"

# Image support
tinybmp = "0.4"

# SPI bus abstraction
embedded-hal-bus = "0.3"
display-interface-spi = "0.5"  # For ili9341

# HAL
esp-hal = { version = "~1.0", features = ["unstable"] }
```

## References

- [embedded-graphics documentation](https://docs.rs/embedded-graphics/)
- [mipidsi crate](https://crates.io/crates/mipidsi)
- [ili9341 crate](https://crates.io/crates/ili9341)
- [Example projects](https://github.com/Vaishnav-Sabari-Girish/Embedded-Rust/tree/main/microcontrollers/esp32)
- [esp-hal examples](https://github.com/esp-rs/esp-hal/tree/main/examples)
