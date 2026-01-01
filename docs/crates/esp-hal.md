# `esp-hal` (~1.0)

## What it does in this repo

Hardware abstraction layer for ESP32‑C3 peripherals + clocks/timers/interrupts.

Used directly in `src/bin/main.rs` for:

- `esp_hal::Config` (CPU clock config)
- `esp_hal::init(...)` (take peripherals)
- `esp_hal::timer::timg::TimerGroup` (timer used to start Embassy via `esp-rtos`)
- software interrupts (`esp_hal::interrupt::software::SoftwareInterruptControl`)

## Key links

- API docs (docs.rs): https://docs.rs/esp-hal/
- Espressif-hosted chip docs (handy for target-specific items): https://docs.espressif.com/projects/rust/
- Project repo: https://github.com/esp-rs/esp-hal
- Examples: https://github.com/esp-rs/esp-hal/tree/main/examples

Useful module docs on `docs.rs`:

- GPIO: https://docs.rs/esp-hal/latest/esp_hal/gpio/index.html
- Timers: https://docs.rs/esp-hal/latest/esp_hal/timer/index.html
- Clocks: https://docs.rs/esp-hal/latest/esp_hal/clock/index.html
- Interrupts: https://docs.rs/esp-hal/latest/esp_hal/interrupt/index.html

## APIs you’ll likely use first

- **Initialization**
  - `esp_hal::Config::default()`
  - `Config::with_cpu_clock(...)`
  - `esp_hal::init(config)` → `peripherals`
- **Clocks**
  - `esp_hal::clock::CpuClock::max()`
- **Timers**
  - `esp_hal::timer::timg::TimerGroup::new(peripherals.TIMG0)`
- **Interrupt plumbing**
  - `esp_hal::interrupt::software::SoftwareInterruptControl::new(peripherals.SW_INTERRUPT)`
- **RAM placement attributes** (used by `esp-alloc` macro)
  - `#[esp_hal::ram(reclaimed)]` (reclaimed RAM heap)

## Async note

Rust-on-ESP notes that many `esp-hal` drivers are constructed in blocking mode and can be converted to async via `into_async`; async drivers are often `!Send` due to core/interrupt binding:
https://docs.espressif.com/projects/rust/book/application-development/async.html

## How to use it (practical patterns)

### 1) Initialize peripherals

This is the standard `esp-hal` bring-up pattern used in this repo:

```rust
use esp_hal::clock::CpuClock;

let config = esp_hal::Config::default().with_cpu_clock(CpuClock::max());
let peripherals = esp_hal::init(config);
```

### 2) GPIO: create an output pin

From the GPIO module docs, the main “user-facing” types include `Io`, `Output`, and `Level`:
https://docs.rs/esp-hal/latest/esp_hal/gpio/index.html

```rust
use esp_hal::gpio::{Io, Level, Output};

let io = Io::new(peripherals.GPIO, peripherals.IO_MUX);
let mut led = Output::new(io.pins.gpio2, Level::Low); // pick the correct pin for your board

// led.set_high();
// led.set_low();
// led.toggle();
```

### 3) Timers: general-purpose timers (TIMG)

The timer module shows how to wrap a hardware timer into a one-shot or periodic timer:
https://docs.rs/esp-hal/latest/esp_hal/timer/index.html

```rust
use esp_hal::timer::{OneShotTimer, PeriodicTimer};
use esp_hal::timer::timg::TimerGroup;
use embassy_time::Duration;

let timg0 = TimerGroup::new(peripherals.TIMG0);

let mut one_shot = OneShotTimer::new(timg0.timer0);
one_shot.delay_millis(500);

let timg0 = TimerGroup::new(peripherals.TIMG0);
let mut periodic = PeriodicTimer::new(timg0.timer0);
periodic.start(Duration::from_secs(1));
loop {
    periodic.wait();
}
```

## Complete API inventory

**Note**: `esp-hal` is a very large crate (1000+ items). 

Full API index: See local docs at `target/riscv32imc-unknown-none-elf/doc/esp_hal/all.html`

**This is a curated list organized by module. For exhaustive coverage, see the local docs.**

### Core Entrypoints
- **`init(config)`**: Takes ownership of peripherals and applies global configuration.
- **`Config`**: Global configuration (clocks, etc.).
- **`peripherals::Peripherals`**: The singleton peripheral container returned by `init`.

### Concurrency/Driver Modes
- **`Blocking`**: Marker type for blocking driver mode.
- **`Async`**: Marker type for async driver mode.

### Key Modules and Types

#### `esp_hal::gpio` (Digital I/O)
- **`Io`**: GPIO I/O controller.
- **`Input`**: Input pin.
- **`Output`**: Output pin.
- **`InputOutput`**: Bidirectional pin.
- **`Level`**: Pin level (High, Low).
- **`Pull`**: Pull-up/pull-down configuration.
- **`Drive`**: Drive strength configuration.
- **`Function`**: Pin function/mux configuration.

#### `esp_hal::timer` (Timers)
- **`PeriodicTimer`**: Periodic timer.
- **`OneShotTimer`**: One-shot timer.
- **`timg::TimerGroup`**: Timer group (TIMG).
- **`systimer::SystemTimer`**: System timer (SYSTIMER).
- **`systimer::Alarm`**: System timer alarm.
- **`systimer::Target`**: System timer target.

