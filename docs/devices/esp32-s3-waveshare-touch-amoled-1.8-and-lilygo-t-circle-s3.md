# Waveshare ESP32-S3-Touch-AMOLED-1.8 & LILYGO T-Circle-S3
## Hardware Reference for Rust Embedded Development

> **⚠️ Important**: This document describes **ESP32-S3 (Xtensa)** boards. This repository currently targets **ESP32-C3 (RISC-V)** with `esp-hal` + Embassy. Use this as a hardware reference when porting to ESP32-S3 or implementing similar features.

---

## Quick Reference (Most Used)

### I2C Address Map (Primary Bus: GPIO11/SDA, GPIO12/SCL)

| Device | Address | Function |
|--------|---------|----------|
| **FT3168** Touch | `0x38` | Capacitive touch (AMOLED board) |
| **CST816D** Touch | `0x15` | Capacitive touch (T-Circle-S3) |
| **QMI8658** IMU | `0x6B` | 6-axis accelerometer + gyroscope |
| **PCF85063** RTC | `0x51` | Real-time clock with backup battery |
| **ES8311** Audio | `0x18` | Audio codec (I2S + I2C control) |
| **AXP2101** PMIC | `0x34` | Power management IC |
| **TCA9554** GPIO Expander | `0x20` | 8-bit I/O expander |

### Pin Mapping Quick Reference

#### Waveshare ESP32-S3-Touch-AMOLED-1.8

| Function | GPIO Pins | Notes |
|----------|-----------|-------|
| **Display (SH8601 QSPI)** | GPIO6,7,11,13,14,38 | QSPI interface, ~330 KB frame buffer |
| **I2C Primary Bus** | GPIO11 (SDA), GPIO12 (SCL) | Shared by 6 devices (see I2C table) |
| **Touch (FT3168)** | GPIO11/12 (I2C) + INT/RST pins | I2C address: 0x38 |
| **IMU (QMI8658)** | GPIO11/12 (I2C) | Interrupt via TCA9554 EXIO6 |
| **RTC (PCF85063)** | GPIO11/12 (I2C) | Interrupt via TCA9554 EXIO5 |
| **Audio (ES8311)** | GPIO8,9,16,45,46 (I2S) + I2C | I2S + I2C control |
| **SD Card** | GPIO1,2,3 (SPI) + EXIO7 (CS) | CS via GPIO expander |
| **UART Debug** | GPIO43 (TX), GPIO44 (RX) | 115200 baud standard |
| **GPIO Expander** | GPIO11/12 (I2C) | Controls EXIO0-7 |
| **Power Button** | EXIO4 (via expander) | Read via TCA9554 |
| **Boot Button** | GPIO0 | Download mode / GPIO |

#### LILYGO T-Circle-S3

| Function | GPIO Pins | Notes |
|----------|-----------|-------|
| **Display (GC9D01N)** | Standard SPI pins | 160×160 circular, ~51 KB frame buffer |
| **Touch (CST816D)** | Standard I2C | I2C address: 0x15 |
| **Audio I2S** | Standard I2S pins | MAX98357A amp + MSM261 mic |
| **RGB LED** | GPIO (varies) | APA102 SPI LED |
| **Expansion** | 6× GPIO headers | 2× 4-pin headers + Qwiic I2C |

### Memory Requirements

| Component | Memory Needed |
|-----------|---------------|
| **AMOLED Frame Buffer** | ~330 KB (368×448×2 bytes) |
| **T-Circle Frame Buffer** | ~51 KB (160×160×2 bytes) |
| **Graphics Buffers** | ~500 KB (recommended) |
| **Audio Buffers** | ~64 KB |

---

## Device Specifications

### Waveshare ESP32-S3-Touch-AMOLED-1.8

**MCU:** ESP32-S3R8 (Xtensa LX7 Dual-Core @ 240 MHz)  
**Memory:** 512 KB SRAM + 8 MB PSRAM + 16 MB Flash  
**Display:** 1.8" AMOLED, 368×448, SH8601 QSPI driver  
**Touch:** FT3168 capacitive (I2C @ 0x38)  
**Sensors:** QMI8658 IMU (I2C @ 0x6B), PCF85063 RTC (I2C @ 0x51)  
**Audio:** ES8311 codec (I2S + I2C @ 0x18)  
**Power:** AXP2101 PMIC (I2C @ 0x34), 3.7V Li-ion battery  
**Expansion:** TCA9554 GPIO expander (I2C @ 0x20), SD card slot

