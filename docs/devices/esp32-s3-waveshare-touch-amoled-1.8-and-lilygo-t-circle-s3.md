# Waveshare ESP32-S3-Touch-AMOLED-1.8 & LILYGO T-Circle-S3
## Comprehensive Technical Specification (for Rust embedded development)

**Scope / status**

- This document is a **hardware reference** (pinout, buses, chips, addresses, peripherals) for:
  - **Waveshare ESP32-S3-Touch-AMOLED-1.8**
  - **LILYGO T-Circle-S3**
- This repository currently documents/builds an **ESP32-C3 `no_std` + Embassy + `esp-hal`** stack. Several code snippets below mention **ESP-IDF / `esp-idf-hal`** patterns; treat those as **illustrative/pseudo-code** unless the project is explicitly switched to an ESP-IDF (std) stack.
- When implementing anything hardware-specific in this repo, **prefer the pin/I²C address tables** and validate against the actual schematic/board revision.

---

## TABLE OF CONTENTS

1. Device Overview & Specifications
2. GPIO Pinout & Hardware Interfaces
3. Display Systems & Controllers
4. Sensor & Peripheral Architecture
5. Power Management System
6. Communication Interfaces
7. Rust Development Environment Setup
8. Code Configuration & Settings
9. Memory Layout & Storage
10. Development Resources & Tips

---

## 1. DEVICE OVERVIEW & SPECIFICATIONS

### 1.1 Waveshare ESP32-S3-Touch-AMOLED-1.8

**Processor:**
- **MCU:** ESP32-S3R8 (Xtensa LX7 Dual-Core @ up to 240 MHz)
- **Architecture:** Xtensa (requires ESP-specific Rust toolchain)
- **Cores:** 2 cores @ 240 MHz

**Memory:**
- **SRAM:** 512 KB (onboard) + 8 MB PSRAM
- **ROM:** 384 KB
- **Flash:** 16 MB (external SPI NOR)

**Wireless:**
- **WiFi:** 802.11 b/g/n @ 2.4 GHz
- **Bluetooth:** BLE 5.0 (onboard antenna)

**Display:**
- **Panel:** 1.8-inch AMOLED
- **Resolution:** 368 × 448 pixels
- **Colors:** 16.7M (8-bit RGB)
- **Driver IC:** SH8601 (QSPI interface)
- **Brightness:** 350 cd/㎡
- **Contrast Ratio:** 100,000:1
- **Response Time:** Fast (AMOLED advantage)

**Touch Controller:**
- **IC:** FT3168 capacitive touch
- **Interface:** I2C
- **Communication Speed:** 10 kHz - 400 kHz (configurable)

**Onboard Sensors & Peripherals:**
- **IMU:** QMI8658 6-axis (3-axis accel + 3-axis gyro) via I2C
- **RTC:** PCF85063 with backup battery support
- **Audio Codec:** ES8311 (I2S interface)
- **Microphone:** Built-in MEMS microphone
- **Speaker:** 3.7V MX1.25 lithium battery connector + amplifier
- **GPIO Expander:** TCA9554 (8-pin I/O expansion via I2C)
- **Power Manager:** AXP2101 (battery charging, power regulation)
- **Storage:** TF/SD card slot (SPI)

**Physical:**
- **Compact form factor** optimized for wearables
- **Side buttons:** PWR and BOOT buttons
- **Exposed GPIOs:** 7× GPIO pads, 1× I2C, 1× UART, 1× USB

---

### 1.2 LILYGO T-Circle-S3

**Processor:**
- **MCU:** ESP32-S3R8 (identical to AMOLED variant)
- **Cores:** 2 @ 240 MHz Xtensa LX7

**Memory:**
- **SRAM:** 512 KB
- **PSRAM:** 8 MB
- **ROM:** 384 KB
- **Flash:** 16 MB

**Display:**
- **Panel:** 0.75-inch circular TFT LCD
- **Resolution:** 160 × 160 pixels (circular)
- **Colors:** 262K (16-bit RGB)
- **Driver IC:** GC9D01N (SPI interface)
- **Bus:** Standard SPI

**Touch Controller:**
- **IC:** CST816D capacitive touch
- **Interface:** I2C

**Audio:**
- **Amplifier:** Maxim MAX98357A Class-D (I²S, 3W output)
- **Microphone:** Goertek MSM261 I²S MEMS microphone

