# Waveshare ESP32-S3 1.8" AMOLED Touch Display Development Board
## Hardware reference + Rust bring-up notes (default device)

**Board / Waveshare wiki name:** ESP32-S3-Touch-AMOLED-1.8  
**MCU module:** ESP32-S3R8 (Xtensa LX7 dual-core)  
**Display:** 1.8" AMOLED 368×448, **SH8601** (QSPI)  
**Touch:** **FT3168** (I2C)  
**Sensors:** **QMI8658C** IMU (I2C), **PCF85063A** RTC (I2C)  
**Power:** **AXP2101** PMU (I2C)  
**Audio:** **ES8311** codec (I2C + I2S), external amp  
**I/O expander:** **TCA9554** (I2C) exposing `EXIO0..7`  

> **Scope**: this page is the **board spec** we consult before implementing or changing anything hardware-related (pin mapping, buses, I²C addresses, reset/power sequencing).

---

## Quick reference

### I2C bus (shared)

**ESP32-S3 pins:** `GPIO14` = SCL, `GPIO15` = SDA  
**Speed:** 100–400 kHz (start at 100 kHz for bring-up)

| Device | 7-bit addr | Notes |
|--------|------------|------|
| FT3168 touch | `0x38` | Capacitive touch controller |
| QMI8658C IMU | `0x6B` | Alt address `0x6A` depending on SA0 strap |
| PCF85063A RTC | `0x51` | Fixed |
| AXP2101 PMU | `0x34` | Common on this board |
| TCA9554 expander | `0x24` | Verify A0/A1/A2 strap if it differs |
| ES8311 codec | `0x18` | I2C control interface |

### Display (SH8601) – QSPI signals

| Function | GPIO | Notes |
|----------|------|------|
| LCD_CS | `GPIO11` | Chip select |
| QSPI_SCL | `GPIO12` | Clock |
| QSPI_SIO0 | `GPIO4` | IO0 |
| QSPI_SIO1 | `GPIO5` | IO1 |
| QSPI_SIO2 | `GPIO6` | IO2 |
| QSPI_SIO3 | `GPIO7` | IO3 |
| LCD_TE | `GPIO13` | Tearing effect |
| LCD_RESET | `EXIO0` | Via TCA9554 |
| DSI_PWR_EN | `EXIO1` | Via TCA9554 (display power enable) |

### Touch (FT3168)

| Function | Pin | Notes |
|----------|-----|------|
| TP_SCL | `GPIO14` | Shared I2C |
| TP_SDA | `GPIO15` | Shared I2C |
| TP_RESET | `EXIO2` | Via TCA9554 |
| TP_INT | `EXIO6` | Via TCA9554 (interrupt) |

### IMU (QMI8658C)

| Function | Pin | Notes |
|----------|-----|------|
| I2C | `GPIO14/15` | Shared I2C |
| INT1 | `GPIO10` | Board-specific sharing possible |
| INT2 | `EXIO3` | Via TCA9554 |

### RTC (PCF85063A)

| Function | Pin | Notes |
|----------|-----|------|
| I2C | `GPIO14/15` | Shared I2C |
| INT | `GPIO10` | Often shared with IMU INT1 |

### PMU (AXP2101)

| Function | GPIO | Notes |
|----------|------|------|
| I2C | `GPIO14/15` | Shared I2C |
| IRQ | `GPIO40` | PMU interrupt |
| PWRON | `GPIO21` | Power button / enable input (board-specific) |

### Audio (ES8311 + I2S)

**I2C control:** `0x18` on `GPIO14/15`  

| Function | GPIO | Notes |
|----------|------|------|
| I2S_MCLK | `GPIO16` | Master clock |
| I2S_SCLK (BCLK) | `GPIO9` | Bit clock |
| I2S_LRCK (WS) | `GPIO8` | Word select |
| I2S_DSDIN | `GPIO18` | Data into codec |
| I2S_ASDOUT | `GPIO17` | Data out of codec |
| PA_CTRL | `GPIO46` | Amplifier enable |
| Codec_CE | `GPIO45` | Codec chip enable |

### SD card (SDMMC-style wiring)

| Function | GPIO / Pin | Notes |
|----------|------------|------|
| D0 (MOSI) | `GPIO1` | Data |
| CLK (SCLK) | `GPIO2` | Clock |
| D1 (MISO) | `GPIO3` | Data |
| SDCS | `EXIO7` | Chip select via TCA9554 |

### USB + UART

| Function | GPIO | Notes |
|----------|------|------|
| USB_D- | `GPIO19` | Native USB |
| USB_D+ | `GPIO20` | Native USB |
| U0TXD | `GPIO43` | UART0 TX |
| U0RXD | `GPIO44` | UART0 RX |

---

## TCA9554 expander (`EXIO0..7`)

The expander gates critical reset/power signals; **bring it up early**.

Suggested mapping (verify against schematic/board revision):

| EXIO | Signal |
|------|--------|
| EXIO0 | LCD_RESET |
| EXIO1 | DSI_PWR_EN |
| EXIO2 | TP_RESET |
| EXIO3 | QMI_INT2 |
| EXIO6 | TP_INT |
| EXIO7 | SDCS |

> Note: the exact TCA9554 address (often `0x24`) depends on A0/A1/A2 straps; confirm by I2C scan.

---

## Bring-up checklist (recommended order)

1. **I2C scan on GPIO14/15** (start 100 kHz) and confirm you see the expected devices.
2. **Initialize TCA9554** and set safe default output states.
3. **Power/reset sequencing for display**: assert `DSI_PWR_EN`, toggle `LCD_RESET`, then wait for panel ready.
4. **Touch reset**: toggle `TP_RESET`, then read FT3168 registers at `0x38`.
5. **IMU WHO_AM_I** read from QMI8658C (commonly register `0x00`; expected value is board/IC revision-dependent).

---

## References

- **Waveshare wiki**: `https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.8`
- **ESP32-S3 datasheet**: `https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf`
- **ESP32-S3 TRM**: `https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf`

# Waveshare ESP32-S3 1.8" AMOLED Touch Display Development Board
## Comprehensive Technical Guide for Rust Development

**Board Model:** ESP32-S3-Touch-AMOLED-1.8  
**Document Version:** 1.0  
**Last Updated:** January 2026

---

## Table of Contents

