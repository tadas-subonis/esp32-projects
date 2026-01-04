#![no_std]
#![no_main]
#![deny(
    clippy::mem_forget,
    reason = "mem::forget is generally not safe to do with esp_hal types, especially those \
    holding buffers for the duration of a data transfer."
)]
#![deny(clippy::large_stack_frames)]

use embassy_executor::Spawner;
use embassy_time::{Duration, Instant, Timer};
use embedded_hal_bus::i2c::RefCellDevice;
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
    gpio::{Input, InputConfig, Pull},
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

fn axp2101_power_off<I2C>(i2c: &mut I2C) -> Result<(), I2C::Error>
where
    I2C: embedded_hal::i2c::I2c,
{
    // AXP2101 PMU on this board typically uses 7-bit I2C address 0x34.
    //
    // Datasheet: writing bit0 in REG 0x10 triggers a power-off sequence.
    const AXP2101_ADDR: u8 = 0x34;
    const REG_POWER_OFF: u8 = 0x10;

    let mut reg10 = [0u8; 1];
    i2c.write_read(AXP2101_ADDR, &[REG_POWER_OFF], &mut reg10)?;
    reg10[0] |= 0x01;
    i2c.write(AXP2101_ADDR, &[REG_POWER_OFF, reg10[0]])?;
    Ok(())
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

    let i2c_bus = core::cell::RefCell::new(i2c);

    esp_println::println!("BOOT: probing TCA9554");
    let reset = WsTca9554Reset::new(RefCellDevice::new(&i2c_bus));
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

    // --- Buttons: power off when BOTH are clicked/held together ---
    //
    // On this board:
    // - GPIO0 is the BOOT / PWR button (active-low, strapping pin).
    // - GPIO21 is labeled PWRON in the board doc (active-low when pressed on some revisions).
    //
    // We treat "click both buttons" as "hold both low for a short time" to avoid false triggers.
    let btn_cfg = InputConfig::default().with_pull(Pull::Up);
    let btn_boot = Input::new(peripherals.GPIO0, btn_cfg);
    let btn_pwron = Input::new(peripherals.GPIO21, btn_cfg);
    let mut pmu_i2c = RefCellDevice::new(&i2c_bus);

    esp_println::println!("BTN: initialized GPIO0 (boot) and GPIO21 (pwron) with pull-up");

    const BOTH_HOLD: Duration = Duration::from_millis(500);
    const POLL: Duration = Duration::from_millis(20);
    const LOG_INTERVAL: Duration = Duration::from_millis(1000);
    const PROGRESS_LOG_INTERVAL: Duration = Duration::from_millis(100);
    let mut both_low_since: Option<Instant> = None;
    let mut consecutive_both_low = 0u32;
    let mut last_log = Instant::now();
    let mut last_progress_log = Instant::now();
    let mut last_boot_state = btn_boot.is_low();
    let mut last_pwron_state = btn_pwron.is_low();

    // Log initial button states
    esp_println::println!("BTN: initial state - boot={} pwron={}", 
        if last_boot_state { "LOW" } else { "HIGH" },
        if last_pwron_state { "LOW" } else { "HIGH" });

    loop {
        let boot_low = btn_boot.is_low();
        let pwron_low = btn_pwron.is_low();
        let pwron_high = btn_pwron.is_high();

        // Log state changes immediately
        if boot_low != last_boot_state {
            esp_println::println!("BTN: boot state changed: {} -> {}", 
                if last_boot_state { "LOW" } else { "HIGH" },
                if boot_low { "LOW" } else { "HIGH" });
            last_boot_state = boot_low;
        }
        if pwron_low != last_pwron_state {
            esp_println::println!("BTN: pwron state changed: {} -> {} (high={})", 
                if last_pwron_state { "LOW" } else { "HIGH" },
                if pwron_low { "LOW" } else { "HIGH" },
                pwron_high);
            last_pwron_state = pwron_low;
        }

        // Log button states periodically for debugging
        if Instant::now().duration_since(last_log) >= LOG_INTERVAL {
            esp_println::println!(
                "BTN: boot={} pwron={} (low={} high={}) consecutive={}",
                if boot_low { "LOW" } else { "HIGH" },
                if pwron_low { "LOW" } else { "HIGH" },
                pwron_low,
                pwron_high,
                consecutive_both_low
            );
            last_log = Instant::now();
        }

        // Try both active-low (both buttons low) and also check if GPIO21 might be active-high
        // Some board revisions might have GPIO21 as active-high (goes HIGH when pressed)
        let both_pressed = (boot_low && pwron_low) || (boot_low && pwron_high);
        
        if both_pressed {
            consecutive_both_low += 1;
            
            // Log which condition matched
            if consecutive_both_low == 1 {
                if boot_low && pwron_low {
                    esp_println::println!("BTN: both buttons LOW detected (active-low mode)");
                } else if boot_low && pwron_high {
                    esp_println::println!("BTN: boot LOW + pwron HIGH detected (GPIO21 active-high mode)");
                }
            }
            
            // Only start the timer after we've seen both pressed for a few polls (debounce)
            if consecutive_both_low >= 3 {
                let since = both_low_since.get_or_insert_with(|| {
                    esp_println::println!("BTN: both buttons confirmed, starting hold timer");
                    Instant::now()
                });
                let held_duration = Instant::now().duration_since(*since);
                
                // Log progress every 100ms
                if Instant::now().duration_since(last_progress_log) >= PROGRESS_LOG_INTERVAL {
                    esp_println::println!("BTN: holding... {:?} / {:?}", held_duration, BOTH_HOLD);
                    last_progress_log = Instant::now();
                }
                
                if held_duration >= BOTH_HOLD {
                    esp_println::println!("PWR: both buttons held for {:?} -> power off", held_duration);
                    match axp2101_power_off(&mut pmu_i2c) {
                        Ok(()) => {
                            esp_println::println!("PWR: power off command sent");
                            // If power off succeeds, execution should stop shortly after.
                            loop {
                                Timer::after(Duration::from_secs(1)).await;
                            }
                        }
                        Err(e) => {
                            esp_println::println!("PWR: AXP2101 power off failed: {:?}", e);
                            // Fall back to a tight loop (better than continuing unexpectedly).
                            loop {
                                Timer::after(Duration::from_secs(1)).await;
                            }
                        }
                    }
                }
            } else if consecutive_both_low == 2 {
                esp_println::println!("BTN: debouncing... ({}/3)", consecutive_both_low);
            }
        } else {
            // Reset counters if both buttons aren't pressed
            if consecutive_both_low > 0 {
                esp_println::println!("BTN: button state changed, resetting (boot={} pwron_low={} pwron_high={})", 
                    if boot_low { "LOW" } else { "HIGH" },
                    pwron_low,
                    pwron_high);
            }
            consecutive_both_low = 0;
            both_low_since = None;
        }

        Timer::after(POLL).await;
    }

    // for inspiration have a look at the examples at https://github.com/esp-rs/esp-hal/tree/esp-hal-v~1.0/examples
}