**Expansion:**
- **GPIO Headers:** 6× GPIOs on back (2× 4-pin female headers)
- **Qwiic:** I²C connector
- **LED:** APA102 RGB LED

**Power & Physical:**
- **USB:** Type-C (5V 500mA charging)
- **Reset Button:** Yes
- **Size:** 32 mm (base) × 28 mm (top) × 17 mm height

---

## 2. GPIO PINOUT & HARDWARE INTERFACES

### 2.1 ESP32-S3-Touch-AMOLED-1.8 - Pin Assignments

#### Display Interface (QSPI - Primary)

```
Display (SH8601 QSPI):
├─ LCD_CS      → GPIO chip select
├─ LCD_SCLK    → SPI clock
├─ LCD_SDIO0   → Serial data I/O 0
├─ LCD_SDIO1   → Serial data I/O 1
├─ LCD_SDIO2   → Serial data I/O 2
├─ LCD_SDIO3   → Serial data I/O 3
├─ LCD_RESET   → Display reset
└─ LCD_TE      → Tearing effect signal

Recommended GPIO mapping (from schematic):
GPIO38  → LCD_CS
GPIO7   → LCD_SCLK (part of QSPI)
GPIO6,11,13,14 → QSPI data lines
```

#### Touch Interface (I2C)

```
Touch (FT3168):
├─ TP_SDA      → I2C Data (GPIO11 or SDA)
├─ TP_SCL      → I2C Clock (GPIO12 or SCL)
├─ TP_INT      → Touch interrupt pin
├─ TP_RESET    → Touch controller reset
└─ I2C Address: 0x38 (standard FT3168)
```

#### Sensor Interface (I2C - Same Bus)

```
Shared I2C Bus (Primary):
├─ SDA (GPIO11)  ┐
└─ SCL (GPIO12)  ├─ Connects to:
                 │  • FT3168 Touch (0x38)
                 │  • QMI8658 IMU (0x6B)
                 │  • PCF85063 RTC
                 │  • ES8311 Audio Codec
                 │  • AXP2101 Power Manager
                 │  • TCA9554 GPIO Expander
                 └─ Max devices: 6
```

#### UART Interface

```
Serial Debug UART:
├─ GPIO43 (U0TXD) → Serial TX
├─ GPIO44 (U0RXD) → Serial RX
└─ Baud: 115200 (standard)
```

#### SD/TF Card Interface (SPI)

```
SD Card (SPI Mode):
├─ GPIO1  (GPIO1)  → MOSI (DI)
├─ GPIO2  (GPIO2)  → SCLK
├─ GPIO3  (GPIO3)  → MISO (DO)
├─ EXIO7           → CS (chip select)
└─ Voltage: 3.3V
```

#### Audio Codec (I2S)

```
I2S Audio (ES8311):
├─ GPIO8   → I2S_SCLK (serial clock)
├─ GPIO9   → I2S_LRCK (left/right clock)
├─ GPIO16  → I2S_DSDIN (data in)
├─ GPIO45  → I2S_ASDOUT (data out)
├─ GPIO46  → I2S_MCLK (master clock)
└─ I2C Control on main I2C bus (SDA/SCL)
```

#### GPIO Expansion (TCA9554 via I2C)

```
Expander Outputs:
├─ EXIO0 → GPIO expander pin 0
├─ EXIO1 → GPIO expander pin 1
├─ EXIO2 → GPIO expander pin 2
├─ EXIO3 → GPIO expander pin 3
├─ EXIO4 → POWER button logic
├─ EXIO5 → RTC interrupt
├─ EXIO6 → IMU interrupt
└─ EXIO7 → SD card CS (as above)
```

#### Power Button & Boot

```
Physical Buttons:
├─ PWR (EXIO4)  → Power management
├─ BOOT (GPIO0) → Download mode / GPIO
└─ Note: Requires I2C expander read for PWR state
```

#### External Pin Headers

```
Reserved GPIO Pads (100mil pitch):
├─ GPIO17, GPIO18
├─ GPIO21
├─ GPIO40, GPIO39
├─ GPIO45, GPIO46
├─ GPIO42, GPIO41
├─ Voltage: 3.3V (pull-ups/downs configured)
└─ Available for custom peripherals
```

---

### 2.2 LILYGO T-Circle-S3 - Pin Assignments

#### Display Interface (SPI)

```
Display (GC9D01N SPI):
├─ CS   → Chip select
├─ CLK  → SPI clock
├─ MOSI → Serial data out
├─ MISO → Serial data in
└─ RST  → Display reset

Note: Pinout varies - check board documentation
Typical Arduino_GFX configuration available
```

