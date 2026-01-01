#![no_std]
#![no_main]
#![deny(
    clippy::mem_forget,
    reason = "mem::forget is generally not safe to do with esp_hal types, especially those \
    holding buffers for the duration of a data transfer."
)]
#![deny(clippy::large_stack_frames)]

use embassy_executor::Spawner;
use embassy_time::{Duration, Timer};
use embedded_graphics::{
    mono_font::{MonoTextStyle, ascii::FONT_10X20},
    pixelcolor::Rgb888,
    prelude::*,
    text::Text,
};
use esp_backtrace as _;
use esp_hal::{
    clock::CpuClock,
    delay::Delay,
    dma::{DmaRxBuf, DmaTxBuf},
    dma_buffers,
    i2c::master::{BusTimeout, Config as I2cConfig, I2c, SoftwareTimeout},
    spi::{
        Mode,
        master::{Config as SpiConfig, Spi},
    },
    time::Rate,
    timer::timg::TimerGroup,
};
use sh8601_rs::{
    ColorMode, DisplaySize, ResetInterface, Sh8601Driver, Ws18AmoledDriver, framebuffer_size,
    DMA_CHUNK_SIZE,
};

// Waveshare ESP32-S3 Touch AMOLED 1.8" uses a TCA9554 I/O expander (EXIO0..7) on the shared I2C
// bus to control critical reset/power signals.
//
// See: docs/devices/waveshare-esp32-s3-touch-amoled-1.8.md
// TCA9554 base address is 0x20; many boards strap it to 0x20, some to 0x24.
// We'll probe both to avoid "works on my board" failures.
const TCA9554_ADDR_PRIMARY: u8 = 0x24;
const TCA9554_ADDR_FALLBACK: u8 = 0x20;
const TCA9554_OUTPUT: u8 = 0x01;
const TCA9554_POLARITY: u8 = 0x02;
const TCA9554_CONFIG: u8 = 0x03; // 0 = output, 1 = input

/// Minimal ResetInterface implementation for the Waveshare board:
/// - EXIO0: LCD_RESET (output)
/// - EXIO1: DSI_PWR_EN (output, display power enable)
/// - EXIO2: TP_RESET (output, keep high so touch isn't held in reset)
/// - EXIO3: QMI_INT2 (input)
/// - EXIO6: TP_INT (input)
/// - EXIO7: SDCS (output, keep high to deselect SD card)
struct WsTca9554Reset<I2C> {
    i2c: I2C,
    addr: u8,
}

impl<I2C> WsTca9554Reset<I2C> {
    fn new(mut i2c: I2C) -> Self
    where
        I2C: embedded_hal::i2c::I2c,
    {
        // Probe the expander address. With I2C timeouts enabled (see I2C config),
        // this will return quickly even if the bus is stuck or the address doesn't ACK.
        let primary_res = i2c.write(TCA9554_ADDR_PRIMARY, &[TCA9554_POLARITY, 0x00]);
        esp_println::println!("TCA9554 probe 0x{:02X}: {:?}", TCA9554_ADDR_PRIMARY, primary_res);

        let fallback_res = i2c.write(TCA9554_ADDR_FALLBACK, &[TCA9554_POLARITY, 0x00]);
        esp_println::println!(
            "TCA9554 probe 0x{:02X}: {:?}",
            TCA9554_ADDR_FALLBACK, fallback_res
        );

        let addr = if primary_res.is_ok() {
            TCA9554_ADDR_PRIMARY
        } else if fallback_res.is_ok() {
            TCA9554_ADDR_FALLBACK
        } else {
            // Neither ACKed; keep using the primary address so later operations
            // fail consistently, and we have logs explaining why.
            TCA9554_ADDR_PRIMARY
        };

        esp_println::println!("TCA9554: using I2C addr 0x{:02X}", addr);
        Self { i2c, addr }
    }
}

impl<I2C> ResetInterface for WsTca9554Reset<I2C>
where
    I2C: embedded_hal::i2c::I2c,
{
    type Error = I2C::Error;

    fn reset(&mut self) -> Result<(), Self::Error> {
        let delay = Delay::new();

        // Configure directions: EXIO3 and EXIO6 as inputs; the rest as outputs.
        // This matches the device doc's recommended default.
        self.i2c
            .write(self.addr, &[TCA9554_CONFIG, 0b0100_1000])?;
        // No polarity inversion.
        self.i2c.write(self.addr, &[TCA9554_POLARITY, 0x00])?;

        // Assert display reset low while keeping display power enabled and SD deselected.
        // Bits: EXIO7 SDCS=1, EXIO2 TP_RESET=1, EXIO1 DSI_PWR_EN=1, EXIO0 LCD_RESET=0
        self.i2c
            .write(self.addr, &[TCA9554_OUTPUT, 0b1000_0110])?;
        delay.delay_millis(20);

        // De-assert reset (high). Keep power enabled.
        self.i2c
            .write(self.addr, &[TCA9554_OUTPUT, 0b1000_0111])?;
        delay.delay_millis(150);

        Ok(())
    }
}

extern crate alloc;

// This creates a default app-descriptor required by the esp-idf bootloader.
// For more information see: <https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/app_image_format.html#application-description>
esp_bootloader_esp_idf::esp_app_desc!();

