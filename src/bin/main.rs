#![no_std]
#![no_main]
#![deny(
    clippy::mem_forget,
    reason = "mem::forget is generally not safe to do with esp_hal types, especially those \
    holding buffers for the duration of a data transfer."
)]
#![deny(clippy::large_stack_frames)]

extern crate alloc;

use alloc::rc::Rc;
use bevy_ecs::prelude::*;
use embassy_executor::Spawner;
use embassy_time::{Duration, Timer};
use esp_backtrace as _;
use esp_hal::{
    clock::CpuClock,
    delay::Delay,
    dma::{DmaRxBuf, DmaTxBuf},
    dma_buffers,
    gpio::{Input, InputConfig, Pull},
    i2c::master::{BusTimeout, Config as I2cConfig, I2c, SoftwareTimeout},
    rng::Rng,
    spi::{master::Config as SpiConfig, Mode},
    time::Rate,
    timer::timg::TimerGroup,
};
use sh8601_rs::{ColorMode, DisplaySize, Sh8601Driver, Ws18AmoledDriver, framebuffer_size, DMA_CHUNK_SIZE};

use waveshare_esp32_sandbox_1::config::{
    AXP2101_ADDR, AXP2101_INTEN1, AXP2101_INTEN2, AXP2101_INTEN3, AXP2101_INTSTS1, AXP2101_INTSTS2,
    AXP2101_INTSTS3, AXP2101_PKEY_SHORT_IRQ_BIT, TCA9554_ADDR_FALLBACK, TCA9554_ADDR_PRIMARY,
    TCA9554_POLARITY,
};
use waveshare_esp32_sandbox_1::display::FrameBufferResource;
use waveshare_esp32_sandbox_1::game::{
    animate_food_system, collision_system, food_spawn_system, input_system, move_snake_system,
    process_input_system, render_system, restart_system, ButtonState, Direction, GameState,
    HighScore, InputState, Position, RngResource, SnakeHead, SnakeSegment,
};
use waveshare_esp32_sandbox_1::perf::PerformanceMetrics;
use waveshare_esp32_sandbox_1::hardware::{
    Axp2101Resource, ButtonLeftResource, DisplayResource, SharedTca9554Reset,
};

// This creates a default app-descriptor required by the esp-idf bootloader.
esp_bootloader_esp_idf::esp_app_desc!();