#### Touch Interface (I2C)

```
Touch (CST816D):
├─ SDA → I2C Data
├─ SCL → I2C Clock
├─ INT → Touch interrupt
└─ I2C Address: 0x15
```

#### Audio System (I2S + I2C)

```
Microphone (I2S):
├─ SCL → I2S serial clock
├─ LRC → I2S left/right clock
├─ DATA → I2S data in

Speaker Amplifier (I2S):
├─ SCL → I2S serial clock
├─ LRC → I2S left/right clock
├─ DATA → I2S data out

Both share I2C control bus
```

#### Expansion Pins (6× GPIO)

```
Rear Headers:
├─ Header 1 (4-pin): 4 GPIO connections
├─ Header 2 (4-pin): 4 GPIO connections + GND/3.3V
└─ Qwiic I2C connector on side
```

---

## 3. DISPLAY SYSTEMS & CONTROLLERS

### 3.1 ESP32-S3-Touch-AMOLED-1.8 (SH8601 QSPI)

**SH8601 Driver Specifications:**

```
Interface: QSPI (Quad SPI - 4-line data)
Data Width: 16-bit RGB565 per pixel
Max Speed: ~100 MHz QSPI clock
Memory Required: ~330 KB for full frame buffer (368×448×2 bytes)
```

**Rust HAL Initialization Pattern:**

```rust
// Enable QSPI peripheral
let qspi = io.pins.gpio6.into();  // Data 0
let qspi1 = io.pins.gpio11.into(); // Data 1
let qspi2 = io.pins.gpio13.into(); // Data 2
let qspi3 = io.pins.gpio14.into(); // Data 3
let sclk = io.pins.gpio7.into();   // Clock
let cs = io.pins.gpio38.into();    // Chip select

// Configuration
let qspi_config = QspiConfig::default()
    .frequency(40.MHz())  // Conservative start
    .max_transfer_size(MaxTransferSize::Bytes32);
```

**Key Controller Features:**
- Supports 16-bit RGB565 color depth
- MIPI DSI interface alternative (not used in QSPI mode)
- Built-in gamma correction
- Hardware support for rotation
- Partial display updates

---

### 3.2 LILYGO T-Circle-S3 (GC9D01N SPI)

**GC9D01N Driver Specifications:**

```
Interface: Standard 4-wire SPI
Data Width: 16-bit (RGB565)
Max Speed: ~40 MHz SPI clock
Memory Required: ~51 KB for full frame buffer (160×160×2 bytes)
Built-in: Circular display optimization

Rust Configuration:
- Use Arduino_GFX library or direct SPI control
- CST816D touch controller on separate I2C bus
```

**Key Advantages:**
- Circular display automatically handled by IC
- Simpler SPI interface vs QSPI
- Excellent for wearable interfaces
- Lower power consumption than AMOLED

---

## 4. SENSOR & PERIPHERAL ARCHITECTURE

### 4.1 IMU Sensor (QMI8658)

**Specifications:**

```
Type: 6-axis IMU (3-axis accelerometer + 3-axis gyroscope)
I2C Address: 0x6B (standard configuration)
I2C Bus: Shared primary I2C (GPIO11/SDA, GPIO12/SCL)
Interrupt Pin: GPIO10 (through TCA9554 as EXIO6)
```

**Rust Configuration:**

```rust
// I2C address and initialization
const QMI8658_ADDRESS: u8 = 0x6B;

// Create I2C driver
let i2c = I2cDriver::new(
    peripherals.i2c0,
    io.pins.gpio11,  // SDA
    io.pins.gpio12,  // SCL
    &Default::default(),
)?;

// Accelerometer range: ±8G typical
// Gyroscope range: ±2000°/s typical
```

**Data Output Format:**
- Acceleration: X, Y, Z (3 × 16-bit signed)
- Gyroscope: X, Y, Z (3 × 16-bit signed)
- Data ready interrupt available

---

### 4.2 RTC Module (PCF85063)

**Specifications:**

```
Type: Real-time clock with backup battery
I2C Address: 0x51 (standard)
Bus: Primary I2C
Backup Power: 3.7V MX1.25 lithium battery connector
Features:
  - Alarm functions
  - Interrupt output
  - Temperature compensation
```

**Rust Usage:**

```rust
const RTC_ADDRESS: u8 = 0x51;
// Time stored as BCD (Binary Coded Decimal)
// Registers: Seconds, Minutes, Hours, Day, Month, Year
```