### LILYGO T-Circle-S3

**MCU:** ESP32-S3R8 (Xtensa LX7 Dual-Core @ 240 MHz)  
**Memory:** 512 KB SRAM + 8 MB PSRAM + 16 MB Flash  
**Display:** 0.75" circular TFT, 160×160, GC9D01N SPI driver  
**Touch:** CST816D capacitive (I2C @ 0x15)  
**Audio:** MAX98357A amp + MSM261 mic (I2S)  
**Expansion:** 6× GPIO headers, Qwiic I2C connector, APA102 RGB LED

---

## Implementation Guides

### 1. Implementing I2C Bus (Primary)

**Hardware:** GPIO11 (SDA), GPIO12 (SCL)  
**Speed:** 100-400 kHz (configurable)  
**Devices:** 6 devices on shared bus (see I2C address table)

**With `esp-hal` (ESP32-S3):**

```rust
use esp_hal::i2c::{I2c, I2cConfig, ClockSpeed};
use esp_hal::gpio::{Io, InputOutput};

// Initialize GPIO
let io = Io::new(peripherals.GPIO, peripherals.IO_MUX);
let sda = InputOutput::new(io.pins.gpio11, esp_hal::gpio::OutputDrive::Standard);
let scl = InputOutput::new(io.pins.gpio12, esp_hal::gpio::OutputDrive::Standard);

// Create I2C driver
let config = I2cConfig::new()
    .clock_speed(ClockSpeed::KHz100);  // Start conservative

let mut i2c = I2c::new(
    peripherals.I2C0,
    sda,
    scl,
    config,
    &clocks,
);

// Scan bus (optional)
for addr in 0x08..=0x77 {
    if i2c.write(addr, &[], &mut []).is_ok() {
        info!("Device found at 0x{:02X}", addr);
    }
}
```

**Common I2C Operations:**

```rust
// Read from device
let mut buffer = [0u8; 6];
i2c.write_read(0x6B, &[0x00], &mut buffer)?;  // Read from QMI8658

// Write to device
i2c.write(0x51, &[0x03, 0x12])?;  // Write to RTC
```

---

### 2. Implementing Display (AMOLED - SH8601 QSPI)

**Hardware:** GPIO6,7,11,13,14 (QSPI data), GPIO38 (CS)  
**Interface:** QSPI (4-line data)  
**Frame Buffer:** ~330 KB (368×448×2 bytes RGB565)

**Key Specifications:**
- 16-bit RGB565 color depth
- Max QSPI clock: ~100 MHz (start at 40 MHz)
- Supports partial updates
- Hardware rotation support

**With `esp-hal` (ESP32-S3):**

```rust
use esp_hal::spi::{Spi, SpiConfig, SpiMode};
use esp_hal::gpio::{Io, Output, Level};

let io = Io::new(peripherals.GPIO, peripherals.IO_MUX);

// QSPI pins
let cs = Output::new(io.pins.gpio38, Level::High);
let sclk = io.pins.gpio7.into();
let d0 = io.pins.gpio6.into();
let d1 = io.pins.gpio11.into();
let d2 = io.pins.gpio13.into();
let d3 = io.pins.gpio14.into();

// SPI configuration for QSPI mode
let config = SpiConfig::new()
    .baudrate(40.MHz())  // Conservative start
    .mode(SpiMode::Mode0);

// Note: QSPI support in esp-hal may require specific peripheral selection
// Check esp-hal docs for QSPI/Quad SPI support on ESP32-S3
let mut spi = Spi::new_qspi(
    peripherals.SPI2,  // Check which SPI peripheral supports QSPI
    sclk,
    d0,
    Some(d1),
    Some(d2),
    Some(d3),
    cs,
    config,
    &clocks,
)?;

// Display initialization sequence (SH8601 specific)
// Send init commands via SPI
let init_cmds = [
    0x01, 0x00,  // Reset command
    // ... more init commands per SH8601 datasheet
];
spi.write(&init_cmds)?;
```

**Frame Buffer Management:**

```rust
// Allocate frame buffer in PSRAM if available
// For 368×448 RGB565: 368 * 448 * 2 = 329,984 bytes
let frame_buffer: &mut [u16] = // Allocate in PSRAM or static

// Update display (full screen)
spi.write_transaction(&frame_buffer)?;

// Partial update (more efficient)
// Set window coordinates, then send pixel data
```