#[allow(
    clippy::large_stack_frames,
    reason = "it's not unusual to allocate larger buffers etc. in main"
)]
#[esp_rtos::main]
async fn main(spawner: Spawner) -> ! {
    esp_println::logger::init_logger_from_env();
    esp_println::println!("BOOT: starting snake game");

    let config = esp_hal::Config::default().with_cpu_clock(CpuClock::max());
    let peripherals = esp_hal::init(config);

    esp_alloc::psram_allocator!(peripherals.PSRAM, esp_hal::psram);
    esp_println::println!("BOOT: after psram_allocator");

    let timg0 = TimerGroup::new(peripherals.TIMG0);
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

    // --- QSPI wiring ---
    esp_println::println!("BOOT: init QSPI");
    let lcd_spi = esp_hal::spi::master::Spi::new(
        peripherals.SPI2,
        SpiConfig::default()
            .with_frequency(Rate::from_mhz(60)) // Increased from 40MHz for better performance
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

    // --- I2C ---
    esp_println::println!("BOOT: init I2C");
    let i2c = I2c::new(
        peripherals.I2C0,
        I2cConfig::default()
            .with_frequency(Rate::from_khz(400))
            .with_timeout(BusTimeout::BusCycles(50))
            .with_software_timeout(SoftwareTimeout::Transaction(
                esp_hal::time::Duration::from_millis(50),
            )),
    )
    .unwrap()
    .with_sda(peripherals.GPIO15)
    .with_scl(peripherals.GPIO14);
    esp_println::println!("BOOT: I2C ready");

    // Use StaticCell to make i2c_bus live for 'static
    // Use Rc to share the I2C bus between reset interface and button reading
    static I2C_BUS: static_cell::StaticCell<
        Rc<core::cell::RefCell<I2c<'static, esp_hal::Blocking>>>,
    > = static_cell::StaticCell::new();
    let i2c_bus = I2C_BUS.init(Rc::new(core::cell::RefCell::new(i2c)));
    esp_println::println!("BOOT: probing TCA9554");

    // Probe TCA9554 to find the correct address
    let tca9554_addr = {
        let mut i2c_ref = i2c_bus.borrow_mut();
        let primary_res = i2c_ref.write(TCA9554_ADDR_PRIMARY, &[TCA9554_POLARITY, 0x00]);
        esp_println::println!(
            "TCA9554 probe 0x{:02X}: {:?}",
            TCA9554_ADDR_PRIMARY,
            primary_res
        );

        let fallback_res = i2c_ref.write(TCA9554_ADDR_FALLBACK, &[TCA9554_POLARITY, 0x00]);
        esp_println::println!(
            "TCA9554 probe 0x{:02X}: {:?}",
            TCA9554_ADDR_FALLBACK,
            fallback_res
        );

        if primary_res.is_ok() {
            TCA9554_ADDR_PRIMARY
        } else if fallback_res.is_ok() {
            TCA9554_ADDR_FALLBACK
        } else {
            TCA9554_ADDR_PRIMARY // Default fallback
        }
    };
    esp_println::println!("TCA9554: using I2C addr 0x{:02X}", tca9554_addr);

    // --- Initialize AXP2101 PMU for power button ---
    esp_println::println!("BOOT: init AXP2101 PMU");
    {
        let mut i2c_ref = i2c_bus.borrow_mut();

        // Probe AXP2101
        let mut chip_id = [0u8; 1];
        if i2c_ref
            .write_read(AXP2101_ADDR, &[0x03], &mut chip_id)
            .is_ok()
        {
            esp_println::println!("AXP2101: chip ID = 0x{:02X}", chip_id[0]);
        } else {
            esp_println::println!("AXP2101: probe failed (will continue anyway)");
        }

        // Disable all IRQs first
        let _ = i2c_ref.write(AXP2101_ADDR, &[AXP2101_INTEN1, 0x00]);
        let _ = i2c_ref.write(AXP2101_ADDR, &[AXP2101_INTEN2, 0x00]);
        let _ = i2c_ref.write(AXP2101_ADDR, &[AXP2101_INTEN3, 0x00]);

        // Clear any pending interrupts
        let _ = i2c_ref.write(AXP2101_ADDR, &[AXP2101_INTSTS1, 0xFF]);
        let _ = i2c_ref.write(AXP2101_ADDR, &[AXP2101_INTSTS2, 0xFF]);
        let _ = i2c_ref.write(AXP2101_ADDR, &[AXP2101_INTSTS3, 0xFF]);

        // Enable PKEY short press IRQ (bit 3 of INTEN2)
        if i2c_ref
            .write(AXP2101_ADDR, &[AXP2101_INTEN2, AXP2101_PKEY_SHORT_IRQ_BIT])
            .is_ok()
        {
            esp_println::println!("AXP2101: power key IRQ enabled");
        } else {
            esp_println::println!("AXP2101: failed to enable power key IRQ");
        }
    }

    // Create reset interface using shared I2C bus (for display reset only)
    let reset = SharedTca9554Reset {
        i2c: Rc::clone(i2c_bus),
        addr: tca9554_addr,
    };
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
    let display = match display_res {
        Ok(d) => {
            esp_println::println!("Display initialized successfully");
            d
        }
        Err(e) => {
            esp_println::println!("Display init failed: {:?}", e);
            loop {
                Timer::after(Duration::from_secs(1)).await;
            }
        }
    };

    // --- Buttons ---
    // GPIO0 (BOOT) = turn left
    // AXP2101 Power Key = turn right (via PMU IRQ)
    let btn_cfg = InputConfig::default().with_pull(Pull::Up);
    let btn_left = Input::new(peripherals.GPIO0, btn_cfg);

    esp_println::println!("BTN: initialized GPIO0 (BOOT = left) and AXP2101 power key (right)");
    esp_println::println!(
        "BTN: BOOT initial state = {}",
        if btn_left.is_low() {
            "pressed"
        } else {
            "released"
        }
    );

    // --- Initialize RNG ---
    let rng = Rng::new();

    // --- Initialize Bevy ECS World ---
    let mut world = World::default();

    // Insert resources
    world.insert_resource(GameState::default());
    world.insert_resource(InputState::default());
    world.insert_resource(ButtonState::default());
    world.insert_resource(PerformanceMetrics::default());
    world.insert_resource(RngResource(rng));
    world.insert_resource(HighScore::new());
    world.insert_resource(FrameBufferResource::new());
    world.insert_non_send_resource(DisplayResource { display });
    world.insert_non_send_resource(ButtonLeftResource { button: btn_left });
    world.insert_non_send_resource(Axp2101Resource {
        i2c: Rc::clone(i2c_bus),
    });

    // Spawn initial snake
    world.spawn((
        SnakeHead,
        Position { x: 23, y: 28 }, // Center of grid
        Direction::Right,
    ));

    // Add initial segments
    world.spawn((SnakeSegment { index: 0 }, Position { x: 22, y: 28 }));
    world.spawn((SnakeSegment { index: 1 }, Position { x: 21, y: 28 }));

    // Create schedule
    let mut schedule = Schedule::default();
    schedule.add_systems(
        (
            input_system,
            restart_system, // Check restart before processing other inputs
            process_input_system,
            animate_food_system, // Animate food before rendering
            move_snake_system,
            collision_system,
            food_spawn_system,
            render_system,
        )
            .chain(),
    );

    esp_println::println!("Entering Bevy ECS main loop...");

    // Performance measurement (variables kept for future use)
    let _frame_start_us = 0u64;
    let _last_perf_log = 0u32;

    loop {
        // Measure frame time using system timer
        // Note: SystemTimer counts in microseconds at 80MHz, so we need to read it
        // For simplicity, we'll use a frame counter and estimate based on loop timing
        let loop_start = embassy_time::Instant::now();

        schedule.run(&mut world);

        let loop_end = embassy_time::Instant::now();
        let frame_time = loop_end.saturating_duration_since(loop_start);
        let frame_time_us = frame_time.as_micros() as u64;

        // Record performance metrics
        if let Some(mut perf) = world.get_resource_mut::<PerformanceMetrics>() {
            perf.record_frame(frame_time_us);

            // Log performance every 300 frames (~6 seconds at 50 FPS)
            if perf.should_log(300) {
                perf.log_performance();
            }
        }

        // Minimal delay for cooperative multitasking - let other tasks run
        // Don't artificially limit frame rate; instead, run input polling as fast as possible.
        // The render_system already uses needs_redraw optimization to avoid unnecessary work.
        // With ~16ms target, we get ~60 FPS for responsive input, but rendering only happens when needed.
        let target_frame_time_ms = 16; // ~60 FPS target for responsive input
        let frame_time_ms = frame_time_us / 1000;
        if frame_time_ms < target_frame_time_ms {
            let delay_ms = target_frame_time_ms - frame_time_ms;
            Timer::after(Duration::from_millis(delay_ms as u64)).await;
        } else {
            // Frame took too long - minimal yield for cooperative multitasking
            Timer::after(Duration::from_micros(100)).await;
        }
    }
}