---

### 4.3 Audio Codec (ES8311)

**Specifications:**

```
Type: Low-power audio codec
Interface: I2S + I2C control
I2S Pins: GPIO8 (SCLK), GPIO9 (LRCK), GPIO16/45 (data)
I2C Control Address: 0x18
Sampling Rates: 8 kHz - 192 kHz
Bit Depth: Up to 32-bit
Microphone Input: Direct connection
Speaker Output: Through MAX98357A amplifier
```

**Rust I2S Configuration:**

```rust
const ES8311_I2C_ADDR: u8 = 0x18;

// I2S configuration
let i2s_config = i2s::I2sConfig::default()
    .sample_rate(16000.Hz())  // Typical voice
    .bits_per_sample(i2s::BitsPerSample::Bits16)
    .tx_frame_sync()
    .rx_frame_sync();
```

---

### 4.4 Power Management (AXP2101)

**Specifications:**

```
Type: Integrated Power Management Unit (PMIC)
I2C Address: 0x34
Bus: Primary I2C
Features:
  - Multiple DC-DC converters
  - LDO outputs
  - Battery charging circuit
  - Power path management
  - ADC for voltage/current monitoring
  - Sleep mode control

Output Rails:
├─ DCDC1: Fixed 3.3V (system power)
├─ DCDC2: 0.9V (core)
├─ DCDC3: 1.2V (SRAM/ROM)
├─ DCDC4: 1.8V (peripherals)
├─ ALDO1-4: 3.3V analog rails
├─ BLDO1-2: 2.8V backup power
└─ RTCLDO: RTC supply
```

---

### 4.5 GPIO Expander (TCA9554)

**Specifications:**

```
Type: 8-bit I/O expander
I2C Address: 0x20 (configurable via pins)
Bus: Primary I2C
Features:
  - 8 independent I/O pins
  - Programmable polarity
  - Input/output direction per pin
  - Interrupt support
  - 100 kHz - 400 kHz I2C speed

Pin Mapping (EXIO0-7):
├─ EXIO0-3: General GPIO
├─ EXIO4: Power button logic
├─ EXIO5: RTC interrupt signal
├─ EXIO6: IMU interrupt signal
└─ EXIO7: SD card chip select
```

**Rust Initialization:**

```rust
const TCA9554_ADDRESS: u8 = 0x20;
// Register: 0x03 = Output port register
// Register: 0x01 = Input port register
// Register: 0x00 = Input/Output selection
```

---

## 5. POWER MANAGEMENT SYSTEM

### 5.1 Battery & Charging

**Specifications:**

```
Battery Type: 3.7V MX1.25 lithium ion
Charging Method: USB Type-C 5V input
Charging Circuit: AXP2101 PMIC
Max Charge Current: 500mA (USB Type-C limited)
```

**Rust Power API Pattern:**

```rust
// Read battery voltage
let battery_voltage = read_axp_adc(0x26);  // Battery ADC

// Set charging current limit
let charge_config = 0x80;  // 500mA typical

// Enable sleep modes for battery conservation
peripherals.RTC.CNTL.modify(|_, w| w.sleep_en().set_bit());
```

---

### 5.2 Power Domains & Isolation

**Active Power Draw:**
- Display: 50-150 mA (AMOLED) / 10-50 mA (LCD)
- CPU @ 240 MHz: 80-150 mA
- Wireless: 40-200 mA (WiFi/BLE)
- Sensors: 2-10 mA combined
- Total active: ~200-400 mA

**Sleep Modes:**
- Light Sleep: ~10 mA
- Deep Sleep: ~0.1 mA
- Sensor Wake-up possible

---

## 6. COMMUNICATION INTERFACES

### 6.1 WiFi (802.11 b/g/n)

**Specifications:**

```
Frequency: 2.4 GHz
Standards: 802.11 b (1 Mbps), g (11 Mbps), n (65 Mbps)
Antenna: Onboard SMD antenna
Typical Range: 50-100m (open space)
Current Draw: 40-200 mA depending on state
```

### 6.2 Bluetooth Low Energy (BLE 5.0)

**Specifications:**

```
Version: BLE 5.0
Frequency: 2.4 GHz (same as WiFi)
TX Power: +3 dBm to +20 dBm (software configurable)
Range: 50-100m (line of sight)
Profiles: GAP, GATT, HFP (if enabled)
```