**Performance Tips:**
- Use QSPI QUAD mode (4 data lines = 4× speed)
- Enable double buffering in PSRAM
- Limit refresh to 30 FPS for battery apps
- Use partial updates when possible

---

### 3. Implementing Display (T-Circle-S3 - GC9D01N SPI)

**Hardware:** Standard SPI pins (varies by board)  
**Interface:** Standard 4-wire SPI  
**Frame Buffer:** ~51 KB (160×160×2 bytes RGB565)

**Key Specifications:**
- 16-bit RGB565 color depth
- Max SPI clock: ~40 MHz
- Circular display (handled by IC)
- Lower power than AMOLED

**With `esp-hal` (ESP32-S3):**

```rust
use esp_hal::spi::{Spi, SpiConfig, SpiMode};
use esp_hal::gpio::{Io, Output, Level};

let io = Io::new(peripherals.GPIO, peripherals.IO_MUX);

// SPI pins (check board docs for exact pins)
let cs = Output::new(io.pins.gpioX, Level::High);  // Replace X
let sclk = io.pins.gpioY.into();  // Replace Y
let mosi = io.pins.gpioZ.into();  // Replace Z
let rst = Output::new(io.pins.gpioW, Level::High);  // Replace W

let config = SpiConfig::new()
    .baudrate(40.MHz())
    .mode(SpiMode::Mode0);

let mut spi = Spi::new(
    peripherals.SPI2,
    sclk,
    mosi,
    None,  // MISO not needed for display
    cs,
    config,
    &clocks,
)?;

// GC9D01N initialization
// Send init commands per GC9D01N datasheet
```

---

### 4. Implementing Touch Controller

#### FT3168 (AMOLED Board)

**I2C Address:** `0x38`  
**Bus:** Primary I2C (GPIO11/12)  
**Interrupt Pin:** Check schematic (may be via GPIO expander)

**With `esp-hal`:**

```rust
const FT3168_ADDR: u8 = 0x38;

// Read touch point
fn read_touch(i2c: &mut I2c) -> Result<(u16, u16, bool)> {
    let mut data = [0u8; 5];
    
    // Read touch status register (check FT3168 datasheet for exact register)
    i2c.write_read(FT3168_ADDR, &[0x02], &mut data)?;
    
    let touch_detected = (data[0] & 0x80) != 0;
    if !touch_detected {
        return Ok((0, 0, false));
    }
    
    // Parse coordinates (format depends on FT3168 configuration)
    let x = ((data[2] as u16) << 8) | (data[3] as u16);
    let y = ((data[4] as u16)) | ((data[1] as u16 & 0x0F) << 8);
    
    Ok((x, y, true))
}
```

#### CST816D (T-Circle-S3)

**I2C Address:** `0x15`  
**Bus:** Standard I2C

```rust
const CST816D_ADDR: u8 = 0x15;

// Read touch (similar pattern, check CST816D datasheet for register map)
fn read_touch(i2c: &mut I2c) -> Result<(u16, u16, bool)> {
    // Implementation per CST816D datasheet
    // ...
}
```

---

### 5. Implementing IMU (QMI8658)

**I2C Address:** `0x6B`  
**Bus:** Primary I2C (GPIO11/12)  
**Interrupt:** Via TCA9554 EXIO6

**Specifications:**
- 6-axis: 3-axis accelerometer + 3-axis gyroscope
- Accelerometer range: ±8G typical
- Gyroscope range: ±2000°/s typical
- Data format: 16-bit signed integers

**With `esp-hal`:**

```rust
const QMI8658_ADDR: u8 = 0x6B;

// Initialize IMU
fn init_imu(i2c: &mut I2c) -> Result<()> {
    // Write configuration registers per QMI8658 datasheet
    // Enable accelerometer and gyroscope
    i2c.write(QMI8658_ADDR, &[0x02, 0x60])?;  // Example: enable accel
    i2c.write(QMI8658_ADDR, &[0x03, 0x60])?;  // Example: enable gyro
    Ok(())
}

// Read accelerometer
fn read_accel(i2c: &mut I2c) -> Result<(i16, i16, i16)> {
    let mut buf = [0u8; 6];
    // Read from accelerometer data registers (check datasheet)
    i2c.write_read(QMI8658_ADDR, &[0x35], &mut buf)?;
    
    let x = i16::from_le_bytes([buf[0], buf[1]]);
    let y = i16::from_le_bytes([buf[2], buf[3]]);
    let z = i16::from_le_bytes([buf[4], buf[5]]);
    
    Ok((x, y, z))
}

// Read gyroscope
fn read_gyro(i2c: &mut I2c) -> Result<(i16, i16, i16)> {
    let mut buf = [0u8; 6];
    // Read from gyroscope data registers
    i2c.write_read(QMI8658_ADDR, &[0x3B], &mut buf)?;
    
    let x = i16::from_le_bytes([buf[0], buf[1]]);
    let y = i16::from_le_bytes([buf[2], buf[3]]);
    let z = i16::from_le_bytes([buf[4], buf[5]]);
    
    Ok((x, y, z))
}

// Convert to physical units
// For ±8G range: sensitivity typically 4096 LSB/G
fn accel_to_g(raw: i16) -> f32 {
    raw as f32 / 4096.0
}
```