#### `esp_hal::clock` (Clock Configuration)
- **`CpuClock`**: CPU clock speed enum.
- **`ClockControl`**: Clock control handle.
- **`Clock`**: Clock type.
- **`XtalClock`**: Crystal clock configuration.
- **`PllClock`**: PLL clock configuration.

#### `esp_hal::interrupt` (Interrupts)
- **`Interrupt`**: Interrupt number enum.
- **`enable`**: Enable an interrupt.
- **`disable`**: Disable an interrupt.
- **`software::SoftwareInterruptControl`**: Software interrupt controller.
- **`software::SoftwareInterrupt`**: Software interrupt handle.

#### `esp_hal::uart` (UART)
- **`Uart`**: UART peripheral driver.
- **`UartRx`**: UART receiver.
- **`UartTx`**: UART transmitter.
- **`UartRxTx`**: UART receiver+transmitter.
- **`Config`**: UART configuration.
- **`DataBits`**: Data bits configuration.
- **`Parity`**: Parity configuration.
- **`StopBits`**: Stop bits configuration.

#### `esp_hal::spi` (SPI)
- **`Spi`**: SPI peripheral driver.
- **`SpiDevice`**: SPI device handle.
- **`SpiBus`**: SPI bus handle.
- **`Config`**: SPI configuration.
- **`Mode`**: SPI mode (CPOL/CPHA).
- **`DutyCycle`**: SPI duty cycle.
- **`BitOrder`**: Bit order (MSB/LSB).

#### `esp_hal::i2c` (I2C)
- **`I2c`**: I2C peripheral driver.
- **`I2cBus`**: I2C bus handle.
- **`Config`**: I2C configuration.
- **`ClockSpeed`**: I2C clock speed.

#### `esp_hal::adc` (ADC - Analog-to-Digital)
- **`Adc`**: ADC peripheral driver.
- **`AdcPin`**: ADC pin.
- **`Resolution`**: ADC resolution.
- **`Attenuation`**: ADC attenuation.

#### `esp_hal::dac` (DAC - Digital-to-Analog)
- **`Dac`**: DAC peripheral driver.
- **`DacChannel`**: DAC channel.

#### `esp_hal::ledc` (LED PWM Controller)
- **`LedcTimer`**: LEDC timer.
- **`LedcChannel`**: LEDC channel.
- **`Resolution`**: LEDC resolution.
- **`ClockSource`**: LEDC clock source.

#### `esp_hal::rmt` (Remote Control)
- **`Rmt`**: RMT peripheral driver.
- **`RmtChannel`**: RMT channel.
- **`Pulse`**: RMT pulse.

#### `esp_hal::usb_serial_jtag` (USB Serial JTAG)
- **`UsbSerialJtag`**: USB Serial JTAG driver.

#### `esp_hal::systimer` (System Timer)
- **`SystemTimer`**: System timer.
- **`Alarm`**: System timer alarm.
- **`Target`**: System timer target.

#### `esp_hal::peripherals` (Peripheral Access)
- **`Peripherals`**: Peripheral container.
- Individual peripheral types (e.g., `GPIO`, `UART0`, `SPI2`, `I2C0`, `TIMG0`, etc.).

#### `esp_hal::ram` (RAM Attributes)
- **`#[ram(reclaimed)]`**: Attribute for reclaimed RAM.
- **`#[ram(psram)]`**: Attribute for PSRAM.

#### `esp_hal::reset` (Reset)
- **`Reset`**: Reset controller.
- **`ResetReason`**: Reset reason enum.

#### `esp_hal::rtc` (RTC - Real-Time Clock)
- **`Rtc`**: RTC peripheral.
- **`DateTime`**: RTC date/time.

#### `esp_hal::efuse` (eFuse)
- **`Efuse`**: eFuse peripheral.
- Various eFuse field accessors.

#### `esp_hal::flash` (Flash)
- **`Flash`**: Flash peripheral.
- **`FlashSize`**: Flash size.

#### `esp_hal::dma` (DMA)
- **`Dma`**: DMA controller.
- **`DmaChannel`**: DMA channel.
- **`DmaTransfer`**: DMA transfer.

#### `esp_hal::soc` (SoC-specific)
- SoC-specific types and constants.

### Additional Modules
- **`esp_hal::rng`**: Random number generator.
- **`esp_hal::sha`**: SHA hashing.
- **`esp_hal::aes`**: AES encryption.
- **`esp_hal::rsa`**: RSA encryption.
- **`esp_hal::hmac`**: HMAC.
- **`esp_hal::ds`**: Digital signature.
- **`esp_hal::temperature_sensor`**: Temperature sensor.
- **`esp_hal::touch`**: Touch sensor (ESP32-S2/S3).
- **`esp_hal::twai`**: Two-Wire Automotive Interface (CAN).
- **`esp_hal::parl_io`**: Parallel I/O (ESP32-S3).
- **`esp_hal::gdma`**: General DMA (ESP32-S3).

**For exhaustive coverage, see**: https://docs.rs/esp-hal/latest/esp_hal/all.html