### 6.3 I2C Bus Architecture

**Primary I2C (GPIO11/SDA, GPIO12/SCL):**

```
Speed: 100 kHz - 400 kHz (configurable)
Devices:
├─ FT3168 Touch (0x38)
├─ QMI8658 IMU (0x6B)
├─ PCF85063 RTC (0x51)
├─ ES8311 Audio (0x18)
├─ AXP2101 Power (0x34)
└─ TCA9554 Expander (0x20)

Total: 6 devices maximum on standard I2C
```

Rust Configuration:

```rust
const I2C_FREQUENCY: u32 = 100_000;  // 100 kHz standard
let i2c = I2cDriver::new(
    peripherals.i2c0,
    sda_pin,
    scl_pin,
    &I2cConfig {
        baudrate: I2C_FREQUENCY.Hz(),
        ..Default::default()
    },
)?;
```

### 6.4 SPI Interfaces

**Primary SPI (Display QSPI - AMOLED):**

```
Clock: 40 MHz standard
Mode: QSPI (4 data lines)
CS: GPIO38
Data pins: GPIO6, 11, 13, 14
CLK: GPIO7
Transaction size: Up to 4096 bytes
```

**Secondary SPI (SD Card):**

```
Clock: 20-40 MHz
Mode: Standard 4-wire SPI
CS: EXIO7 (through TCA9554 expander)
Data pins: GPIO1 (MOSI), GPIO3 (MISO)
CLK: GPIO2
```

**T-Circle-S3 SPI (Display):**

```
Clock: 40 MHz standard
Mode: Standard 4-wire SPI
Interface: Arduino_GFX library compatible
```

---

## 7. RUST DEVELOPMENT ENVIRONMENT SETUP

### 7.1 Prerequisites & Installation

**Step 1: Install Rust ESP Toolchain**

```bash
# Download and run installer
curl -LO https://raw.githubusercontent.com/esp-rs/rust-build/main/install-rust-toolchain.sh
chmod +x install-rust-toolchain.sh
./install-rust-toolchain.sh

# Source environment (add to ~/.bashrc or ~/.zshrc)
source export-esp.sh
```

**Step 2: Install Development Tools**

```bash
cargo install espflash cargo-espflash espmonitor ldproxy
```

**Step 3: Create Project from Template**

```bash
cargo install cargo-generate
cargo generate --git https://github.com/esp-rs/esp-idf-template.git
# Select: esp32s3 (Xtensa architecture)
# Select features: heap, logging, Wi-Fi/BLE as needed
```

---

### 7.2 Cargo.toml Configuration

**For ESP32-S3 with ESP-IDF:**

```toml
[package]
name = "esp32-amoled-project"
version = "0.1.0"
edition = "2021"

[dependencies]
esp-idf-sys = { version = "0.33", features = ["esp32s3"] }
esp-idf-hal = { version = "0.42", features = ["esp32s3"] }
esp-idf-svc = { version = "0.47" }

# Display & Graphics
esp-idf-drv-st7789 = "0.1"  # For other displays
ili9341 = "0.5"

# Sensors
mpu6050 = "0.6"  # For IMU-like sensors
ds18b20 = "0.1"  # For temperature

# Audio
i2s-hal = "0.1"

# Serial/Logging
esp-println = "0.5"
log = "0.4"

# Async runtime (optional)
embassy-executor = { version = "0.5", features = ["executor-thread"] }
embassy-time = { version = "0.3", features = ["esp-hal-timer"] }

[profile.release]
opt-level = 3
lto = true
codegen-units = 1

[profile.dev]
opt-level = 0
```

---

### 7.3 Target Triple Configuration

**Create `.cargo/config.toml`:**

```toml
[build]
target = "xtensa-esp32s3-espidf"

[target.xtensa-esp32s3-espidf]
linker = "ldproxy"
runner = "espflash flash --monitor"

[target.xtensa-esp32s3-espidf.env]
ESP_IDF_VERSION = "release/v5.0"
```

---

## 8. CODE CONFIGURATION & SETTINGS

### 8.1 Basic "Hello World" Rust Code