---

### 6. Implementing RTC (PCF85063)

**I2C Address:** `0x51`  
**Bus:** Primary I2C (GPIO11/12)  
**Interrupt:** Via TCA9554 EXIO5

**Features:**
- Real-time clock with backup battery
- Alarm functions
- Temperature compensation
- BCD (Binary Coded Decimal) time format

**With `esp-hal`:**

```rust
const RTC_ADDR: u8 = 0x51;

// BCD conversion helpers
fn to_bcd(value: u8) -> u8 {
    ((value / 10) << 4) | (value % 10)
}

fn from_bcd(value: u8) -> u8 {
    ((value >> 4) * 10) + (value & 0x0F)
}

// Set time
fn set_time(i2c: &mut I2c, seconds: u8, minutes: u8, hours: u8, 
            day: u8, month: u8, year: u8) -> Result<()> {
    let data = [
        0x04,  // Start at seconds register
        to_bcd(seconds),
        to_bcd(minutes),
        to_bcd(hours),
        to_bcd(day),
        to_bcd(month),
        to_bcd(year % 100),  // Year as 2-digit
    ];
    i2c.write(RTC_ADDR, &data)
}

// Read time
fn read_time(i2c: &mut I2c) -> Result<(u8, u8, u8, u8, u8, u8)> {
    let mut data = [0u8; 7];
    i2c.write_read(RTC_ADDR, &[0x04], &mut data)?;
    
    Ok((
        from_bcd(data[0] & 0x7F),  // Seconds
        from_bcd(data[1] & 0x7F),  // Minutes
        from_bcd(data[2] & 0x3F),  // Hours
        from_bcd(data[3] & 0x3F),  // Day
        from_bcd(data[4] & 0x1F),  // Month
        from_bcd(data[5]),          // Year
    ))
}
```

---

### 7. Implementing GPIO Expander (TCA9554)

**I2C Address:** `0x20`  
**Bus:** Primary I2C (GPIO11/12)  
**Function:** 8-bit I/O expansion

**Pin Mapping:**
- EXIO0-3: General GPIO
- EXIO4: Power button logic
- EXIO5: RTC interrupt
- EXIO6: IMU interrupt
- EXIO7: SD card CS

**With `esp-hal`:**

```rust
const TCA9554_ADDR: u8 = 0x20;

// Register addresses (check TCA9554 datasheet)
const REG_INPUT: u8 = 0x00;
const REG_OUTPUT: u8 = 0x01;
const REG_POLARITY: u8 = 0x02;
const REG_CONFIG: u8 = 0x03;

// Configure pin as input or output
fn set_pin_direction(i2c: &mut I2c, pin: u8, is_output: bool) -> Result<()> {
    let mut config = [0u8];
    i2c.write_read(TCA9554_ADDR, &[REG_CONFIG], &mut config)?;
    
    if is_output {
        config[0] &= !(1 << pin);  // Clear bit = output
    } else {
        config[0] |= 1 << pin;     // Set bit = input
    }
    
    i2c.write(TCA9554_ADDR, &[REG_CONFIG, config[0]])
}

// Read input pin
fn read_pin(i2c: &mut I2c, pin: u8) -> Result<bool> {
    let mut data = [0u8];
    i2c.write_read(TCA9554_ADDR, &[REG_INPUT], &mut data)?;
    Ok((data[0] & (1 << pin)) != 0)
}

// Write output pin
fn write_pin(i2c: &mut I2c, pin: u8, high: bool) -> Result<()> {
    let mut output = [0u8];
    i2c.write_read(TCA9554_ADDR, &[REG_OUTPUT], &mut output)?;
    
    if high {
        output[0] |= 1 << pin;
    } else {
        output[0] &= !(1 << pin);
    }
    
    i2c.write(TCA9554_ADDR, &[REG_OUTPUT, output[0]])
}

// Read power button (EXIO4)
fn read_power_button(i2c: &mut I2c) -> Result<bool> {
    read_pin(i2c, 4)
}
```