#[allow(
    clippy::large_stack_frames,
    reason = "it's not unusual to allocate larger buffers etc. in main"
)]
#[esp_rtos::main]
async fn main(spawner: Spawner) -> ! {
    // generator version: 1.1.0

    esp_println::logger::init_logger_from_env();
    esp_println::println!("BOOT: starting");

    let config = esp_hal::Config::default().with_cpu_clock(CpuClock::max());
    let peripherals = esp_hal::init(config);

    // Waveshare ESP32-S3 Touch AMOLED has PSRAM; put the framebuffer there.
    esp_alloc::psram_allocator!(peripherals.PSRAM, esp_hal::psram);
    esp_println::println!("BOOT: after psram_allocator");

    let timg0 = TimerGroup::new(peripherals.TIMG0);
    esp_println::println!("BOOT: TimerGroup ready, starting esp-rtos");
    // `esp-rtos` needs a software interrupt only on RISC-V (ESP32-C3/C6/etc).
    // On Xtensa (ESP32-S3), `start` only takes the timer source.
    #[cfg(target_arch = "riscv32")]
    {
        let sw_interrupt =
            esp_hal::interrupt::software::SoftwareInterruptControl::new(peripherals.SW_INTERRUPT);
        esp_rtos::start(timg0.timer0, sw_interrupt.software_interrupt0);
    }
    #[cfg(not(target_arch = "riscv32"))]
    {
        esp_rtos::start(timg0.timer0);
    }
    esp_println::println!("BOOT: esp-rtos started");

    let _ = spawner;

    // --- DMA buffers for QSPI ---
    esp_println::println!("BOOT: allocating DMA buffers");
    let (rx_buffer, rx_descriptors, tx_buffer, tx_descriptors) = dma_buffers!(DMA_CHUNK_SIZE);
    let dma_rx_buf = DmaRxBuf::new(rx_descriptors, rx_buffer).unwrap();
    let dma_tx_buf = DmaTxBuf::new(tx_descriptors, tx_buffer).unwrap();

    // --- QSPI wiring (from schematic / sh8601-rs Waveshare example) ---
    // QSPI bus:
    //   SIO0..3: GPIO4..7
    //
    // NOTE: A known-working public repo for this board uses CS=GPIO12 and SCK=GPIO11.
    // We'll match that here (if your board revision differs, we can make this configurable).
    //   CS:      GPIO12
    //   SCK:     GPIO11
    esp_println::println!("BOOT: init QSPI");
    let lcd_spi = Spi::new(
        peripherals.SPI2,
        SpiConfig::default()
            .with_frequency(Rate::from_mhz(40))
            .with_mode(Mode::_0),
    )
    .unwrap()
    .with_sio0(peripherals.GPIO4)
    .with_sio1(peripherals.GPIO5)
    .with_sio2(peripherals.GPIO6)
    .with_sio3(peripherals.GPIO7)
    .with_cs(peripherals.GPIO12)
    .with_sck(peripherals.GPIO11)
    .with_dma(peripherals.DMA_CH0)
    .with_buffers(dma_rx_buf, dma_tx_buf);
    esp_println::println!("BOOT: QSPI ready");

    // I2C GPIO expander (TCA9554 @ 0x24 typical) controls LCD_RESET/DSI_PWR_EN/TP_RESET:
    //   SDA: GPIO15
    //   SCL: GPIO14
    esp_println::println!("BOOT: init I2C");
    let i2c = I2c::new(
        peripherals.I2C0,
        // IMPORTANT: enable timeouts so a missing/stuck I2C device doesn't hang the whole boot.
        // Default on some chips is "bus timeout disabled".
        I2cConfig::default()
            .with_frequency(Rate::from_khz(400))
            .with_timeout(BusTimeout::BusCycles(50))
            .with_software_timeout(SoftwareTimeout::Transaction(esp_hal::time::Duration::from_millis(50))),
    )
    .unwrap()
    .with_sda(peripherals.GPIO15)
    .with_scl(peripherals.GPIO14);
    esp_println::println!("BOOT: I2C ready");

    esp_println::println!("BOOT: probing TCA9554");
    let reset = WsTca9554Reset::new(i2c);
    esp_println::println!("BOOT: TCA9554 ready");
    let ws_driver = Ws18AmoledDriver::new(lcd_spi);

    const DISPLAY_SIZE: DisplaySize = DisplaySize::new(368, 448);
    const FB_SIZE: usize = framebuffer_size(DISPLAY_SIZE, ColorMode::Rgb888);

    let delay = Delay::new();
    esp_println::println!("Display: init...");
    let display_res = Sh8601Driver::new_heap::<_, FB_SIZE>(
        ws_driver,
        reset,
        ColorMode::Rgb888,
        DISPLAY_SIZE,
        delay,
    );
    let mut display = match display_res {
        Ok(d) => d,
        Err(e) => {
            esp_println::println!("Display init failed: {:?}", e);
            loop {
                Timer::after(Duration::from_secs(1)).await;
            }
        }
    };

    display.clear(Rgb888::BLACK).unwrap();
    let style = MonoTextStyle::new(&FONT_10X20, Rgb888::WHITE);
    Text::new("Hello World", Point::new(20, 40), style)
        .draw(&mut display)
        .unwrap();
    display.flush().unwrap();

    esp_println::println!("BOOT: ok (hello world drawn)");

    loop {
        Timer::after(Duration::from_secs(1)).await;
    }

    // for inspiration have a look at the examples at https://github.com/esp-rs/esp-hal/tree/esp-hal-v~1.0/examples
}