```rust
use esp_idf_sys as _;
use esp_idf_hal::{
    clock::ClockControl,
    delay::Delay,
    gpio::IO,
    peripherals::Peripherals,
    prelude::*,
    system::SystemControl,
};
use esp_println::println;

fn main() {
    // Get peripherals
    let peripherals = Peripherals::take().unwrap();
    let system = SystemControl::new(peripherals.system);
    
    // Configure clocks (80 MHz for battery life, up to 240 MHz available)
    let clocks = ClockControl::max(system.clock_control).freeze();
    let delay = Delay::new(&clocks);
    
    // Initialize logging
    esp_println::logger::init_logger_from_env();
    
    println!("System initialized at {} MHz", clocks.cpu_freq().mhz());
    
    loop {
        println!("Hello from ESP32-S3!");
        delay.delay_ms(1000u32);
    }
}
```

---

### 8.2 Display Driver Configuration

**AMOLED (SH8601 QSPI) - Pseudo-code:**

```rust
use esp_idf_hal::spi::{SpiDriver, SpiConfig};
use esp_idf_hal::gpio::Level;

// Initialize QSPI pins
let cs = io.pins.gpio38.into();
let sclk = io.pins.gpio7.into();
let d0 = io.pins.gpio6.into();
let d1 = io.pins.gpio11.into();
let d2 = io.pins.gpio13.into();
let d3 = io.pins.gpio14.into();

// SPI configuration
let spi_config = SpiConfig::new()
    .baudrate(40.MHz())
    .data_mode(spi::SpiMode::Mode0)
    .cs_active_high(false);

let mut spi = SpiDriver::new(
    peripherals.spi3,
    sclk,
    d0,  // MOSI for QSPI mode
    Some(d1),
    &SpiDriverConfig::new().dma(Dma::Auto(128)),
)?;

// Send display initialization commands
send_init_commands(&mut spi);

// Clear display
send_display_data(&mut spi, &[0x00u8; 329_984]); // 368*448*2 bytes
```

---

### 8.3 Touch Controller Configuration

```rust
use esp_idf_hal::i2c::{I2cDriver, I2cConfig};

// I2C for touch (same bus as other sensors)
let i2c = I2cDriver::new(
    peripherals.i2c0,
    io.pins.gpio11,  // SDA
    io.pins.gpio12,  // SCL
    &I2cConfig::new().baudrate(400.kHz()),
)?;

// Touch controller address
const FT3168_ADDR: u8 = 0x38;

// Read touch point
let mut data = [0u8; 5];
i2c.read(FT3168_ADDR, &mut data, Duration::from_millis(100))?;

// Parse coordinates (format depends on FT3168 configuration)
let x = ((data[2] as u16) << 8) | (data[3] as u16);
let y = ((data[4] as u16)) | ((data[1] as u16 & 0x0F) << 8);
```

---

### 8.4 IMU Sensor Configuration

```rust
const QMI8658_ADDR: u8 = 0x6B;

// Accelerometer raw read
fn read_accel(i2c: &mut I2cDriver) -> Result<(i16, i16, i16)> {
    let mut buf = [0u8; 6];
    i2c.read(QMI8658_ADDR, &mut buf, Duration::from_millis(10))?;
    
    let x = i16::from_le_bytes([buf[0], buf[1]]);
    let y = i16::from_le_bytes([buf[2], buf[3]]);
    let z = i16::from_le_bytes([buf[4], buf[5]]);
    
    Ok((x, y, z))
}

// Typical sensitivity: ±8G = 4096 LSB/G
// x_g = x_raw / 4096
```

---

### 8.5 RTC Configuration

```rust
const RTC_ADDR: u8 = 0x51;

fn set_time(i2c: &mut I2cDriver, seconds: u8, minutes: u8, hours: u8) -> Result<()> {
    let data = [
        0x03,  // Start register (seconds)
        to_bcd(seconds),
        to_bcd(minutes),
        to_bcd(hours),
    ];
    i2c.write(RTC_ADDR, &data, Duration::from_millis(10))
}

fn to_bcd(value: u8) -> u8 {
    ((value / 10) << 4) | (value % 10)
}

fn from_bcd(value: u8) -> u8 {
    ((value >> 4) * 10) + (value & 0x0F)
}
```

---

### 8.6 I2S Audio Configuration

```rust
use esp_idf_hal::i2s::{I2sDriver, I2sConfig, I2sStandardConfig};

let i2s_config = I2sConfig::new()
    .channel_type(I2sChannelType::Stereo)
    .auto_clear(true);

let mut i2s = I2sDriver::new(
    peripherals.i2s0,
    io.pins.gpio46,  // MCLK
    io.pins.gpio8,   // SCLK
    io.pins.gpio9,   // LRCK
    io.pins.gpio45,  // DOUT (to speaker)
    io.pins.gpio16,  // DIN (from microphone)
    &i2s_config,
)?;

// Write audio data (16-bit stereo samples)
let audio_data: &[u8] = &[/* PCM data */];
i2s.write(audio_data, Duration::from_millis(100))?;
```