---

### 8. Implementing Power Management (AXP2101)

**I2C Address:** `0x34`  
**Bus:** Primary I2C (GPIO11/12)

**Features:**
- Battery charging
- Multiple DC-DC converters
- Voltage/current monitoring via ADC
- Sleep mode control

**With `esp-hal`:**

```rust
const AXP2101_ADDR: u8 = 0x34;

// Read battery voltage (example register - check AXP2101 datasheet)
fn read_battery_voltage(i2c: &mut I2c) -> Result<f32> {
    let mut data = [0u8; 2];
    // Read from battery voltage ADC register (check datasheet)
    i2c.write_read(AXP2101_ADDR, &[0x26], &mut data)?;
    
    // Convert to voltage (formula per AXP2101 datasheet)
    let voltage_mv = ((data[0] as u16) << 4) | (data[1] as u16 & 0x0F);
    Ok(voltage_mv as f32 / 1000.0)  // Convert mV to V
}

// Set charging current
fn set_charge_current(i2c: &mut I2c, current_ma: u16) -> Result<()> {
    // Map current to register value (check datasheet)
    let reg_value = match current_ma {
        100 => 0x00,
        190 => 0x01,
        280 => 0x02,
        360 => 0x03,
        450 => 0x04,
        550 => 0x05,
        630 => 0x06,
        700 => 0x07,
        _ => return Err(/* invalid */),
    };
    
    i2c.write(AXP2101_ADDR, &[0x33, reg_value])  // Example register
}
```

---

### 9. Implementing Audio (ES8311)

**I2C Address:** `0x18` (control)  
**I2S Pins:** GPIO8 (SCLK), GPIO9 (LRCK), GPIO16 (DIN), GPIO45 (DOUT), GPIO46 (MCLK)  
**Bus:** Primary I2C for control

**Features:**
- Low-power audio codec
- Sampling rates: 8 kHz - 192 kHz
- Bit depth: up to 32-bit
- Microphone input + speaker output

**With `esp-hal`:**

```rust
const ES8311_I2C_ADDR: u8 = 0x18;

// Initialize codec via I2C
fn init_audio_codec(i2c: &mut I2c) -> Result<()> {
    // Write configuration registers per ES8311 datasheet
    // Enable ADC, DAC, set sample rate, etc.
    i2c.write(ES8311_I2C_ADDR, &[0x00, 0x80])?;  // Example: reset
    // ... more init commands
    Ok(())
}

// I2S configuration (separate from I2C control)
use esp_hal::i2s::{I2s, I2sConfig};

let i2s_config = I2sConfig::new()
    .sample_rate(16000.Hz())  // 16 kHz typical for voice
    .bits_per_sample(16)
    .channels(2);  // Stereo

let mut i2s = I2s::new(
    peripherals.I2S0,
    io.pins.gpio46,  // MCLK
    io.pins.gpio8,   // SCLK
    io.pins.gpio9,   // LRCK
    io.pins.gpio16,  // DIN (microphone)
    io.pins.gpio45,  // DOUT (speaker)
    i2s_config,
    &clocks,
)?;

// Write audio data
let audio_samples: &[i16] = &[/* PCM data */];
i2s.write(&audio_samples)?;
```

---

### 10. Implementing SD Card

**SPI Pins:** GPIO1 (MOSI), GPIO2 (SCLK), GPIO3 (MISO)  
**CS Pin:** EXIO7 (via TCA9554 GPIO expander)

**With `esp-hal`:**

```rust
use esp_hal::spi::{Spi, SpiConfig, SpiMode};

let io = Io::new(peripherals.GPIO, peripherals.IO_MUX);

// SPI pins
let mosi = io.pins.gpio1.into();
let sclk = io.pins.gpio2.into();
let miso = io.pins.gpio3.into();

// CS via GPIO expander (set EXIO7 as output, drive low for CS)
let mut expander = // ... TCA9554 instance
expander.set_pin_direction(7, true)?;  // EXIO7 as output

let config = SpiConfig::new()
    .baudrate(20.MHz())  // SD card typically 20-40 MHz
    .mode(SpiMode::Mode0);

let mut spi = Spi::new(
    peripherals.SPI2,
    sclk,
    mosi,
    Some(miso),
    // CS handled via GPIO expander
    config,
    &clocks,
)?;

// SD card initialization sequence
// 1. Set CS high, send 80+ clock cycles
// 2. Send CMD0 (GO_IDLE_STATE)
// 3. Send CMD8 (check voltage)
// 4. Send ACMD41 (initialize)
// ... per SD card specification
```

