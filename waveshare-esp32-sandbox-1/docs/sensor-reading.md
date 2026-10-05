# Sensor Reading on ESP32-S3

This guide covers reading various sensors on ESP32-S3, including I2C sensors (BME280), DHT22, ultrasonic sensors, and touch sensors.

## Table of contents

- [Overview](#overview)
- [I2C sensors](#i2c-sensors)
- [DHT22 temperature and humidity](#dht22-temperature-and-humidity)
- [Ultrasonic sensors](#ultrasonic-sensors)
- [Touch sensors](#touch-sensors)
- [Common patterns](#common-patterns)
- [Troubleshooting](#troubleshooting)

## Overview

ESP32-S3 can interface with many sensor types:

- **I2C sensors**: BME280, BMP280, MPU6050, etc. (digital, precise)
- **One-wire sensors**: DHT22, DS18B20 (simple, single pin)
- **Analog sensors**: Potentiometers, light sensors (ADC required)
- **Digital sensors**: Ultrasonic (HC-SR04), touch sensors (capacitive)

### Sensor selection guide

| Sensor Type | Interface | Pros | Cons | Use Cases |
|------------|-----------|------|------|-----------|
| I2C (BME280) | I2C bus | Precise, multiple sensors on one bus | Requires pull-ups, addressing | Weather stations, environmental monitoring |
| DHT22 | One-wire | Simple, temperature + humidity | Slower, less precise | Basic weather monitoring |
| Ultrasonic | Digital GPIO | Distance measurement | Limited range, angle-dependent | Obstacle detection, distance measurement |
| Touch | Capacitive | No moving parts | Calibration needed | User interfaces, proximity detection |

## I2C sensors

I2C (Inter-Integrated Circuit) is a common interface for digital sensors. Multiple sensors can share the same I2C bus.

### BME280 (temperature, pressure, humidity)

Complete example:

```rust
#![no_std]
#![no_main]
#![deny(clippy::mem_forget)]

use esp_backtrace as _;
use esp_hal::{
    i2c::master::{I2c, Config},
    clock::CpuClock,
    delay::Delay,
    main
};
use esp_println::println;
use bmpe280::bme280::BME280;

esp_bootloader_esp_idf::esp_app_desc!();

#[main]
fn main() -> ! {
    let config = esp_hal::Config::default().with_cpu_clock(CpuClock::max());
    let peripherals = esp_hal::init(config);
    let delay = Delay::new();

    // I2C pins (check your board!)
    let sda = peripherals.GPIO21;
    let scl = peripherals.GPIO22;

    // Initialize I2C
    let i2c = I2c::new(peripherals.I2C0, Config::default())
        .unwrap()
        .with_sda(sda)
        .with_scl(scl);

    // Initialize BME280 (default I2C address: 0x76)
    let mut bme = BME280::new(i2c, delay);

    loop {
        // Read sensor
        let measurement = bme.measure();

        println!(
            "Temperature: {} C, Pressure: {}, Humidity: {}%",
            measurement.temperature,
            measurement.pressure,
            measurement.humidity
        );

        delay.delay_millis(1000);
    }
}
```

**Key points:**
- I2C requires SDA (data) and SCL (clock) pins
- BME280 default address is `0x76` (can be `0x77` with address pin)
- `measure()` returns temperature (°C), pressure (Pa), and humidity (%)
- I2C bus can have multiple devices (different addresses)

### I2C configuration

```rust
use esp_hal::i2c::master::{I2c, Config};

// Default configuration (100 kHz)
let i2c = I2c::new(peripherals.I2C0, Config::default())
    .unwrap()
    .with_sda(sda)
    .with_scl(scl);

// Fast mode (400 kHz) - for faster sensors
let i2c = I2c::new(
    peripherals.I2C0,
    Config::default().with_baudrate(Rate::from_khz(400))
)
.unwrap()
.with_sda(sda)
.with_scl(scl);
```

**Note:** I2C requires pull-up resistors (typically 4.7kΩ). Many boards include these. If not, add external pull-ups to SDA and SCL.

### Multiple I2C sensors

```rust
// Initialize I2C bus
let i2c = I2c::new(peripherals.I2C0, Config::default())
    .unwrap()
    .with_sda(sda)
    .with_scl(scl);

// BME280 at address 0x76
let mut bme = BME280::new(i2c, delay);

// MPU6050 at address 0x68 (example)
// Note: You may need to share the I2C bus or use separate I2C peripherals
```

**Note:** Sharing I2C bus requires careful driver design. Some sensor crates consume the I2C bus. Check crate documentation.

### Common I2C sensor addresses

| Sensor | Default Address | Alternative Address |
|--------|----------------|---------------------|
| BME280 | 0x76 | 0x77 (with SDO high) |
| BMP280 | 0x76 | 0x77 |
| MPU6050 | 0x68 | - |
| TCA9554 | 0x20 | 0x21-0x27 (address pins) |

## DHT22 temperature and humidity

DHT22 uses a one-wire protocol (not I2C). It requires open-drain GPIO configuration.

### Complete example

```rust
#![no_std]
#![no_main]
#![deny(clippy::mem_forget)]

use esp_backtrace as _;
use esp_hal::{
    clock::CpuClock,
    delay::Delay,
    gpio::{DriveMode, Output, OutputConfig, Pull},
    main, 
};
use esp_println::println;
use embedded_dht_rs::dht22;

esp_bootloader_esp_idf::esp_app_desc!();

#[main]
fn main() -> ! {
    let config = esp_hal::Config::default().with_cpu_clock(CpuClock::max());
    let peripherals = esp_hal::init(config);

    // DHT22 requires open-drain configuration
    let od_config = OutputConfig::default()
        .with_drive_mode(DriveMode::OpenDrain)
        .with_pull(Pull::None);

    // Create open-drain pin for DHT22
    let od_for_dht22 = Output::new(
        peripherals.GPIO4,
        esp_hal::gpio::Level::High,
        od_config
    )
    .into_flex();  // Convert to flexible pin

    // Set as input (DHT22 protocol requires input mode)
    od_for_dht22.peripheral_input();

    let delay = Delay::new();
    let mut dht22 = dht22::Dht22::new(od_for_dht22, delay);

    loop {
        delay.delay_millis(2000);  // DHT22 needs 2s between readings

        match dht22.read() {
            Ok(sensor_reading) => println!(
                "DHT22 - Temperature: {} C, Humidity: {} %",
                sensor_reading.temperature,
                sensor_reading.humidity
            ),
            Err(error) => println!("Error reading DHT22: {:?}", error),
        }

        println!("_____________________________________________________");
    }
}
```

**Key points:**
- DHT22 requires **open-drain** GPIO configuration
- Pin must be set to **input mode** for reading
- Minimum 2 seconds between readings
- Returns `Result` - always handle errors

### DHT22 wiring

- **VCC**: 3.3V or 5V (check sensor spec)
- **GND**: Ground
- **DATA**: GPIO pin (with 4.7kΩ pull-up to VCC)

**Note:** Some DHT22 modules include the pull-up resistor. Check your module.

### DHT22 vs DHT11

- **DHT22**: More accurate, wider range, more expensive
- **DHT11**: Less accurate, narrower range, cheaper

Both use the same protocol and driver (`embedded_dht_rs` supports both).

## Ultrasonic sensors

HC-SR04 ultrasonic sensor measures distance using sound waves.

### Basic pattern

```rust
use esp_hal::gpio::{Input, InputConfig, Output, OutputConfig, Level};
use esp_hal::delay::Delay;

// Trigger pin (output)
let mut trigger = Output::new(peripherals.GPIO5, Level::Low, OutputConfig::default());

// Echo pin (input)
let echo = Input::new(peripherals.GPIO18, InputConfig::default());

let delay = Delay::new();

// Send trigger pulse
trigger.set_high();
delay.delay_micros(10);  // 10μs pulse
trigger.set_low();

// Wait for echo (measure pulse width)
// Distance = (pulse_width_us * speed_of_sound) / 2
// Speed of sound ≈ 343 m/s = 0.0343 cm/μs
// Distance (cm) = pulse_width_us * 0.0343 / 2
```

**Note:** HC-SR04 requires precise timing. Consider using hardware timers or async delays for accurate measurements.

## Touch sensors

ESP32-S3 has built-in capacitive touch sensors on specific GPIO pins.

### Basic pattern

```rust
use esp_hal::touch::TouchPad;

// Initialize touch pad (GPIO pins vary by chip)
let touch_pad = TouchPad::new(peripherals.TOUCH);

// Configure touch pad
let mut pad = touch_pad.channel(0);  // Channel 0
pad.set_threshold(100);  // Threshold value

// Read touch value
let value = pad.read();
if value < threshold {
    // Touch detected
}
```

**Note:** Touch sensor API varies by ESP32 variant. Check ESP-HAL documentation for your chip.

## Common patterns

### Periodic sensor reading

```rust
use embassy_time::{Duration, Timer};

#[embassy_executor::task]
async fn sensor_task() {
    loop {
        let measurement = bme.measure();
        println!("Temp: {} C", measurement.temperature);
        
        Timer::after(Duration::from_secs(5)).await;  // Read every 5 seconds
    }
}
```

### Error handling

```rust
match sensor.read() {
    Ok(reading) => {
        println!("Temperature: {} C", reading.temperature);
    }
    Err(e) => {
        println!("Sensor error: {:?}", e);
        // Retry, use last known value, or enter error state
    }
}
```

### Sensor calibration

```rust
// Read multiple samples and average
let mut sum = 0.0;
let samples = 10;

for _ in 0..samples {
    let reading = sensor.read().unwrap();
    sum += reading.temperature;
    delay.delay_millis(100);
}

let average = sum / samples as f32;
println!("Calibrated temperature: {} C", average);
```

### Multiple sensors

```rust
// Read multiple sensors in sequence
let temp = bme.measure().temperature;
let dht_reading = dht22.read()?;

println!("BME280: {} C", temp);
println!("DHT22: {} C", dht_reading.temperature);
```

## Troubleshooting

### "I2C sensor not detected"

- **Check wiring**: SDA and SCL connected correctly
- **Check pull-ups**: I2C requires 4.7kΩ pull-ups (many boards include these)
- **Check address**: Sensor might use different I2C address
- **Scan I2C bus**: Use I2C scanner to find device addresses
- **Check power**: Sensor needs proper power (3.3V or 5V)

### "DHT22 reading fails"

- **Check wiring**: DATA pin, VCC, GND
- **Check pull-up**: 4.7kΩ pull-up resistor on DATA pin
- **Check timing**: Minimum 2 seconds between readings
- **Check open-drain**: Must use open-drain GPIO configuration
- **Check input mode**: Pin must be in input mode for reading

### "Sensor readings are wrong"

- **Check calibration**: Some sensors need calibration
- **Check units**: Temperature in °C or °F? Pressure in Pa or hPa?
- **Check range**: Sensor might be outside measurement range
- **Check environment**: Temperature, humidity affect some sensors

### "I2C bus hangs"

- **Check pull-ups**: Missing or wrong value pull-ups cause bus hangs
- **Check wiring**: Short circuits or loose connections
- **Add timeouts**: Configure I2C timeouts to prevent hangs
- **Check clock stretching**: Some sensors use clock stretching

### "Multiple sensors conflict"

- **Check addresses**: Each I2C device needs unique address
- **Use separate buses**: Use I2C0 and I2C1 for different sensors
- **Check driver design**: Some sensor crates consume the I2C bus

## Dependencies

Add to `Cargo.toml`:

```toml
[dependencies]
# I2C sensors
bmpe280 = "0.2"  # BME280/BMP280
# embedded-mpu = "0.1"  # MPU6050 (example)

# DHT22
embedded-dht-rs = "0.1"

# HAL
esp-hal = { version = "~1.0", features = ["unstable"] }
esp-println = "0.7"
esp-backtrace = "0.9"
esp-bootloader-esp-idf = "0.1"
```

## References

- [ESP-HAL I2C Documentation](https://docs.rs/esp-hal/)
- [BME280 Datasheet](https://www.bosch-sensortec.com/products/environmental-sensors/humidity-sensors-bme280/)
- [DHT22 Datasheet](https://www.sparkfun.com/datasheets/Sensors/Temperature/DHT22.pdf)
- [Rust-on-ESP Book](https://docs.espressif.com/projects/rust/book/)
- [Example Projects](https://github.com/esp-rs/esp-hal/tree/main/examples)
- [ESP32 Sensor Examples](https://github.com/Vaishnav-Sabari-Girish/Embedded-Rust/tree/main/microcontrollers/esp32/sensor_reading) - BME280, DHT22, ultrasonic, and touch sensor examples