---

### 8.7 SD/TF Card Access

```rust
use esp_idf_hal::spi::{SpiDriver, SpiDeviceDriver};
use esp_idf_hal::sd::{SdMmcDriver, SdMmcConfig};

// Mount SD card via SDMMC interface (simpler than pure SPI)
let sdmmc_config = SdMmcConfig::new();
let mut sd = SdMmcDriver::new(
    peripherals.sdspi,
    Some(io.pins.gpio2),   // CLK
    Some(io.pins.gpio1),   // MOSI
    Some(io.pins.gpio3),   // MISO
    None,                  // DAT1
    None,                  // DAT2
    Some(io.pins.gpio10),  // DAT3/CS
    &sdmmc_config,
)?;

// List files (example using embedded VFS)
match sd.card() {
    Some(card) => println!("Card size: {} MB", card.size_mb()),
    None => println!("No card detected"),
}
```

---

## 9. MEMORY LAYOUT & STORAGE

### 9.1 Memory Allocation

**Flash Partition Layout (16 MB total):**

```
0x000000 - 0x007FFF    (32 KB)   Bootloader
0x008000 - 0x00FFFF    (32 KB)   Partition table
0x010000 - 0x1FFFFF    (1.95 MB) Application (OTA partition 1)
0x200000 - 0x3FFFFF    (2 MB)    Application (OTA partition 2)
0x400000 - 0xFFFFFF    (~12 MB)  FATFS/SPIFFS file system
```

**SRAM Layout:**

```
512 KB onboard SRAM:
├─ ISR stack, WiFi/BLE buffers: ~150 KB
├─ Heap for malloc: ~200 KB
├─ Display frame buffer: ~330 KB (external PSRAM)
└─ Remaining: Free

8 MB PSRAM:
├─ Display frame buffer (368×448×2): 330 KB
├─ Graphics library buffers: 500 KB
├─ Audio buffers: 64 KB
└─ Available for user: ~7 MB
```

---

### 9.2 Build Size Optimization

**Cargo.toml Release Settings:**

```toml
[profile.release]
opt-level = 3          # Maximum optimization
lto = true             # Link-time optimization
codegen-units = 1      # Better optimization
strip = true           # Strip symbols
panic = "abort"        # Minimal panic handling
```

**Typical Binary Sizes:**
- Minimal app (Hello World): ~500 KB
- With display driver: ~1.2 MB
- Full featured (WiFi + display + sensors): ~1.8 MB

---

## 10. DEVELOPMENT RESOURCES & TIPS

### 10.1 Essential Repositories & Documentation

**Official Resources:**

```
ESP-IDF (C/C++ reference):
https://github.com/espressif/esp-idf

Rust ESP Resources:
https://github.com/esp-rs/esp-idf-hal
https://github.com/esp-rs/esp-idf-template
https://github.com/esp-rs/espflash

"The Rust on ESP" Book:
https://esp-rs.github.io/book/
```

**Community Rust Projects:**

```
LVGL Rust bindings:
https://github.com/lvgl/lvgl-rs

Embassy (async runtime):
https://github.com/embassy-rs/embassy

ESP32-S3 peripherals (PAC):
https://github.com/esp-rs/esp-pacs
```

---

### 10.2 Debugging & Monitoring

**Serial Monitor:**

```bash
# Simple monitor
espmonitor /dev/ttyUSB0 115200

# With filtering
cargo espflash monitor --release 2>&1 | grep -i "error\\|warn"
```

**Logging Setup:**

```rust
use log::*;

fn main() {
    esp_println::logger::init_logger_from_env();
    
    info!("Starting application");
    debug!("Debug level messages");
    warn!("Warning level messages");
    error!("Error level messages");
}

// Run with: RUST_LOG=debug cargo run --release
```

---

### 10.3 Common Troubleshooting

| Issue | Solution |
|-------|----------|
| **Build fails: "target not found"** | Run `rustup target add xtensa-esp32s3-espidf` |
| **Upload timeout** | Press BOOT button + RST button to force download mode |
| **I2C communication fails** | Verify pull-ups on SDA/SCL (typical: 4.7kΩ already onboard) |
| **Display shows garbage** | Check QSPI clock frequency (40 MHz recommended) |
| **Memory exhaustion** | Enable PSRAM and use `_MALLOC_PSRAM` environment variable |
| **WiFi drops randomly** | Reduce CPU frequency or improve antenna placement |