---

## Power Management

### Battery & Charging

**Battery:** 3.7V MX1.25 lithium ion  
**Charging:** USB Type-C 5V input, max 500mA  
**PMIC:** AXP2101 handles charging and power regulation

### Power Consumption Estimates

| Component | Current Draw |
|-----------|--------------|
| **AMOLED Display** | 50-150 mA |
| **TFT Display** | 10-50 mA |
| **CPU @ 240 MHz** | 80-150 mA |
| **CPU @ 80 MHz** | 30-50 mA |
| **WiFi Active** | 40-200 mA |
| **BLE Active** | 20-50 mA |
| **Sensors (I2C)** | 2-10 mA |
| **Total Active** | ~200-400 mA |

**Sleep Modes:**
- Light Sleep: ~10 mA
- Deep Sleep: ~0.1 mA
- Sensor wake-up possible

---

## Memory Layout

### Flash Partition (16 MB)

```
0x000000 - 0x007FFF    (32 KB)   Bootloader
0x008000 - 0x00FFFF    (32 KB)   Partition table
0x010000 - 0x1FFFFF    (1.95 MB) Application (OTA partition 1)
0x200000 - 0x3FFFFF    (2 MB)    Application (OTA partition 2)
0x400000 - 0xFFFFFF    (~12 MB)  File system (FATFS/SPIFFS)
```

### SRAM & PSRAM

**512 KB SRAM:**
- ISR stack, WiFi/BLE buffers: ~150 KB
- Heap: ~200 KB
- Remaining: free

**8 MB PSRAM:**
- Display frame buffer: 330 KB (AMOLED) / 51 KB (T-Circle)
- Graphics buffers: ~500 KB
- Audio buffers: ~64 KB
- Available: ~7 MB

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| **I2C communication fails** | Verify pull-ups (4.7kΩ typically onboard), check speed (start at 100 kHz) |
| **Display shows garbage** | Check QSPI/SPI clock frequency (start conservative: 40 MHz), verify init sequence |
| **Touch not responding** | Check I2C address (0x38 for FT3168, 0x15 for CST816D), verify interrupt pin |
| **IMU readings wrong** | Check register map, verify data format (endianness), check sensitivity settings |
| **RTC loses time** | Verify backup battery connection, check I2C communication |
| **Audio distortion** | Check sample rate configuration, verify I2S clock settings |
| **SD card not detected** | Verify CS pin (EXIO7 via expander), check SPI speed, verify card initialization sequence |
| **Power button not working** | Read via TCA9554 expander (EXIO4), not direct GPIO |

---

## Development Notes

### Code Examples Disclaimer

The code examples above use `esp-hal` patterns. When implementing:

1. **Check exact API**: `esp-hal` APIs may differ between ESP32-C3 and ESP32-S3
2. **Verify pin assignments**: Always cross-reference with board schematic
3. **Register maps**: Consult device datasheets for exact register addresses and formats
4. **QSPI support**: Verify QSPI peripheral availability and API on ESP32-S3
5. **I2S support**: Check I2S peripheral configuration options

### Recommended Development Flow

1. **Start with I2C bus**: Get basic I2C communication working first
2. **Test each device individually**: Verify each I2C device before combining
3. **Use conservative speeds**: Start with 100 kHz I2C, 40 MHz SPI/QSPI
4. **Enable logging**: Use `esp-println` to debug communication issues
5. **Check datasheets**: Each IC has specific initialization sequences

### Useful Resources

- **ESP-HAL Docs**: https://docs.rs/esp-hal/
- **ESP-HAL Examples**: https://github.com/esp-rs/esp-hal/tree/main/examples
- **Rust on ESP Book**: https://esp-rs.github.io/book/
- **Device Datasheets**: Check manufacturer websites for FT3168, QMI8658, PCF85063, ES8311, AXP2101, TCA9554, SH8601, GC9D01N

---

**Last Updated:** January 2026  
**Target Devices:** Waveshare ESP32-S3-Touch-AMOLED-1.8 & LILYGO T-Circle-S3  
**Architecture:** ESP32-S3 (Xtensa LX7)