1. [Hardware Overview](#hardware-overview)
2. [Technical Specifications](#technical-specifications)
3. [Pin Definitions & GPIO Mapping](#pin-definitions--gpio-mapping)
4. [Component Details & I2C Addresses](#component-details--i2c-addresses)
5. [Display Configuration](#display-configuration)
6. [Rust Development Setup](#rust-development-setup)
7. [Cargo Configuration](#cargo-configuration)
8. [Driver Crates & Dependencies](#driver-crates--dependencies)
9. [Peripheral Initialization Examples](#peripheral-initialization-examples)
10. [Code Examples](#code-examples)
11. [Power Management](#power-management)
12. [Audio Configuration](#audio-configuration)
13. [Troubleshooting](#troubleshooting)

---

## Hardware Overview

The ESP32-S3-Touch-AMOLED-1.8 is a high-performance, compact MCU development board featuring:

- **Microcontroller:** ESP32-S3R8 (Xtensa 32-bit LX7 dual-core, up to 240MHz)
- **Display:** 1.8" AMOLED capacitive touch screen (368×448 pixels, 16.7M colors)
- **Wireless:** 2.4GHz Wi-Fi (802.11 b/g/n) + Bluetooth 5 (LE) with onboard antenna
- **Memory:** 512KB SRAM, 384KB ROM, 8MB PSRAM, 16MB Flash
- **Sensors:** 6-axis IMU (QMI8658C), RTC (PCF85063A)
- **Audio:** ES8311 codec with speaker and microphone
- **Power:** AXP2101 PMU with 3.7V Li-ion battery support
- **Storage:** MicroSD card slot (SDMMC interface)
- **Expansion:** 7× GPIO, 1× I2C, 1× UART, 1× USB pads

---

## Technical Specifications

### Microcontroller (ESP32-S3R8)

| Parameter | Specification |
|-----------|--------------|
| Architecture | Xtensa 32-bit LX7 dual-core |
| Clock Frequency | Up to 240 MHz |
| SRAM | 512 KB |
| ROM | 384 KB |
| PSRAM (embedded) | 8 MB |
| Flash (external) | 16 MB (W25Q128JVSIQ) |
| Wi-Fi | 2.4 GHz 802.11 b/g/n |
| Bluetooth | Bluetooth 5 (LE) |
| Operating Voltage | 3.0V - 3.6V |
| USB | Native USB OTG (GPIO19/20) |

### Display Specifications

| Parameter | Specification |
|-----------|--------------|
| Panel Type | AMOLED |
| Size | 1.8 inch |
| Resolution | 368 × 448 pixels |
| Active Area | 28.70(W) × 34.94(H) mm |
| Pixel Pitch | 78.0 × 78.0 μm |
| Color Depth | 16.7M (24-bit RGB) |
| Brightness | 350 cd/m² |
| Contrast Ratio | 100,000:1 |
| Viewing Angle | 178° |
| Driver IC | SH8601 |
| Interface | QSPI (Quad SPI) |
| Touch Controller | FT3168 (I2C) |
| Touch Type | Capacitive (self-capacitance) |

### Physical Dimensions

| Measurement | Size (mm) |
|-------------|----------|
| Display Outer | 45.20 × 37.60 |
| Display Active Area | 34.94 × 28.70 |
| Board Dimensions | See schematic for exact dimensions |

---

## Pin Definitions & GPIO Mapping

### Core ESP32-S3 Connections

Based on schematic analysis, here are the key pin assignments:

#### Display Interface (QSPI)

| Function | GPIO | Description |
|----------|------|-------------|
| LCD_CS | GPIO11 | Chip Select for display |
| QSPI_SCL | GPIO12 | QSPI Clock |
| QSPI_SIO0 (MOSI) | GPIO4 | Data line 0 |
| QSPI_SI1 | GPIO5 | Data line 1 |
| QSPI_SI2 | GPIO6 | Data line 2 |
| QSPI_SI3 | GPIO7 | Data line 3 |
| LCD_RESET | EXIO0 (via TCA9554) | Display reset |
| LCD_TE | GPIO13 | Tearing effect signal |
| DSI_PWR_EN | EXIO1 (via TCA9554) | Display power enable |

#### Touch Controller (FT3168 - I2C)

| Function | GPIO | I2C Address | Description |
|----------|------|-------------|-------------|
| TP_SDA | GPIO15 | 0x38 | Touch I2C Data |
| TP_SCL | GPIO14 | 0x38 | Touch I2C Clock |
| TP_INT | EXIO6 (via TCA9554) | - | Touch interrupt |
| TP_RESET | EXIO2 (via TCA9554) | - | Touch reset |

**Note:** FT3168 I2C slave address is 0x38 (7-bit addressing)

#### I2C Bus (Shared Components)

| Function | GPIO | Description |
|----------|------|-------------|
| ESP32_SDA | GPIO15 | Main I2C Data (shared bus) |
| ESP32_SCL | GPIO14 | Main I2C Clock (shared bus) |

**Shared I2C Devices on GPIO14/15:**
- FT3168 Touch Controller (0x38)
- QMI8658C IMU (0x6B)
- PCF85063A RTC (0x51)
- AXP2101 PMU (0x34)
- TCA9554 GPIO Expander (0x24 - address may vary)
- ES8311 Audio Codec (0x18)

#### 6-Axis IMU (QMI8658C - I2C)

| Function | GPIO | I2C Address | Description |
|----------|------|-------------|-------------|
| QMI_SDA | GPIO15 (shared) | 0x6B | IMU I2C Data |
| QMI_SCL | GPIO14 (shared) | 0x6B | IMU I2C Clock |
| QMI_INT1 | GPIO10 | - | Interrupt 1 output |
| QMI_INT2 | EXIO3 (via TCA9554) | - | Interrupt 2 output |

**Note:** QMI8658C I2C address is 0x6B when SA0 is high (default on this board)

#### RTC (PCF85063A - I2C)

| Function | GPIO | I2C Address | Description |
|----------|------|-------------|-------------|
| RTC_SDA | GPIO15 (shared) | 0x51 | RTC I2C Data |
| RTC_SCL | GPIO14 (shared) | 0x51 | RTC I2C Clock |
| RTC_INT | GPIO10 (shared with QMI_INT1) | - | RTC interrupt output |

**Note:** PCF85063A I2C address is 0x51 (fixed)

#### Power Management (AXP2101 - I2C)

| Function | GPIO | I2C Address | Description |
|----------|------|-------------|-------------|
| AXP_SDA | GPIO15 (shared) | 0x34 | PMU I2C Data |
| AXP_SCL | GPIO14 (shared) | 0x34 | PMU I2C Clock |
| AXP_IRQ | GPIO40 | - | PMU interrupt |
| PWRON | GPIO21 (chip enable) | - | Power button input |

**Note:** AXP2101 I2C address is 0x34 (can be 0x68/0x69 depending on config, but schematic shows 0x34)

#### GPIO Expander (TCA9554 - I2C)

| Function | GPIO | I2C Address | Description |
|----------|------|-------------|-------------|
| TCA_SDA | GPIO15 (shared) | 0x24 (typical) | Expander I2C Data |
| TCA_SCL | GPIO14 (shared) | 0x24 (typical) | Expander I2C Clock |
| TCA_INT | GPIO16 (likely) | - | Expander interrupt |

**TCA9554 Extended I/O Pins:**
- EXIO0: LCD_RESET
- EXIO1: DSI_PWR_EN  
- EXIO2: TP_RESET
- EXIO3: QMI_INT2
- EXIO4: System control
- EXIO5: PWRON (connected)
- EXIO6: TP_INT
- EXIO7: SD Card CS (SDCS)

#### Audio Codec (ES8311 - I2C + I2S)

**I2C Interface:**

| Function | GPIO | I2C Address | Description |
|----------|------|-------------|-------------|
| ES_SDA | GPIO15 (shared) | 0x18 | Codec I2C Data |
| ES_SCL | GPIO14 (shared) | 0x18 | Codec I2C Clock |

**I2S Interface:**

| Function | GPIO | Description |
|----------|------|-------------|
| I2S_MCLK | GPIO16 | Master clock input |
| I2S_SCLK | GPIO9 | Serial clock (bit clock) |
| I2S_LRCK | GPIO8 | Left/Right clock (word select) |
| I2S_DSDIN | GPIO18 | Data input (to codec) |
| I2S_ASDOUT | GPIO17 | Data output (from codec) |
| PA_CTRL | GPIO46 | Power amplifier enable |
| Codec_CE | GPIO45 | Codec chip enable |

**Audio Peripherals:**
- Microphone: Connected to ES8311 analog inputs (MIC1P/MIC1N)
- Speaker: Connected via NS4150B amplifier (GPIO46 controls PA)

#### SD Card (SDMMC Interface)

| Function | GPIO | Description |
|----------|------|-------------|
| MOSI (D0) | GPIO1 | Data line 0 |
| SCLK (CLK) | GPIO2 | Clock |
| MISO (D1) | GPIO3 | Data line 1 (can use 1-bit or 4-bit mode) |
| SDCS (CS) | EXIO7 (via TCA9554) | Chip select |

**Note:** Can operate in 1-bit or 4-bit SDMMC mode. Using SPI mode pins are GPIO1/2/3.

#### USB Interface

| Function | GPIO | Description |
|----------|------|-------------|
| USB_P (D+) | GPIO20 | USB Data Plus |
| USB_N (D-) | GPIO19 | USB Data Minus |

**Note:** Native USB OTG interface, not USB-to-Serial bridge

#### UART Interface (Debug/Programming)

| Function | GPIO | Description |
|----------|------|-------------|
| U0TXD | GPIO43 | UART0 TX |
| U0RXD | GPIO44 | UART0 RX |

#### Strapping Pins (Boot Configuration)

| Pin | Default | Boot Mode Control |
|-----|---------|------------------|
| GPIO0 | Pull-up (1) | Boot mode selection |
| GPIO3 | Floating | JTAG signal source |
| GPIO45 | Pull-down (0) | - |
| GPIO46 | Pull-down (0) | Boot mode selection |

**Boot Mode Selection:**
- **SPI Boot (normal):** GPIO0 = 1, GPIO46 = any
- **Download Boot:** GPIO0 = 0, GPIO46 = 0

#### System Control Pins

| Function | GPIO | Description |
|----------|------|-------------|
| CHIP_PU (EN) | External pull-up | Chip enable (reset) |
| GPIO0 | Strapping + PWR Button | Boot mode + power button |
| GPIO38 | User defined | General purpose |
| GPIO39 | User defined | General purpose |
| GPIO41 | User defined | General purpose |
| GPIO42 | User defined | General purpose |

#### Available GPIO Expansion Pads

According to the board, the following GPIOs are exposed on expansion headers:

| GPIO | Note | Availability |
|------|------|-------------|
| GPIO4 | Used by display QSPI | Not available |
| GPIO5 | Used by display QSPI | Not available |
| GPIO6 | Used by display QSPI | Not available |
| GPIO7 | Used by display QSPI | Not available |
| GPIO17 | Used by I2S | Check availability |
| GPIO18 | Used by I2S | Check availability |
| GPIO41 | Available | General purpose |
| GPIO42 | Available | General purpose |

**Caution:** Verify which pins are actually broken out to expansion headers by checking physical board markings.

---

## Component Details & I2C Addresses

### Complete I2C Device Summary

The board uses a shared I2C bus on GPIO14 (SCL) and GPIO15 (SDA) with the following devices:

| Device | I2C Address | Description | Interrupt GPIO |
|--------|-------------|-------------|----------------|
| FT3168 | 0x38 | Capacitive touch controller | EXIO6 |
| QMI8658C | 0x6B | 6-axis IMU (accel + gyro) | GPIO10, EXIO3 |
| PCF85063A | 0x51 | Real-time clock | GPIO10 |
| AXP2101 | 0x34 | Power management IC | GPIO40 |
| TCA9554 | 0x24 | I/O expander (8-bit) | GPIO16 (likely) |
| ES8311 | 0x18 | Audio codec | - |

**I2C Bus Configuration:**
- Speed: Up to 400 kHz (Fast Mode)
- Pull-ups: 2.2kΩ on-board resistors
- Voltage: 3.3V logic level

### Display Driver (SH8601)

**Interface:** QSPI (Quad SPI)  
**Communication Protocol:** MIPI DBI Type C (4-wire SPI with data/command mode)

**Key Characteristics:**
- 4-data line QSPI for high-speed transfers
- Supports 16-bit RGB565 and 24-bit RGB888 color formats
- Frame buffer: Internal GRAM
- Tearing effect pin (TE) for synchronization
- Brightness control via register 0x51 (0x00-0xFF)

**Register Access:**
- Command/Data selection via DC pin (not applicable in QSPI mode)
- Commands sent via QSPI protocol
- See SH8601 datasheet for full register map

### Touch Controller (FT3168)

**Interface:** I2C  
**I2C Address:** 0x38 (7-bit)  
**Communication Speed:** 100 kHz ~ 400 kHz

**Features:**
- Self-capacitance touch sensing
- Multi-touch support (exact number not specified, typically 5-10 points)
- Touch coordinates: 368×448 (matches display resolution)
- Interrupt-driven operation (active low)
- Gesture recognition support

**Register Map (Key Registers):**
- Touch point data starts at register 0x03
- Number of touch points at register 0x02
- Touch X/Y coordinates in big-endian format

**Initialization:**
- Requires power-on reset sequence
- Default settings generally suitable
- Interrupt pin goes low when touch detected

### IMU (QMI8658C)

**Interface:** I2C  
**I2C Address:** 0x6B (when SA0 = 1, which is the default)  
**Alternative Address:** 0x6A (when SA0 = 0)

**Accelerometer Specifications:**
- Range: ±2g, ±4g, ±8g, ±16g (configurable)
- Resolution: 16-bit
- Output data rate: Configurable

**Gyroscope Specifications:**
- Range: ±16, ±32, ±64, ±128, ±256, ±512, ±1024, ±2048 dps (configurable)
- Resolution: 16-bit
- Output data rate: Configurable

**Key Features:**
- Motion detection
- Step counting
- Configurable interrupts on INT1 and INT2 pins
- Low power modes

**Register Map (Key Registers):**
- WHO_AM_I: 0x00 (returns device ID)
- Accelerometer data: 0x35-0x3A (X, Y, Z 16-bit values)
- Gyroscope data: 0x3B-0x40 (X, Y, Z 16-bit values)
- Control registers: 0x02-0x08

### RTC (PCF85063A)

**Interface:** I2C  
**I2C Address:** 0x51 (fixed, 7-bit)  
**Communication Speed:** Up to 400 kHz

**Features:**
- Real-time clock and calendar
- Battery backup support (via AXP2101)
- Alarm function
- Clock output (32.768 kHz, 1024 Hz, 32 Hz, 1 Hz)
- Offset register for fine-tuning
- Low power consumption

**Time Format:**
- Seconds, minutes, hours (12/24-hour format)
- Day, weekday, month, year
- Century bit for year rollover

**Register Map:**
- Time/date registers: 0x04-0x0A
- Alarm registers: 0x0B-0x0E
- Control registers: 0x00-0x01

**Backup Battery:**
- Primary power from AXP2101
- Optional backup battery pads for continuous RTC operation

### Power Management (AXP2101)

**Interface:** I2C + Power control  
**I2C Address:** 0x34 (can be 0x68/0x69 in other configurations)  
**Alternative Protocol:** RSB (for Allwinner platforms)

**DC-DC Converters (4 channels):**
- DCDC1: 3.3V (VCC3V3) - Main system power
- DCDC2: 0.9V - CPU core voltage
- DCDC3: 1.2V - Memory/peripheral voltage
- DCDC4: 1.8V - I/O voltage
- DCDC5: Configurable (not used on this board)

**LDO Regulators (11 channels):**
- ALDO1: 3.3V (VL1_3.3V)
- ALDO2: 3.3V (VL2_3.3V)
- ALDO3: 3.0V (VL3_3V)
- ALDO4: 1.8V (VL3_1.8V)
- BLDO1: 1.2V (VL_1.2V)
- BLDO2: 2.8V (VL_2.8V)
- CPUSLDO: 1.2V (VCL_1.2V)
- DLDO1/2: Configurable
- RTCLDO: RTC backup power

**Battery Charging:**
- Input: USB VBUS (5V) via Type-C
- Battery: 3.7V Li-ion (JST PH1.25 connector)
- Charge current: Configurable up to 1A
- Pre-charge and fast-charge support
- Target voltage: 4.2V (configurable: 4.0V-4.6V)

**Key Registers:**
- Power status: 0x00, 0x01
- DC-DC enable: Various registers
- Battery status: 0x01[6:5]
- IRQ control: 0x40-0x4C
- ADC data: Multiple registers for voltage/current monitoring

**Protection Features:**
- Over-voltage protection (OVP)
- Over-current protection (OCP)
- Over-temperature protection (OTP)
- Under-voltage lockout (UVLO)

**E-Gauge (Fuel Gauge):**
- Battery capacity estimation
- Coulomb counter
- Real-time SoC (State of Charge) calculation

### Audio Codec (ES8311)

**Interface:** I2C (control) + I2S (audio data)  
**I2C Address:** 0x18 (typical)

**I2S Configuration:**
- Master or slave mode
- Sample rates: 8 kHz - 96 kHz (configurable)
- Data format: I2S, Left-Justified, Right-Justified, PCM/DSP
- Word length: 16, 18, 20, 24, 32 bits

**Analog Inputs:**
- MIC1P/MIC1N: Differential microphone input
- Built-in microphone bias
- Programmable gain amplifier (PGA)

**Analog Outputs:**
- OUTP/OUTN: Differential line output to amplifier
- Headphone output capability

**Power Amplifier (NS4150B):**
- Class D amplifier (external chip)
- Enable control: GPIO46 (PA_CTRL)
- Power: 3W typical
- Drives 8Ω speaker

**Key Features:**
- Low power consumption
- High SNR (Signal-to-Noise Ratio)
- Programmable gain and volume control
- ALC (Automatic Level Control)
- Digital volume control

---

## Display Configuration

### SH8601 QSPI Interface Details

**Initialization Sequence:**
1. Reset display via EXIO0 (GPIO expander)
2. Enable display power via EXIO1
3. Configure QSPI peripheral on ESP32-S3
4. Send initialization commands
5. Configure brightness and color mode
6. Enable display output

**QSPI Pin Configuration:**

```
CS:    GPIO11  (Chip Select - active low)
SCLK:  GPIO12  (Clock - up to 80 MHz)
IO0:   GPIO4   (Data line 0 / MOSI in SPI mode)
IO1:   GPIO5   (Data line 1 / MISO in SPI mode)
IO2:   GPIO6   (Data line 2 / WP in SPI mode)
IO3:   GPIO7   (Data line 3 / HOLD in SPI mode)
```

**Color Formats:**
- RGB565: 16 bits per pixel (5-6-5 bit distribution)
- RGB666: 18 bits per pixel (6-6-6 bit distribution)
- RGB888: 24 bits per pixel (8-8-8 bit distribution)

**Frame Buffer:**
- Size: 368 × 448 pixels
- RGB565: ~322 KB
- RGB888: ~483 KB

**Brightness Control:**
- Register: 0x51
- Range: 0x00 (off) to 0xFF (maximum 350 cd/m²)
- Linear brightness control

**Display Timing:**
- TE (Tearing Effect) signal on GPIO13
- Synchronize writes to avoid tearing
- Typical frame rate: 60 Hz

### Touch Controller Configuration

**Calibration:**
- Factory calibrated for 368×448 resolution
- No additional calibration typically required
- Touch coordinates map 1:1 to display pixels

**Touch Detection:**
- Poll I2C registers or use interrupt pin (EXIO6)
- Interrupt fires on touch press/release
- Read touch point count and coordinates

**Gesture Support:**
- Depends on FT3168 firmware
- May support swipe, pinch, rotate gestures
- Check FT3168 datasheet for gesture registers

---

## Rust Development Setup

### Development Approaches

There are **two main approaches** to ESP32 Rust development:

#### 1. **Bare-Metal (no_std) with esp-hal**

**Best for:**
- Resource-constrained applications
- Real-time systems
- Maximum performance
- Full control over hardware

**Characteristics:**
- No standard library (`#![no_std]`)
- Direct hardware abstraction layers
- Smaller binary sizes
- Embassy-based async runtime
- Lower overhead

**Toolchain:**
- Rust nightly or stable (with Xtensa support)
- `esp-hal` crate family

#### 2. **std with ESP-IDF (esp-idf-hal)**

**Best for:**
- Rapid prototyping
- Networking (Wi-Fi, BLE) integration
- Using ESP-IDF drivers and libraries
- Standard Rust ecosystem compatibility

**Characteristics:**
- Full standard library support
- ESP-IDF framework integration
- FreeRTOS underneath
- More overhead, larger binaries
- Easier networking and peripherals

**Toolchain:**
- Rust nightly (required for Xtensa)
- `esp-idf-sys`, `esp-idf-hal`, `esp-idf-svc` crates
- ESP-IDF installed locally or managed by build system

### Installing Rust Toolchain for ESP32-S3

#### Prerequisites

```bash
# Install Rust (if not already installed)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env

# Install dependencies (Debian/Ubuntu)
sudo apt-get install git wget flex bison gperf python3 python3-pip \
  python3-venv cmake ninja-build ccache libffi-dev libssl-dev \
  dfu-util libusb-1.0-0 libudev-dev
```

#### Option 1: Bare-Metal (esp-hal) Setup

```bash
# Install Rust nightly (esp-hal supports stable for RISC-V, nightly for Xtensa)
rustup install nightly
rustup default nightly

# Add Xtensa target
rustup component add rust-src --toolchain nightly

# Install cargo tools
cargo install cargo-generate
cargo install ldproxy
cargo install espup
cargo install espflash
cargo install cargo-espflash

# Install ESP Rust toolchain
espup install
# Follow instructions to add to PATH
source $HOME/export-esp.sh
```

#### Option 2: std with ESP-IDF Setup

```bash
# Install ESP-IDF prerequisites
sudo apt-get install git wget flex bison gperf python3 python3-pip \
  python3-venv cmake ninja-build ccache libffi-dev libssl-dev \
  dfu-util libusb-1.0-0

# Install Rust nightly (required for Xtensa)
rustup install nightly
rustup default nightly
rustup component add rust-src --toolchain nightly

# Install tools
cargo install cargo-generate
cargo install ldproxy
cargo install espup
cargo install espflash
cargo install cargo-espflash

# Install ESP Rust toolchain with ESP-IDF support
espup install
source $HOME/export-esp.sh

# Set up ESP-IDF (automatically managed or manual)
# Automatic: esp-idf-sys will download and build ESP-IDF
# Manual: Install ESP-IDF separately for faster builds
```

### Project Setup

#### Using cargo-generate (Recommended)

**Bare-Metal Project:**

```bash
cargo generate esp-rs/esp-template
# Select:
# - Which MCU: esp32s3
# - Configure advanced: Yes
# - Enable peripheral support: Yes (hal)
# - Enable WiFi: If needed
# - Configure project: customize name, etc.
```

**ESP-IDF Project:**

```bash
cargo generate esp-rs/esp-idf-template cargo
# Select:
# - Configure project: esp32s3
# - Advanced options as needed
```

#### Manual Project Setup

**Bare-Metal Project Structure:**

```
my-esp32s3-project/
├── .cargo/
│   └── config.toml
├── src/
│   └── main.rs
├── Cargo.toml
└── rust-toolchain.toml
```

**ESP-IDF Project Structure:**

```
my-esp32s3-idf-project/
├── .cargo/
│   └── config.toml
├── src/
│   └── main.rs
├── Cargo.toml
├── build.rs
├── sdkconfig.defaults (optional)
└── rust-toolchain.toml
```

---

## Cargo Configuration

### .cargo/config.toml (Bare-Metal)

```toml
[build]
target = "xtensa-esp32s3-none-elf"

[target.xtensa-esp32s3-none-elf]
linker = "ldproxy"
runner = "espflash flash --monitor"

# ESP32-S3 with 8MB PSRAM and 16MB Flash
rustflags = [
  "-C", "link-arg=-Tlinkall.x",
  "-C", "link-arg=-Trom_functions.x",
]

[unstable]
build-std = ["core", "alloc"]

[env]
ESP_LOG = "debug"
DEFMT_LOG = "trace"
```

### .cargo/config.toml (ESP-IDF)

```toml
[build]
target = "xtensa-esp32s3-espidf"

[target.xtensa-esp32s3-espidf]
linker = "ldproxy"
runner = "espflash flash --monitor"

rustflags = [
    "-C", "default-linker-libraries",
]

[unstable]
build-std = ["std", "panic_abort"]

[env]
MCU = "esp32s3"
ESP_IDF_VERSION = "v5.3"  # Or specific version
ESP_IDF_SDKCONFIG_DEFAULTS = "sdkconfig.defaults"
ESP_IDF_TOOLS_INSTALL_DIR = "global"  # or "workspace" or custom path
```

### Cargo.toml (Bare-Metal Example)

```toml
[package]
name = "esp32s3-amoled-demo"
version = "0.1.0"
edition = "2021"

[dependencies]
esp-hal = { version = "0.20", features = ["esp32s3"] }
esp-backtrace = { version = "0.14", features = ["esp32s3", "println"] }
esp-println = { version = "0.11", features = ["esp32s3"] }
embedded-hal = "1.0"
embedded-hal-async = "1.0"
embedded-io = "0.6"
embedded-io-async = "0.6"

# Display and graphics
embedded-graphics = "0.8"
sh8601-rs = "0.1"  # SH8601 driver (if available)
# Or use mipidsi for generic MIPI displays
display-interface = "0.5"
display-interface-spi = "0.5"

# Touch
# FT3168 driver (may need to create or find)

# IMU
# QMI8658 Rust driver (check crates.io or GitHub)

# RTC
# PCF85063 driver (check crates.io)

# Optional: Async runtime
embassy-executor = { version = "0.6", features = ["arch-xtensa", "executor-thread"] }
embassy-time = "0.3"

[profile.release]
opt-level = "z"  # Optimize for size
lto = true
codegen-units = 1
strip = true
```

### Cargo.toml (ESP-IDF Example)

```toml
[package]
name = "esp32s3-amoled-demo"
version = "0.1.0"
edition = "2021"

[dependencies]
esp-idf-svc = { version = "0.49", features = ["binstart"] }
esp-idf-hal = "0.44"
esp-idf-sys = { version = "0.35", features = ["binstart"] }
embedded-hal = "1.0"
embedded-svc = "0.28"

# Graphics
embedded-graphics = "0.8"

# Logging
log = "0.4"
env_logger = "0.11"

[build-dependencies]
embuild = "0.32"

[profile.release]
opt-level = "z"
lto = true
codegen-units = 1
strip = true

[profile.dev]
debug = true
opt-level = "s"

[package.metadata.esp-idf-sys]
esp_idf_tools_install_dir = "global"
esp_idf_version = "v5.3"
esp_idf_sdkconfig_defaults = ["sdkconfig.defaults"]
```

### rust-toolchain.toml

```toml
[toolchain]
channel = "nightly-2024-12-01"  # Or specific nightly version
components = ["rust-src", "rustfmt", "clippy"]
targets = ["xtensa-esp32s3-none-elf"]  # or xtensa-esp32s3-espidf for std
```

---

## Driver Crates & Dependencies

### Available Crates for This Board

#### Display Drivers

**SH8601 Specific:**

```toml
# Rust crate for SH8601 (QSPI AMOLED driver)
[dependencies]
sh8601-rs = "0.1"  # Check crates.io for latest
```

**Generic MIPI DBI:**

If no SH8601-specific crate exists, use `mipidsi`:

```toml
[dependencies]
mipidsi = "0.8"
display-interface = "0.5"
display-interface-spi = "0.5"
```

**Usage with mipidsi:**

```rust
use mipidsi::{Builder, models::ILI9486Rgb666};  // Use closest compatible model
use display_interface_spi::SPIInterface;
```

#### Touch Controller

**FT3168:**

No official crate at time of writing. Options:

1. **Use generic I2C touch driver pattern**
2. **Port from Arduino/C libraries**
3. **Implement using embedded-hal I2C traits**

**Reference implementation approach:**

```rust
// Pseudo-code for FT3168
pub struct FT3168<I2C> {
    i2c: I2C,
    address: u8,  // 0x38
}

impl<I2C, E> FT3168<I2C>
where
    I2C: embedded_hal::i2c::I2c<Error = E>,
{
    pub fn new(i2c: I2C) -> Self {
        Self {
            i2c,
            address: 0x38,
        }
    }

    pub fn read_touch_points(&mut self) -> Result<Vec<TouchPoint>, E> {
        // Implementation
    }
}
```

#### IMU Sensor

**QMI8658:**

```toml
[dependencies]
# Check for QMI8658 crate on crates.io or GitHub
# May need to use from git if not published
qmi8658 = { git = "https://github.com/IniterWorker/qmi8658", optional = true }
```

**Alternative:** Implement using `embedded-hal` I2C traits

#### RTC

**PCF85063:**

```toml
[dependencies]
pcf85063 = "0.1"  # Or pcf8563 (very similar, may be compatible)
# Or use embedded-time compatible RTC crate
```

**PCF8563 (close relative, may work):**

```toml
[dependencies]
pcf8563 = "0.2"
```

#### Power Management

**AXP2101:**

No official Rust crate. Options:

```toml
# Use XPowersLib bindings or implement manually
# Check esp-idf-lib components for AXP2101 if using ESP-IDF
```

**ESP-IDF approach (std):**

```toml
# In ESP-IDF mode, can use C component
[[package.metadata.esp-idf-sys.extra_components]]
remote_component = { name = "xpowers_axp2101", git = "https://github.com/XPowersLib" }
```

#### Graphics

**Embedded Graphics:**

```toml
[dependencies]
embedded-graphics = "0.8"

# Optional: For text rendering
embedded-text = "0.7"

# Optional: For image loading
tinybmp = "0.5"
tinypng = "0.1"
```

### Peripheral HAL Summary

| Peripheral | bare-metal (esp-hal) | std (esp-idf-hal) |
|------------|----------------------|-------------------|
| GPIO | `esp_hal::gpio` | `esp_idf_hal::gpio` |
| I2C | `esp_hal::i2c::I2c` | `esp_idf_hal::i2c::I2cDriver` |
| SPI/QSPI | `esp_hal::spi` | `esp_idf_hal::spi::SpiDriver` |
| I2S | `esp_hal::i2s` | `esp_idf_hal::i2s` |
| UART | `esp_hal::uart` | `esp_idf_hal::uart::UartDriver` |
| ADC | `esp_hal::analog::adc` | `esp_idf_hal::adc` |
| USB | `esp_hal::usb_serial_jtag` | ESP-IDF USB components |
| Wi-Fi | `esp-wifi` (bare-metal) | `esp_idf_svc::wifi` |
| Bluetooth | `esp-wifi` (BLE) | `esp_idf_svc::bt` |

---

## Peripheral Initialization Examples

### GPIO Expander (TCA9554)

The TCA9554 I/O expander controls several critical pins including display reset and touch reset.

**Bare-Metal (esp-hal):**

```rust
use esp_hal::i2c::I2c;
use esp_hal::gpio::Io;
use esp_hal::peripherals::Peripherals;

const TCA9554_ADDR: u8 = 0x24;  // Verify actual address

// TCA9554 Registers
const TCA9554_INPUT: u8 = 0x00;
const TCA9554_OUTPUT: u8 = 0x01;
const TCA9554_POLARITY: u8 = 0x02;
const TCA9554_CONFIG: u8 = 0x03;  // 0 = output, 1 = input

pub struct TCA9554<I2C> {
    i2c: I2C,
    address: u8,
}

impl<I2C, E> TCA9554<I2C>
where
    I2C: embedded_hal::i2c::I2c<Error = E>,
{
    pub fn new(i2c: I2C, address: u8) -> Self {
        Self { i2c, address }
    }

    pub fn init(&mut self) -> Result<(), E> {
        // Configure pins as outputs (0) or inputs (1)
        // EXIO0: LCD_RESET (output)
        // EXIO1: DSI_PWR_EN (output)
        // EXIO2: TP_RESET (output)
        // EXIO3: QMI_INT2 (input)
        // EXIO6: TP_INT (input)
        // EXIO7: SDCS (output)
        
        let config: u8 = 0b01001000;  // Bit 3 and 6 as inputs
        self.write_register(TCA9554_CONFIG, config)?;
        
        // Set initial output states
        let output: u8 = 0b00000011;  // EXIO0, EXIO1 high
        self.write_register(TCA9554_OUTPUT, output)?;
        
        Ok(())
    }

    pub fn set_pin(&mut self, pin: u8, state: bool) -> Result<(), E> {
        let mut current = self.read_register(TCA9554_OUTPUT)?;
        if state {
            current |= 1 << pin;
        } else {
            current &= !(1 << pin);
        }
        self.write_register(TCA9554_OUTPUT, current)
    }

    pub fn read_pin(&mut self, pin: u8) -> Result<bool, E> {
        let value = self.read_register(TCA9554_INPUT)?;
        Ok((value & (1 << pin)) != 0)
    }

    fn write_register(&mut self, reg: u8, value: u8) -> Result<(), E> {
        self.i2c.write(self.address, &[reg, value])
    }

    fn read_register(&mut self, reg: u8) -> Result<u8, E> {
        let mut buffer = [0u8];
        self.i2c.write_read(self.address, &[reg], &mut buffer)?;
        Ok(buffer[0])
    }
}

// Usage
fn main() -> ! {
    let peripherals = Peripherals::take();
    let io = Io::new(peripherals.GPIO, peripherals.IO_MUX);

    // Initialize I2C
    let i2c = I2c::new(
        peripherals.I2C0,
        io.pins.gpio15,  // SDA
        io.pins.gpio14,  // SCL
        400.kHz(),
    );

    let mut expander = TCA9554::new(i2c, TCA9554_ADDR);
    expander.init().unwrap();

    // Control LCD reset (EXIO0)
    expander.set_pin(0, false).unwrap();  // Reset low
    // delay...
    expander.set_pin(0, true).unwrap();   // Reset high

    loop {}
}
```

**ESP-IDF (std):**

```rust
use esp_idf_hal::i2c::*;
use esp_idf_hal::peripherals::Peripherals;

fn main() {
    esp_idf_sys::link_patches();

    let peripherals = Peripherals::take().unwrap();

    let i2c_config = I2cConfig::new()
        .baudrate(400.kHz().into());

    let i2c = I2cDriver::new(
        peripherals.i2c0,
        peripherals.pins.gpio15,  // SDA
        peripherals.pins.gpio14,  // SCL
        &i2c_config,
    ).unwrap();

    let mut expander = TCA9554::new(i2c, 0x24);
    expander.init().unwrap();
    
    // Use as above
}
```

### I2C Initialization

**Bare-Metal (esp-hal):**

```rust
use esp_hal::{
    clock::ClockControl,
    gpio::Io,
    i2c::I2c,
    peripherals::Peripherals,
    prelude::*,
};

fn main() -> ! {
    let peripherals = Peripherals::take();
    let io = Io::new(peripherals.GPIO, peripherals.IO_MUX);

    // Initialize I2C on GPIO14 (SCL) and GPIO15 (SDA)
    let mut i2c = I2c::new(
        peripherals.I2C0,
        io.pins.gpio15,  // SDA
        io.pins.gpio14,  // SCL
        400.kHz(),       // Fast mode
    );

    // Scan I2C bus
    println!("Scanning I2C bus...");
    for addr in 0x08..0x78 {
        if i2c.write(addr, &[]).is_ok() {
            println!("Found device at address: 0x{:02X}", addr);
        }
    }

    loop {}
}
```

**ESP-IDF (std):**

```rust
use esp_idf_hal::i2c::*;
use esp_idf_hal::peripherals::Peripherals;
use esp_idf_hal::prelude::*;

fn main() {
    esp_idf_sys::link_patches();

    let peripherals = Peripherals::take().unwrap();

    let i2c_config = I2cConfig::new()
        .baudrate(400.kHz().into())
        .sda_pullup_enabled(true)
        .scl_pullup_enabled(true);

    let mut i2c = I2cDriver::new(
        peripherals.i2c0,
        peripherals.pins.gpio15,  // SDA
        peripherals.pins.gpio14,  // SCL
        &i2c_config,
    ).unwrap();

    // Scan I2C bus
    println!("Scanning I2C bus...");
    for addr in 0x08..0x78 {
        if i2c.write(addr, &[], 1000).is_ok() {
            println!("Found device at address: 0x{:02X}", addr);
        }
    }

    loop {
        std::thread::sleep(std::time::Duration::from_secs(1));
    }
}
```

### QSPI Display Initialization

**Bare-Metal (esp-hal):**

```rust
use esp_hal::{
    gpio::{Io, Level, Output},
    peripherals::Peripherals,
    prelude::*,
    spi::{master::Spi, SpiMode},
    Delay,
};

fn main() -> ! {
    let peripherals = Peripherals::take();
    let io = Io::new(peripherals.GPIO, peripherals.IO_MUX);
    let mut delay = Delay::new();

    // Initialize GPIO expander first (see previous section)
    // Then control LCD_RESET and DSI_PWR_EN via expander

    // QSPI pins
    let sclk = io.pins.gpio12;
    let cs = io.pins.gpio11;
    let sio0 = io.pins.gpio4;  // MOSI
    let sio1 = io.pins.gpio5;
    let sio2 = io.pins.gpio6;
    let sio3 = io.pins.gpio7;

    // Configure SPI in QSPI mode
    // Note: esp-hal may not directly support QSPI for display
    // You might need to use raw SPI registers or ESP-IDF

    // For now, standard SPI example:
    let mut spi = Spi::new(
        peripherals.SPI2,
        sclk,
        sio0,  // MOSI
        sio1,  // MISO (not used in display writes)
        cs,
        80.MHz(),
        SpiMode::Mode0,
    );

    // Initialize display
    // 1. Reset via GPIO expander
    // 2. Send init commands
    // 3. Configure display parameters

    loop {}
}
```

**Note:** Full QSPI support for displays may require ESP-IDF or custom implementation. The `sh8601-rs` crate (if available) would handle this.

### I2S Audio Initialization

**Bare-Metal (esp-hal):**

```rust
use esp_hal::{
    gpio::Io,
    i2s::I2s,
    peripherals::Peripherals,
    prelude::*,
};

fn main() -> ! {
    let peripherals = Peripherals::take();
    let io = Io::new(peripherals.GPIO, peripherals.IO_MUX);

    // I2S pins
    let mclk = io.pins.gpio16;
    let bclk = io.pins.gpio9;
    let ws = io.pins.gpio8;
    let dout = io.pins.gpio18;  // To codec
    let din = io.pins.gpio17;   // From codec

    // Initialize I2S
    let mut i2s = I2s::new(
        peripherals.I2S0,
        mclk,
        bclk,
        ws,
        dout,
        din,
    );

    // Configure I2S parameters
    // Sample rate, bit depth, etc.

    // Initialize ES8311 codec via I2C
    // (codec configuration registers)

    // Enable power amplifier (GPIO46)
    let mut pa_ctrl = Output::new(io.pins.gpio46, Level::High);

    loop {}
}
```

**ESP-IDF (std):**

```rust
use esp_idf_hal::i2s::*;
use esp_idf_hal::peripherals::Peripherals;

fn main() {
    esp_idf_sys::link_patches();

    let peripherals = Peripherals::take().unwrap();

    let i2s_config = I2sConfig::new()
        .sample_rate(44100.Hz())
        .bits_per_sample(I2sBitsPerSample::Bits16)
        .channel_format(I2sChannelFmt::RightLeft)
        .communication_format(I2sCommFormat::I2sStandard)
        .dma_buffer_count(6)
        .dma_buffer_len(1024);

    let i2s = I2sDriver::new_std_tx(
        peripherals.i2s0,
        &i2s_config,
        peripherals.pins.gpio18,  // DOUT
        peripherals.pins.gpio9,   // BCLK
        peripherals.pins.gpio8,   // WS
        peripherals.pins.gpio16,  // MCLK
    ).unwrap();

    // Write audio data
    // i2s.write(&audio_buffer, WAIT).unwrap();
}
```

---

## Code Examples

### Complete Minimal Example (Bare-Metal)

```rust
#![no_std]
#![no_main]

use esp_backtrace as _;
use esp_hal::{
    clock::ClockControl,
    delay::Delay,
    gpio::{Io, Level, Output},
    i2c::I2c,
    peripherals::Peripherals,
    prelude::*,
    system::SystemControl,
};
use esp_println::println;

#[entry]
fn main() -> ! {
    let peripherals = Peripherals::take();
    let system = SystemControl::new(peripherals.SYSTEM);
    let clocks = ClockControl::max(system.clock_control).freeze();

    let io = Io::new(peripherals.GPIO, peripherals.IO_MUX);
    let mut delay = Delay::new(&clocks);

    println!("ESP32-S3 AMOLED Board - Rust Demo");

    // Initialize I2C for peripherals
    let mut i2c = I2c::new(
        peripherals.I2C0,
        io.pins.gpio15,  // SDA
        io.pins.gpio14,  // SCL
        400.kHz(),
        &clocks,
    );

    // Scan I2C devices
    println!("Scanning I2C bus...");
    for addr in 0x08..0x78 {
        if i2c.write(addr, &[]).is_ok() {
            println!("Device found at 0x{:02X}", addr);
        }
    }

    // Initialize GPIO expander
    const TCA9554_ADDR: u8 = 0x24;
    // Configure as outputs (except INT pins)
    i2c.write(TCA9554_ADDR, &[0x03, 0b01001000]).ok();
    // Set initial state (resets high, power enable high)
    i2c.write(TCA9554_ADDR, &[0x01, 0b00000011]).ok();

    println!("GPIO expander initialized");

    // Read IMU WHO_AM_I
    const QMI8658_ADDR: u8 = 0x6B;
    let mut who_am_i = [0u8];
    if i2c.write_read(QMI8658_ADDR, &[0x00], &mut who_am_i).is_ok() {
        println!("QMI8658 WHO_AM_I: 0x{:02X}", who_am_i[0]);
    }

    // Blink LED or read RTC
    loop {
        println!("Loop running...");
        delay.delay_millis(1000);
    }
}
```

### IMU Reading Example

```rust
const QMI8658_ADDR: u8 = 0x6B;

fn init_imu(i2c: &mut I2c) -> Result<(), ()> {
    // Enable accelerometer and gyroscope
    i2c.write(QMI8658_ADDR, &[0x07, 0b11000000]).map_err(|_| ())?;
    Ok(())
}

fn read_accel(i2c: &mut I2c) -> Result<(i16, i16, i16), ()> {
    let mut buffer = [0u8; 6];
    i2c.write_read(QMI8658_ADDR, &[0x35], &mut buffer).map_err(|_| ())?;

    let x = i16::from_le_bytes([buffer[0], buffer[1]]);
    let y = i16::from_le_bytes([buffer[2], buffer[3]]);
    let z = i16::from_le_bytes([buffer[4], buffer[5]]);

    Ok((x, y, z))
}

fn read_gyro(i2c: &mut I2c) -> Result<(i16, i16, i16), ()> {
    let mut buffer = [0u8; 6];
    i2c.write_read(QMI8658_ADDR, &[0x3B], &mut buffer).map_err(|_| ())?;

    let x = i16::from_le_bytes([buffer[0], buffer[1]]);
    let y = i16::from_le_bytes([buffer[2], buffer[3]]);
    let z = i16::from_le_bytes([buffer[4], buffer[5]]);

    Ok((x, y, z))
}

// In main loop:
loop {
    if let Ok((x, y, z)) = read_accel(&mut i2c) {
        println!("Accel: X={}, Y={}, Z={}", x, y, z);
    }
    delay.delay_millis(100);
}
```

### RTC Example

```rust
const PCF85063_ADDR: u8 = 0x51;

fn set_time(i2c: &mut I2c, hour: u8, minute: u8, second: u8) -> Result<(), ()> {
    // Convert to BCD
    let sec_bcd = ((second / 10) << 4) | (second % 10);
    let min_bcd = ((minute / 10) << 4) | (minute % 10);
    let hour_bcd = ((hour / 10) << 4) | (hour % 10);

    i2c.write(PCF85063_ADDR, &[
        0x04,      // Start at seconds register
        sec_bcd,
        min_bcd,
        hour_bcd,
    ]).map_err(|_| ())
}

fn read_time(i2c: &mut I2c) -> Result<(u8, u8, u8), ()> {
    let mut buffer = [0u8; 3];
    i2c.write_read(PCF85063_ADDR, &[0x04], &mut buffer).map_err(|_| ())?;

    // Convert from BCD
    let second = ((buffer[0] >> 4) & 0x07) * 10 + (buffer[0] & 0x0F);
    let minute = ((buffer[1] >> 4) & 0x07) * 10 + (buffer[1] & 0x0F);
    let hour = ((buffer[2] >> 4) & 0x03) * 10 + (buffer[2] & 0x0F);

    Ok((hour, minute, second))
}

// Usage:
set_time(&mut i2c, 14, 30, 0).ok();
loop {
    if let Ok((h, m, s)) = read_time(&mut i2c) {
        println!("Time: {:02}:{:02}:{:02}", h, m, s);
    }
    delay.delay_millis(1000);
}
```

### Touch Reading Example

```rust
const FT3168_ADDR: u8 = 0x38;

struct TouchPoint {
    x: u16,
    y: u16,
    event: u8,
}

fn read_touch(i2c: &mut I2c) -> Result<Vec<TouchPoint, 5>, ()> {
    let mut buffer = [0u8; 16];
    i2c.write_read(FT3168_ADDR, &[0x02], &mut buffer).map_err(|_| ())?;

    let num_points = buffer[0] & 0x0F;
    let mut points = Vec::new();

    for i in 0..num_points.min(5) {
        let base = 1 + (i as usize) * 6;
        let x = (((buffer[base] & 0x0F) as u16) << 8) | buffer[base + 1] as u16;
        let y = (((buffer[base + 2] & 0x0F) as u16) << 8) | buffer[base + 3] as u16;
        let event = (buffer[base] >> 6) & 0x03;

        points.push(TouchPoint { x, y, event }).ok();
    }

    Ok(points)
}

// Usage with interrupt:
let tp_int = Input::new(expander_pin6, Pull::Up);  // Via expander
loop {
    if tp_int.is_low() {
        if let Ok(points) = read_touch(&mut i2c) {
            for point in points.iter() {
                println!("Touch: X={}, Y={}", point.x, point.y);
            }
        }
    }
    delay.delay_millis(10);
}
```

---

## Power Management

### AXP2101 Initialization

```rust
const AXP2101_ADDR: u8 = 0x34;

fn init_axp2101(i2c: &mut I2c) -> Result<(), ()> {
    // Read PMU status
    let mut status = [0u8];
    i2c.write_read(AXP2101_ADDR, &[0x00], &mut status).map_err(|_| ())?;
    println!("AXP2101 Status: 0x{:02X}", status[0]);

    // Enable DC-DC converters if needed
    // Configure LDOs
    // Set battery charge parameters

    Ok(())
}

fn read_battery_voltage(i2c: &mut I2c) -> Result<u16, ()> {
    // Read ADC value from appropriate register
    // Convert to millivolts
    let mut buffer = [0u8; 2];
    i2c.write_read(AXP2101_ADDR, &[0x34], &mut buffer).map_err(|_| ())?;
    let raw = u16::from_be_bytes(buffer);
    let voltage_mv = raw;  // Apply conversion factor
    Ok(voltage_mv)
}
```

### Battery Monitoring

```rust
loop {
    if let Ok(voltage) = read_battery_voltage(&mut i2c) {
        println!("Battery: {}mV", voltage);
    }
    delay.delay_millis(5000);
}
```

---

## Audio Configuration

### ES8311 Codec Setup

```rust
const ES8311_ADDR: u8 = 0x18;

fn init_es8311(i2c: &mut I2c) -> Result<(), ()> {
    // Reset codec
    i2c.write(ES8311_ADDR, &[0x00, 0x1F]).map_err(|_| ())?;
    delay.delay_millis(10);

    // Configure clock
    i2c.write(ES8311_ADDR, &[0x01, 0x30]).map_err(|_| ())?;

    // Configure I2S format
    i2c.write(ES8311_ADDR, &[0x17, 0x18]).map_err(|_| ())?;

    // Set DAC volume
    i2c.write(ES8311_ADDR, &[0x32, 0xBF]).map_err(|_| ())?;

    // Power up
    i2c.write(ES8311_ADDR, &[0x0E, 0x02]).map_err(|_| ())?;

    Ok(())
}

fn enable_speaker(pa_ctrl: &mut Output) {
    pa_ctrl.set_high();
}

fn disable_speaker(pa_ctrl: &mut Output) {
    pa_ctrl.set_low();
}
```

---

## Troubleshooting

### Common Issues

#### 1. **I2C Devices Not Responding**

**Symptoms:** I2C reads/writes fail, devices not detected  
**Solutions:**
- Check I2C address (7-bit vs 8-bit confusion)
- Verify pull-up resistors (2.2kΩ on-board)
- Check I2C speed (try 100 kHz instead of 400 kHz)
- Ensure power is stable (check AXP2101 outputs)
- Verify GPIO expander is initialized (required for some resets)

```rust
// Try slower speed
let mut i2c = I2c::new(
    peripherals.I2C0,
    io.pins.gpio15,
    io.pins.gpio14,
    100.kHz(),  // Slower
    &clocks,
);
```

#### 2. **Display Not Working**

**Symptoms:** Blank screen, no response  
**Solutions:**
- Initialize GPIO expander first (controls LCD_RESET and DSI_PWR_EN)
- Reset display: EXIO0 low -> delay -> high
- Enable display power: EXIO1 high
- Check QSPI connections and clock speed
- Verify SH8601 initialization sequence

```rust
// Proper display reset sequence
expander.set_pin(0, false).unwrap();  // Reset low
delay.delay_millis(10);
expander.set_pin(0, true).unwrap();   // Reset high
delay.delay_millis(120);              // Wait for display ready
expander.set_pin(1, true).unwrap();   // Power enable
```

#### 3. **Touch Not Responding**

**Symptoms:** No touch events detected  
**Solutions:**
- Initialize touch reset via GPIO expander (EXIO2)
- Check FT3168 I2C address (0x38)
- Verify touch interrupt pin (EXIO6)
- Check power supply to touch controller

```rust
// Reset touch controller
expander.set_pin(2, false).unwrap();  // Reset low
delay.delay_millis(10);
expander.set_pin(2, true).unwrap();   // Reset high
delay.delay_millis(200);              // Wait for ready
```

#### 4. **IMU Reads Garbage Data**

**Symptoms:** Invalid WHO_AM_I, incorrect readings  
**Solutions:**
- Verify I2C address (0x6B with SA0=1, 0x6A with SA0=0)
- Check sensor initialization sequence
- Enable accelerometer and gyroscope in control registers
- Allow time for sensor to stabilize after power-on

```rust
// Proper IMU init
let mut who_am_i = [0u8];
i2c.write_read(QMI8658_ADDR, &[0x00], &mut who_am_i).unwrap();
println!("WHO_AM_I: 0x{:02X}", who_am_i[0]);  // Should be 0x05

// Enable sensors
i2c.write(QMI8658_ADDR, &[0x07, 0b00000001]).unwrap();  // Enable accel
i2c.write(QMI8658_ADDR, &[0x08, 0b00000001]).unwrap();  // Enable gyro
delay.delay_millis(100);
```

#### 5. **RTC Time Not Persisting**

**Symptoms:** RTC resets after power cycle  
**Solutions:**
- Check AXP2101 RTC power configuration
- Verify RTC battery backup (optional external battery)
- Ensure RTCLDO is enabled in AXP2101

#### 6. **Flash/Upload Fails**

**Symptoms:** Cannot program board, upload errors  
**Solutions:**
- Hold BOOT button (GPIO0) during power-on
- Check USB connection (native USB on GPIO19/20)
- Try different USB cable/port
- Erase flash: `espflash erase-flash`
- Check for correct target: `xtensa-esp32s3-none-elf` or `xtensa-esp32s3-espidf`

```bash
# Force bootloader mode
espflash flash --monitor --baud 115200 target/xtensa-esp32s3-none-elf/release/my-app

# Erase and reflash
espflash erase-flash
espflash flash --monitor target/xtensa-esp32s3-none-elf/release/my-app
```

#### 7. **PSRAM Not Accessible**

**Symptoms:** Out of memory errors, PSRAM not detected  
**Solutions:**
- ESP32-S3R8 has **embedded** 8MB PSRAM (always available)
- Ensure correct chip variant selected (esp32s3 with R8)
- Check linker configuration for PSRAM usage

#### 8. **Audio Distortion/No Sound**

**Symptoms:** Crackling, silence, distorted audio  
**Solutions:**
- Initialize ES8311 codec via I2C before I2S
- Enable power amplifier (GPIO46 high)
- Check I2S clock configuration (MCLK, BCLK, LRCK)
- Verify codec registers (volume, DAC enable)
- Check speaker connections

```rust
// Enable codec and PA
init_es8311(&mut i2c).unwrap();
let mut pa_ctrl = Output::new(io.pins.gpio46, Level::High);
```

### Debugging Tips

#### Enable Debug Logging

**Bare-Metal:**

```rust
use esp_println::println;

println!("Debug message: {}", value);
```

**ESP-IDF:**

```rust
use log::{info, debug, warn, error};

info!("Application started");
debug!("I2C address: 0x{:02X}", addr);
```

#### I2C Bus Scanning

```rust
println!("Scanning I2C bus...");
for addr in 0x08..0x78 {
    match i2c.write(addr, &[]) {
        Ok(_) => println!("Device at 0x{:02X}", addr),
        Err(_) => {}
    }
}
```

**Expected Devices:**
- 0x18: ES8311
- 0x24: TCA9554 (verify address)
- 0x34: AXP2101
- 0x38: FT3168
- 0x51: PCF85063A
- 0x6B: QMI8658C

#### Measuring I2C Bus Activity

Use logic analyzer or oscilloscope on GPIO14 (SCL) and GPIO15 (SDA).

#### Power Consumption Check

Monitor battery voltage and current via AXP2101 ADC registers.

---

## Additional Resources

### Official Documentation

- **ESP32-S3 Datasheet:** [Espressif ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- **ESP32-S3 Technical Reference Manual:** [ESP32-S3 TRM](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- **Waveshare Wiki:** [ESP32-S3-Touch-AMOLED-1.8](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.8)

### Rust on ESP32 Resources

- **The Rust on ESP Book:** [https://docs.esp-rs.org/](https://docs.esp-rs.org/)
- **esp-rs GitHub Organization:** [https://github.com/esp-rs](https://github.com/esp-rs)
- **esp-hal Documentation:** [https://docs.esp-rs.org/esp-hal/](https://docs.esp-rs.org/esp-hal/)
- **esp-idf-hal Documentation:** [https://docs.rs/esp-idf-hal/](https://docs.rs/esp-idf-hal/)
- **Awesome ESP Rust:** [https://github.com/esp-rs/awesome-esp-rust](https://github.com/esp-rs/awesome-esp-rust)

### Component Datasheets

- **SH8601 Display Driver:** Search for official datasheet
- **FT3168 Touch Controller:** [FocalTech FT3168 Datasheet](https://datasheet4u.com/datasheets/FocalTech/FT3168/)
- **QMI8658C IMU:** [QMI8658C Datasheet](https://qstcorp.com/upload/pdf/202202/QMI8658C%20datasheet%20rev%200.9.pdf)
- **PCF85063A RTC:** [NXP PCF85063A Datasheet](https://www.nxp.com/docs/en/data-sheet/PCF85063A.pdf)
- **AXP2101 PMU:** [AXP2101 Datasheet](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/core/Core2%20v1.1/axp2101.pdf)
- **ES8311 Audio Codec:** Search for Everest Semi ES8311 datasheet
- **TCA9554 I/O Expander:** [Texas Instruments TCA9554 Datasheet](https://www.ti.com/lit/ds/symlink/tca9554.pdf)

### Community and Support

- **ESP32 Forum:** [https://esp32.com/](https://esp32.com/)
- **Rust Embedded Community:** [https://matrix.to/#/#rust-embedded:matrix.org](https://matrix.to/#/#rust-embedded:matrix.org)
- **esp-rs Matrix Chat:** [https://matrix.to/#/#esp-rs:matrix.org](https://matrix.to/#/#esp-rs:matrix.org)

### Example Projects

- **Lilygo AMOLED Examples:** [https://github.com/Xinyuan-LilyGO/T-Display-S3-AMOLED](https://github.com/Xinyuan-LilyGO/T-Display-S3-AMOLED) (similar hardware, C++)
- **ESP32 Rust Examples:** [https://github.com/esp-rs/esp-hal/tree/main/examples](https://github.com/esp-rs/esp-hal/tree/main/examples)

---

## Appendix: Pin Summary Table

| Pin/GPIO | Function | Peripheral | I2C Address | Notes |
|----------|----------|------------|-------------|-------|
| GPIO0 | Boot / PWR Button | System | - | Strapping pin |
| GPIO1 | SD MOSI (D0) | SD Card | - | SDMMC mode |
| GPIO2 | SD SCLK (CLK) | SD Card | - | SDMMC mode |
| GPIO3 | SD MISO (D1) | SD Card | - | SDMMC mode, strapping |
| GPIO4 | QSPI_SIO0 (MOSI) | Display | - | QSPI data line 0 |
| GPIO5 | QSPI_SI1 | Display | - | QSPI data line 1 |
| GPIO6 | QSPI_SI2 | Display | - | QSPI data line 2 |
| GPIO7 | QSPI_SI3 | Display | - | QSPI data line 3 |
| GPIO8 | I2S_LRCK | Audio | - | Word select |
| GPIO9 | I2S_SCLK | Audio | - | Bit clock |
| GPIO10 | INT (QMI/RTC) | IMU/RTC | - | Shared interrupt |
| GPIO11 | LCD_CS | Display | - | Chip select |
| GPIO12 | QSPI_SCL | Display | - | QSPI clock |
| GPIO13 | LCD_TE | Display | - | Tearing effect |
| GPIO14 | I2C_SCL | I2C Bus | - | Shared SCL for all I2C devices |
| GPIO15 | I2C_SDA | I2C Bus | - | Shared SDA for all I2C devices |
| GPIO16 | I2S_MCLK | Audio | - | Master clock |
| GPIO17 | I2S_ASDOUT | Audio | - | Data out from codec |
| GPIO18 | I2S_DSDIN | Audio | - | Data in to codec |
| GPIO19 | USB_D- | USB | - | Native USB |
| GPIO20 | USB_D+ | USB | - | Native USB |
| GPIO21 | PWRON | Power | - | Power button / chip enable |
| GPIO38 | GPIO | Available | - | General purpose |
| GPIO39 | GPIO | Available | - | General purpose |
| GPIO40 | AXP_IRQ | Power | - | PMU interrupt |
| GPIO41 | GPIO | Available | - | General purpose |
| GPIO42 | GPIO | Available | - | General purpose |
| GPIO43 | U0TXD | UART | - | Debug TX |
| GPIO44 | U0RXD | UART | - | Debug RX |
| GPIO45 | Codec_CE | Audio | - | Codec chip enable |
| GPIO46 | PA_CTRL | Audio | - | Power amplifier control |
| EXIO0 | LCD_RESET | Display | 0x24 (TCA9554) | Via GPIO expander |
| EXIO1 | DSI_PWR_EN | Display | 0x24 (TCA9554) | Via GPIO expander |
| EXIO2 | TP_RESET | Touch | 0x24 (TCA9554) | Via GPIO expander |
| EXIO3 | QMI_INT2 | IMU | 0x24 (TCA9554) | Via GPIO expander |
| EXIO6 | TP_INT | Touch | 0x24 (TCA9554) | Via GPIO expander |
| EXIO7 | SDCS | SD Card | 0x24 (TCA9554) | Via GPIO expander |

---

## License

This document is provided for informational and educational purposes. All trademarks and product names are the property of their respective owners. Refer to component datasheets and official documentation for authoritative technical specifications.

---

**Document End**