---

### 10.4 Performance Tips

**CPU Clock Optimization:**

```rust
// For maximum performance (more power draw)
let clocks = ClockControl::max(system.clock_control).freeze();
// 240 MHz

// For balanced performance
let clocks = ClockControl::new(system.clock_control)
    .cpu_frequency(160.MHz())
    .freeze();
// 160 MHz

// For power savings
let clocks = ClockControl::new(system.clock_control)
    .cpu_frequency(80.MHz())
    .freeze();
// 80 MHz (still supports WiFi)
```

**Display Rendering Optimization:**

```
1. Use partial updates instead of full screen refresh
2. Enable double buffering for animations (PSRAM available)
3. Limit refresh rate to 30 FPS for battery apps
4. Use QSPI in QUAD mode (4 data lines = 4× speed)
5. Cache frequently used assets in PSRAM
```

---

### 10.5 Useful Crates for ESP32-S3 Development

```toml
[dependencies]
# Async runtime
embassy-executor = "0.5"
embassy-time = "0.3"

# Display & Graphics
esp-idf-drv-st7789 = "0.1"
embedded-graphics = "0.8"
tinybmp = "0.6"

# Sensor libraries
bmp280 = "0.4"
dht-sensor = "0.1"
ads1x1x = "0.2"

# Networking
reqwest = { version = "0.11", features = ["blocking"] }
serde = { version = "1.0", features = ["derive"] }
serde_json = "1.0"

# Debugging
defmt = "0.3"
defmt-rtt = "0.4"

# Math & utilities
libm = "0.2"
heapless = "0.8"
```

---

## APPENDIX A: Quick Reference Pinout Table

### ESP32-S3-Touch-AMOLED-1.8

| Function | Pin(s) | Interface |
|----------|--------|-----------|
| Display QSPI | GPIO 6,7,11,13,14,38 | QSPI |
| Touch I2C | GPIO 11/12 (SDA/SCL) | I2C + GPIO for INT/RST |
| IMU I2C | GPIO 11/12 (SDA/SCL) | I2C @ 0x6B |
| RTC I2C | GPIO 11/12 (SDA/SCL) | I2C @ 0x51 |
| Audio I2S | GPIO 8,9,16,45,46 | I2S + I2C for codec |
| SD Card SPI | GPIO 1,2,3 + EXIO7 | SPI |
| Serial UART | GPIO 43/44 | UART0 |
| GPIO Expander | GPIO 11/12 (SDA/SCL) | I2C @ 0x20 |
| Power Manager | GPIO 11/12 (SDA/SCL) | I2C @ 0x34 |

### T-Circle-S3

| Function | Pin(s) | Interface |
|----------|--------|-----------|
| Display SPI | Standard SPI pins | SPI |
| Touch I2C | Standard I2C | I2C @ 0x15 |
| Audio I2S | Standard I2S pins | I2S |
| RGB LED | GPIO (varies) | SPI (APA102) |
| Rear GPIO | Header pads | GPIO |
| Qwiic | Standard I2C | I2C (via connector) |

---

## APPENDIX B: Essential I2C Address Reference

```
0x15 - CST816D Touch (T-Circle-S3)
0x18 - ES8311 Audio Codec
0x20 - TCA9554 GPIO Expander
0x34 - AXP2101 Power Manager
0x38 - FT3168 Touch (AMOLED)
0x51 - PCF85063 RTC
0x6B - QMI8658 IMU
```

---

## APPENDIX C: Rust Build & Flash Commands

```bash
# Generate ESP-IDF project
cargo generate --git https://github.com/esp-rs/esp-idf-template.git
cd your_project

# Build project
cargo build --release

# Flash to device
cargo espflash flash --release

# Flash + monitor
cargo espflash flash --release --monitor

# Monitor only (if already flashed)
cargo espmonitor /dev/ttyUSB0

# Full: build + flash + monitor (one command)
cargo espflash flash --release --monitor
```

---

**Report Generated:** January 2026  
**Target Devices:** Waveshare ESP32-S3-Touch-AMOLED-1.8 & LILYGO T-Circle-S3  
**Development Language:** Rust (esp-idf-hal, no_std embedded)  
**Architecture:** Xtensa LX7 (ESP32-S3R8)

