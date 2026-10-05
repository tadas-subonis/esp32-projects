# PWM and LEDC on ESP32-S3

This guide covers Pulse Width Modulation (PWM) using the LED Controller (LEDC) peripheral on ESP32-S3. LEDC is used for LED fading, servo control, tone generation, and other applications requiring variable duty cycle signals.

## Table of contents

- [Overview](#overview)
- [Basic LED fading](#basic-led-fading)
- [Configuration options](#configuration-options)
- [Multiple channels](#multiple-channels)
- [Common patterns](#common-patterns)
- [Troubleshooting](#troubleshooting)

## Overview

The ESP32-S3 LEDC peripheral provides:
- **Multiple channels**: Up to 8 channels (4 high-speed, 4 low-speed)
- **Variable frequency**: Configurable PWM frequency
- **Duty cycle control**: 0-100% duty cycle
- **Fade control**: Hardware-accelerated fading between duty cycles
- **Multiple timers**: Independent timers for different frequencies

### Use cases

- **LED brightness control**: Fade LEDs smoothly
- **Servo motor control**: Position servos (typically 50Hz PWM)
- **Tone generation**: Generate audio tones
- **Motor speed control**: Control DC motor speed
- **Power control**: Variable power output

## Basic LED fading

Complete example: Fade LED from 0% to 100% and back:

```rust
#![no_std]
#![no_main]
#![deny(clippy::mem_forget)]

use esp_hal::{
    clock::CpuClock,
    ledc::{
        channel,
        channel::ChannelIFace,
        timer::TimerIFace,
        timer,
        LSGlobalClkSource,
        Ledc,
        LowSpeed
    },
    time::Rate,
    main
};
use log::info;

#[panic_handler]
fn panic(_: &core::panic::PanicInfo) -> ! {
    loop {}
}

esp_bootloader_esp_idf::esp_app_desc!();

#[main]
fn main() -> ! {
    esp_println::logger::init_logger_from_env();

    let config = esp_hal::Config::default().with_cpu_clock(CpuClock::max());
    let peripherals = esp_hal::init(config);

    // GPIO pin for LED
    let led = peripherals.GPIO2;

    // Initialize LEDC
    let mut ledc = Ledc::new(peripherals.LEDC);
    ledc.set_global_slow_clock(LSGlobalClkSource::APBClk);

    // Configure timer
    let mut lstimer0 = ledc.timer::<LowSpeed>(timer::Number::Timer0);
    lstimer0
        .configure(timer::config::Config {
            duty: timer::config::Duty::Duty5Bit,  // 5-bit resolution (0-31)
            clock_source: timer::LSClockSource::APBClk,
            frequency: Rate::from_khz(24),  // 24 kHz PWM frequency
        })
        .unwrap();

    // Configure channel
    let mut channel0 = ledc.channel(channel::Number::Channel0, led);
    channel0
        .configure(channel::config::Config {
            timer: &lstimer0,
            duty_pct: 10,  // Start at 10% duty cycle
            pin_config: channel::config::PinConfig::PushPull,
        })
        .unwrap();

    loop {
        // Fade from 0% to 100% over 1 second
        channel0.start_duty_fade(0, 100, 1000).unwrap();
        while channel0.is_duty_fade_running() {}
        
        // Fade from 100% to 0% over 1 second
        channel0.start_duty_fade(100, 0, 1000).unwrap();
        while channel0.is_duty_fade_running() {}
    }
}
```

**Key points:**
- `Ledc::new()` initializes the LEDC peripheral
- `set_global_slow_clock()` sets the clock source for low-speed timers
- `timer::configure()` sets PWM frequency and resolution
- `channel::configure()` connects timer to GPIO pin
- `start_duty_fade()` starts hardware-accelerated fading
- `is_duty_fade_running()` checks if fade is complete

## Configuration options

### Duty cycle resolution

The duty cycle resolution determines the number of steps available:

```rust
timer::config::Duty::Duty5Bit   // 0-31 steps (32 levels)
timer::config::Duty::Duty6Bit   // 0-63 steps (64 levels)
timer::config::Duty::Duty7Bit   // 0-127 steps (128 levels)
timer::config::Duty::Duty8Bit   // 0-255 steps (256 levels)
timer::config::Duty::Duty9Bit   // 0-511 steps (512 levels)
timer::config::Duty::Duty10Bit  // 0-1023 steps (1024 levels)
timer::config::Duty::Duty11Bit  // 0-2047 steps (2048 levels)
timer::config::Duty::Duty12Bit  // 0-4095 steps (4096 levels)
timer::config::Duty::Duty13Bit  // 0-8191 steps (8192 levels)
timer::config::Duty::Duty14Bit  // 0-16383 steps (16384 levels)
```

**Trade-off:** Higher resolution = smoother fading but lower maximum frequency.

### PWM frequency

Choose frequency based on application:

```rust
// LED fading (smooth, not visible flicker)
frequency: Rate::from_khz(24)  // 24 kHz

// Servo control (standard 50 Hz)
frequency: Rate::from_hz(50)   // 50 Hz

// Audio tone generation
frequency: Rate::from_hz(1000) // 1 kHz

// Motor control
frequency: Rate::from_khz(1)   // 1 kHz
```

**Guidelines:**
- **LEDs**: 1-24 kHz (above visible flicker, ~100 Hz)
- **Servos**: 50 Hz (standard servo frequency)
- **Audio**: 20 Hz - 20 kHz (audible range)
- **Motors**: 1-20 kHz (depends on motor)

### Clock source

```rust
// Low-speed timer clock source
timer::LSClockSource::APBClk  // APB clock (default)
timer::LSClockSource::RTC8M   // RTC 8 MHz clock
timer::LSClockSource::XTAL     // Crystal oscillator

// Global slow clock source
ledc.set_global_slow_clock(LSGlobalClkSource::APBClk);
```

**Default:** `APBClk` works for most applications.

### Pin configuration

```rust
channel::config::PinConfig::PushPull  // Standard output (default)
channel::config::PinConfig::OpenDrain  // Open-drain output
```

**Default:** `PushPull` works for most applications.

## Multiple channels

You can use multiple channels with the same or different timers:

```rust
let mut ledc = Ledc::new(peripherals.LEDC);
ledc.set_global_slow_clock(LSGlobalClkSource::APBClk);

// Timer 0: 24 kHz for LED fading
let mut timer0 = ledc.timer::<LowSpeed>(timer::Number::Timer0);
timer0.configure(timer::config::Config {
    duty: timer::config::Duty::Duty8Bit,
    clock_source: timer::LSClockSource::APBClk,
    frequency: Rate::from_khz(24),
}).unwrap();

// Timer 1: 50 Hz for servo
let mut timer1 = ledc.timer::<LowSpeed>(timer::Number::Timer1);
timer1.configure(timer::config::Config {
    duty: timer::config::Duty::Duty10Bit,
    clock_source: timer::LSClockSource::APBClk,
    frequency: Rate::from_hz(50),
}).unwrap();

// Channel 0: LED on GPIO2 (uses timer 0)
let mut led_channel = ledc.channel(channel::Number::Channel0, peripherals.GPIO2);
led_channel.configure(channel::config::Config {
    timer: &timer0,
    duty_pct: 50,
    pin_config: channel::config::PinConfig::PushPull,
}).unwrap();

// Channel 1: Servo on GPIO3 (uses timer 1)
let mut servo_channel = ledc.channel(channel::Number::Channel1, peripherals.GPIO3);
servo_channel.configure(channel::config::Config {
    timer: &timer1,
    duty_pct: 7,  // 7% = ~1ms pulse (servo center)
    pin_config: channel::config::PinConfig::PushPull,
}).unwrap();

// Control independently
led_channel.start_duty_fade(0, 100, 1000).unwrap();
servo_channel.set_duty(10).unwrap();  // Set servo position
```

**Key points:**
- Each channel can use any timer
- Multiple channels can share the same timer (same frequency)
- Channels are independent (different duty cycles)

## Common patterns

### Simple on/off (no fading)

```rust
// Set duty cycle directly (no fade)
channel0.set_duty(50).unwrap();  // 50% duty cycle
channel0.set_duty(0).unwrap();   // 0% (off)
channel0.set_duty(100).unwrap(); // 100% (full on)
```

### Fade with callback

```rust
// Start fade
channel0.start_duty_fade(0, 100, 1000).unwrap();

// Wait for completion
while channel0.is_duty_fade_running() {
    // Do other work here
    // or just busy-wait
}

// Fade complete
println!("Fade complete");
```

### Servo control

```rust
// Configure for servo (50 Hz, 10-bit resolution)
let mut timer = ledc.timer::<LowSpeed>(timer::Number::Timer0);
timer.configure(timer::config::Config {
    duty: timer::config::Duty::Duty10Bit,
    clock_source: timer::LSClockSource::APBClk,
    frequency: Rate::from_hz(50),  // 50 Hz standard servo frequency
}).unwrap();

let mut servo = ledc.channel(channel::Number::Channel0, servo_pin);
servo.configure(channel::config::Config {
    timer: &timer,
    duty_pct: 7,  // Center position (~1.5ms pulse)
    pin_config: channel::config::PinConfig::PushPull,
}).unwrap();

// Servo positions (typical range: 5% to 10% duty cycle at 50 Hz)
// 5%  = ~1.0ms pulse (0 degrees)
// 7%  = ~1.5ms pulse (90 degrees, center)
// 10% = ~2.0ms pulse (180 degrees)

servo.set_duty(5).unwrap();   // 0 degrees
servo.set_duty(7).unwrap();    // 90 degrees
servo.set_duty(10).unwrap();  // 180 degrees
```

### Tone generation

```rust
// Configure for audio (higher frequency)
let mut timer = ledc.timer::<LowSpeed>(timer::Number::Timer0);
timer.configure(timer::config::Config {
    duty: timer::config::Duty::Duty8Bit,
    clock_source: timer::LSClockSource::APBClk,
    frequency: Rate::from_hz(1000),  // 1 kHz tone
}).unwrap();

let mut buzzer = ledc.channel(channel::Number::Channel0, buzzer_pin);
buzzer.configure(channel::config::Config {
    timer: &timer,
    duty_pct: 50,  // 50% duty cycle for square wave
    pin_config: channel::config::PinConfig::PushPull,
}).unwrap();

// Play tone
buzzer.set_duty(50).unwrap();  // Start tone
delay.delay_millis(500);       // Play for 500ms
buzzer.set_duty(0).unwrap();   // Stop tone
```

### Smooth brightness control

```rust
// Fade LED smoothly
fn set_brightness(channel: &mut impl ChannelIFace, brightness: u8) {
    channel.start_duty_fade(0, brightness as u32, 100).unwrap();
    while channel.is_duty_fade_running() {}
}

// Gradually increase brightness
for brightness in 0..=100 {
    set_brightness(&mut channel0, brightness);
    delay.delay_millis(10);
}
```

## Troubleshooting

### "LED doesn't fade smoothly"

- **Check frequency**: Too low frequency causes visible flicker (use 1+ kHz)
- **Check resolution**: Higher resolution = smoother steps
- **Check fade time**: Longer fade time = smoother transition

### "Servo doesn't move"

- **Check frequency**: Servos need 50 Hz (not kHz!)
- **Check duty cycle range**: Typical range is 5-10% at 50 Hz
- **Check wiring**: Servo needs power, ground, and signal
- **Check pulse width**: 1.0-2.0ms pulse width (5-10% at 50 Hz)

### "PWM frequency is wrong"

- **Check clock source**: Ensure `APBClk` is configured
- **Check resolution**: Higher resolution limits maximum frequency
- **Calculate frequency**: `frequency = clock_source / (2^resolution)`

### "Multiple channels interfere"

- **Use separate timers**: Each channel can use a different timer
- **Check timer configuration**: Ensure timers are configured correctly
- **Check channel assignment**: Each channel must use a timer

### "Fade doesn't complete"

- **Check `is_duty_fade_running()`**: Always wait for fade to complete
- **Check fade time**: Very short fade times may not be visible
- **Check duty range**: Ensure start and end duty are valid (0-100%)

## Dependencies

Add to `Cargo.toml`:

```toml
[dependencies]
esp-hal = { version = "~1.0", features = ["unstable"] }
log = "0.4"
esp-println = "0.7"
esp-backtrace = "0.9"
esp-bootloader-esp-idf = "0.1"
```

## References

- [ESP-HAL LEDC Documentation](https://docs.rs/esp-hal/)
- [ESP32-S3 Technical Reference Manual - LED PWM Controller](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [Rust-on-ESP Book](https://docs.espressif.com/projects/rust/book/)
- [Example Projects](https://github.com/esp-rs/esp-hal/tree/main/examples)
- [ESP32 PWM Example](https://github.com/Vaishnav-Sabari-Girish/Embedded-Rust/tree/main/microcontrollers/esp32/led_pwm) - Complete LED fading example
