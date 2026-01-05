#![no_std]
#![no_main]
#![deny(
    clippy::mem_forget,
    reason = "mem::forget is generally not safe to do with esp_hal types, especially those \
    holding buffers for the duration of a data transfer."
)]
#![deny(clippy::large_stack_frames)]

extern crate alloc;

use alloc::boxed::Box;
use alloc::rc::Rc;
use bevy_ecs::prelude::*;
use core::fmt::Write;
use embedded_graphics::{
    Drawable,
    mono_font::{MonoTextStyle, ascii::FONT_10X20},
    pixelcolor::Rgb888,
    prelude::*,
    primitives::{PrimitiveStyle, Rectangle},
    text::Text,
};
use embedded_graphics_framebuf::FrameBuf;
use embedded_graphics_framebuf::backends::FrameBufferBackend;
use heapless::String;
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
    spi::{
        Mode,
        master::{Config as SpiConfig, Spi},
    },
    time::Rate,
    timer::timg::TimerGroup,
    rng::Rng,
};
use sh8601_rs::{
    ColorMode, DisplaySize, ResetInterface, Sh8601Driver, Ws18AmoledDriver, framebuffer_size,
    DMA_CHUNK_SIZE,
};

// This creates a default app-descriptor required by the esp-idf bootloader.
esp_bootloader_esp_idf::esp_app_desc!();

// --- TCA9554 I/O Expander Support (used for display reset) ---
// Address is 0x20 per official Arduino examples (pin_config.h)
const TCA9554_ADDR_PRIMARY: u8 = 0x20;
const TCA9554_ADDR_FALLBACK: u8 = 0x24;
const TCA9554_OUTPUT: u8 = 0x01;
const TCA9554_POLARITY: u8 = 0x02;
const TCA9554_CONFIG: u8 = 0x03;

// --- AXP2101 PMU Support (for power button) ---
const AXP2101_ADDR: u8 = 0x34;
// Interrupt Enable registers
const AXP2101_INTEN1: u8 = 0x40;
const AXP2101_INTEN2: u8 = 0x41;
#[allow(dead_code)]
const AXP2101_INTEN3: u8 = 0x42;
// Interrupt Status registers
#[allow(dead_code)]
const AXP2101_INTSTS1: u8 = 0x48;
const AXP2101_INTSTS2: u8 = 0x49;
#[allow(dead_code)]
const AXP2101_INTSTS3: u8 = 0x4A;
// INTEN2 / INTSTS2 bit masks for power key
const AXP2101_PKEY_SHORT_IRQ_BIT: u8 = 0x08;  // Bit 3: POWERON Short Press IRQ
#[allow(dead_code)]
const AXP2101_PKEY_LONG_IRQ_BIT: u8 = 0x04;   // Bit 2: POWERON Long Press IRQ

// --- Framebuffer Support ---
const LCD_H_RES: usize = 368;
const LCD_V_RES: usize = 448;
const LCD_BUFFER_SIZE: usize = LCD_H_RES * LCD_V_RES;

pub struct HeapBuffer<C: PixelColor, const N: usize>(Box<[C; N]>);

impl<C: PixelColor, const N: usize> HeapBuffer<C, N> {
    pub fn new(data: Box<[C; N]>) -> Self {
        Self(data)
    }
}

impl<C: PixelColor, const N: usize> core::ops::Deref for HeapBuffer<C, N> {
    type Target = [C; N];
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<C: PixelColor, const N: usize> core::ops::DerefMut for HeapBuffer<C, N> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl<C: PixelColor, const N: usize> FrameBufferBackend for HeapBuffer<C, N> {
    type Color = C;
    fn set(&mut self, index: usize, color: Self::Color) {
        self.0[index] = color;
    }
    fn get(&self, index: usize) -> Self::Color {
        self.0[index]
    }
    fn nr_elements(&self) -> usize {
        N
    }
}

type FbBuffer = HeapBuffer<Rgb888, LCD_BUFFER_SIZE>;
type MyFrameBuf = FrameBuf<Rgb888, FbBuffer>;

// --- Snake Game Components ---

#[derive(Component, Clone, Copy, PartialEq, Eq)]
struct Position {
    x: i32,
    y: i32,
}

#[derive(Component, Clone, Copy)]
enum Direction {
    Up,
    Down,
    Left,
    Right,
}

impl Direction {
    fn turn_left(self) -> Self {
        match self {
            Direction::Up => Direction::Left,
            Direction::Left => Direction::Down,
            Direction::Down => Direction::Right,
            Direction::Right => Direction::Up,
        }
    }

    fn turn_right(self) -> Self {
        match self {
            Direction::Up => Direction::Right,
            Direction::Right => Direction::Down,
            Direction::Down => Direction::Left,
            Direction::Left => Direction::Up,
        }
    }
}

#[derive(Component)]
struct SnakeHead;

#[derive(Component)]
struct SnakeSegment {
    index: usize,
}

#[derive(Component, Clone, Copy, PartialEq, Eq, Debug)]
enum FoodType {
    Regular,  // 10 points
    Golden,   // 50 points
    Special,  // 100 points
}

impl FoodType {
    fn points(&self) -> u32 {
        match self {
            FoodType::Regular => 10,
            FoodType::Golden => 50,
            FoodType::Special => 100,
        }
    }
    
    fn color(&self) -> Rgb888 {
        match self {
            FoodType::Regular => Rgb888::new(255, 0, 0),      // Red
            FoodType::Golden => Rgb888::new(255, 215, 0),     // Gold
            FoodType::Special => Rgb888::new(255, 0, 255),    // Magenta
        }
    }
    
    fn spawn_chance(&self) -> u32 {
        match self {
            FoodType::Regular => 70,  // 70% chance
            FoodType::Golden => 25,   // 25% chance
            FoodType::Special => 5,   // 5% chance
        }
    }
}

#[derive(Component)]
struct Food {
    food_type: FoodType,
    animation_frame: u8, // For pulsing animation
}

// --- Game Resources ---

#[derive(Resource)]
struct FrameBufferResource {
    frame_buf: MyFrameBuf,
}

impl FrameBufferResource {
    fn new() -> Self {
        let fb_data: Box<[Rgb888; LCD_BUFFER_SIZE]> = Box::new([Rgb888::BLACK; LCD_BUFFER_SIZE]);
        let heap_buffer = HeapBuffer::new(fb_data);
        let frame_buf = MyFrameBuf::new(heap_buffer, LCD_H_RES, LCD_V_RES);
        Self { frame_buf }
    }
}

#[derive(Resource)]
struct GameState {
    score: u32,
    game_over: bool,
    move_timer: u32,
    move_interval: u32, // frames between moves
    frames_elapsed: u32, // Track total frames to prevent immediate collision
    needs_redraw: bool, // Track if we need to redraw
    new_high_score: bool, // Track if we just achieved a new high score
}

impl Default for GameState {
    fn default() -> Self {
        Self {
            score: 0,
            game_over: false,
            move_timer: 0,
            move_interval: 10, // Movement speed (frames between moves) - tuned for ~60fps target
            frames_elapsed: 0,
            needs_redraw: true, // Initial render needed
            new_high_score: false,
        }
    }
}

#[derive(Resource)]
struct RngResource(Rng);

#[derive(Resource)]
struct HighScore {
    score: u32,
    // TODO: Add flash persistence when esp-storage or similar is available
    // For now, high score resets on reboot
}

impl HighScore {
    fn new() -> Self {
        Self { score: 0 }
    }
    
    fn update(&mut self, new_score: u32) -> bool {
        if new_score > self.score {
            self.score = new_score;
            esp_println::println!("NEW HIGH SCORE: {}!", self.score);
            true
        } else {
            false
        }
    }
    
    fn get(&self) -> u32 {
        self.score
    }
}

#[derive(Resource, Default)]
struct InputState {
    turn_left: bool,
    turn_right: bool,
    restart: bool,
}

#[derive(Resource)]
struct PerformanceMetrics {
    frame_count: u32,
    total_frame_time_us: u64,
    max_frame_time_us: u64,
    min_frame_time_us: u64,
    last_log_frame: u32,
    #[allow(dead_code)] // For future on-screen FPS display
    show_on_screen: bool,
}

impl Default for PerformanceMetrics {
    fn default() -> Self {
        Self {
            frame_count: 0,
            total_frame_time_us: 0,
            max_frame_time_us: 0,
            min_frame_time_us: u64::MAX,
            last_log_frame: 0,
            show_on_screen: true, // Show FPS on screen by default
        }
    }
}

impl PerformanceMetrics {
    fn record_frame(&mut self, frame_time_us: u64) {
        self.frame_count += 1;
        self.total_frame_time_us += frame_time_us;
        if frame_time_us > self.max_frame_time_us {
            self.max_frame_time_us = frame_time_us;
        }
        if frame_time_us < self.min_frame_time_us {
            self.min_frame_time_us = frame_time_us;
        }
    }
    
    fn log_performance(&mut self) {
        if self.frame_count == 0 {
            return;
        }
        
        let avg_frame_time_us = self.total_frame_time_us / self.frame_count as u64;
        let avg_fps = if avg_frame_time_us > 0 {
            1_000_000 / avg_frame_time_us
        } else {
            0
        };
        
        esp_println::println!("=== Performance ({} frames) ===", self.frame_count);
        esp_println::println!("  Avg: {} us ({} FPS)", avg_frame_time_us, avg_fps);
        esp_println::println!("  Min: {} us", self.min_frame_time_us);
        esp_println::println!("  Max: {} us", self.max_frame_time_us);
        
        // Reset for next measurement period
        self.frame_count = 0;
        self.total_frame_time_us = 0;
        self.max_frame_time_us = 0;
        self.min_frame_time_us = u64::MAX;
    }
    
    #[allow(dead_code)] // For future on-screen FPS display
    fn get_current_fps(&self) -> u32 {
        if self.frame_count == 0 {
            return 0;
        }
        let avg_frame_time_us = self.total_frame_time_us / self.frame_count as u64;
        if avg_frame_time_us > 0 {
            (1_000_000 / avg_frame_time_us) as u32
        } else {
            0
        }
    }
}


#[derive(Resource, Default)]
struct ButtonState {
    /// Was BOOT button pressed last frame (for edge detection)
    boot_was_pressed: bool,
    /// BOOT button debounce counter
    boot_debounce: u32,
    /// Was power key pressed last frame (for edge detection)  
    pkey_was_pressed: bool,
    /// Frame counter for hold-to-restart
    hold_start_frame: Option<u32>,
    /// Current frame number
    frame: u32,
}

const HOLD_TO_RESTART_FRAMES: u32 = 90; // ~1.5 seconds at 60fps to restart

// AXP2101 PMU resource for reading power button
struct Axp2101Resource {
    i2c: Rc<core::cell::RefCell<I2c<'static, esp_hal::Blocking>>>,
}

// Shared TCA9554 reset interface that uses Rc to share I2C bus
struct SharedTca9554Reset {
    i2c: Rc<core::cell::RefCell<I2c<'static, esp_hal::Blocking>>>,
    addr: u8,
}

impl ResetInterface for SharedTca9554Reset {
    type Error = <I2c<'static, esp_hal::Blocking> as embedded_hal::i2c::ErrorType>::Error;
    
    fn reset(&mut self) -> Result<(), Self::Error> {
        let delay = Delay::new();
        let mut i2c = self.i2c.borrow_mut();
        i2c.write(self.addr, &[TCA9554_CONFIG, 0b0111_1000])?;
        i2c.write(self.addr, &[TCA9554_POLARITY, 0x00])?;
        i2c.write(self.addr, &[TCA9554_OUTPUT, 0b1000_0110])?;
        delay.delay_millis(20);
        i2c.write(self.addr, &[TCA9554_OUTPUT, 0b1000_0111])?;
        delay.delay_millis(150);
        Ok(())
    }
}

// Type alias for the display driver to simplify the type
type DisplayDriver = Sh8601Driver<
    Ws18AmoledDriver,
    SharedTca9554Reset,
>;

// Display resource - NonSend because it contains non-thread-safe components
struct DisplayResource {
    display: DisplayDriver,
}

// Button resources - NonSend because GPIO pins are not Send
struct ButtonLeftResource {
    button: Input<'static>,
}

// --- Game Systems ---

/// Two-button input system:
/// - GPIO0 (BOOT button) = turn left
/// - AXP2101 Power Key (via PMU IRQ) = turn right
/// - Hold either when game over = restart
/// 
/// Uses EDGE DETECTION: only triggers on button press, not while held
fn input_system(
    left_btn: NonSendMut<ButtonLeftResource>,
    axp2101: NonSendMut<Axp2101Resource>,
    mut input_state: ResMut<InputState>,
    mut button_state: ResMut<ButtonState>,
    game_state: Res<GameState>,
) {
    button_state.frame += 1;
    
    // === BOOT BUTTON (GPIO0) - active-low ===
    let boot_raw = left_btn.button.is_low();
    
    // Minimal debounce: just 1 frame to avoid electrical noise
    // (Reduced from 3 frames for faster response)
    let boot_pressed = if boot_raw {
        button_state.boot_debounce = button_state.boot_debounce.saturating_add(1);
        button_state.boot_debounce >= 1
    } else {
        button_state.boot_debounce = 0;
        false
    };
    
    // Edge detection for BOOT button
    let boot_just_pressed = boot_pressed && !button_state.boot_was_pressed;
    button_state.boot_was_pressed = boot_pressed;
    
    // === POWER KEY (AXP2101 PMU) - read from IRQ status register ===
    // The power key press is detected via the PMU's interrupt status register.
    // We read INTSTS2, check the PKEY_SHORT bit, then clear it by writing 0xFF.
    let pkey_pressed = {
        let mut i2c = axp2101.i2c.borrow_mut();
        let mut status = [0u8; 1];
        
        // Read interrupt status register 2 (0x49)
        if i2c.write_read(AXP2101_ADDR, &[AXP2101_INTSTS2], &mut status).is_ok() {
            let short_press = (status[0] & AXP2101_PKEY_SHORT_IRQ_BIT) != 0;
            
            // Clear the interrupt by writing 0xFF to the status register
            if short_press {
                let _ = i2c.write(AXP2101_ADDR, &[AXP2101_INTSTS2, 0xFF]);
            }
            
            short_press
        } else {
            false
        }
    };
    
    // Power key is edge-triggered by design (IRQ fires once per press)
    let pkey_just_pressed = pkey_pressed && !button_state.pkey_was_pressed;
    button_state.pkey_was_pressed = pkey_pressed;
    
    // === Combined input handling ===
    let any_pressed = boot_pressed || pkey_pressed;
    let _any_just_pressed = boot_just_pressed || pkey_just_pressed;
    
    // Handle game-over state: hold to restart
    if game_state.game_over {
        if any_pressed {
            if button_state.hold_start_frame.is_none() {
                button_state.hold_start_frame = Some(button_state.frame);
            } else if let Some(start) = button_state.hold_start_frame {
                let hold_duration = button_state.frame.saturating_sub(start);
                if hold_duration >= HOLD_TO_RESTART_FRAMES && !input_state.restart {
                    input_state.restart = true;
                    esp_println::println!("BTN: held long enough - restart!");
                }
            }
        } else {
            button_state.hold_start_frame = None;
        }
    } else {
        // During gameplay: BOOT = left, Power Key = right
        button_state.hold_start_frame = None;
        
        if boot_just_pressed {
            input_state.turn_left = true;
            esp_println::println!("BTN: BOOT press -> turn left");
        }
        if pkey_just_pressed {
            input_state.turn_right = true;
            esp_println::println!("BTN: Power key press -> turn right");
        }
    }
}

fn process_input_system(
    mut input_state: ResMut<InputState>,
    mut game_state: ResMut<GameState>,
    mut head_query: Query<&mut Direction, With<SnakeHead>>,
) {
    // Handle restart first (works even when game is over)
    if input_state.restart {
        return;
    }
    
    // Only process turn inputs if game is not over
    if game_state.game_over {
        input_state.turn_left = false;
        input_state.turn_right = false;
        return;
    }
    
    // Process turn inputs (edge-triggered in input_system, so this fires at most once per press)
    // Left takes priority if both pressed simultaneously
    if input_state.turn_left {
        if let Ok(mut dir) = head_query.single_mut() {
            *dir = dir.turn_left();
            game_state.needs_redraw = true;
        }
        input_state.turn_left = false;
    } else if input_state.turn_right {
        if let Ok(mut dir) = head_query.single_mut() {
            *dir = dir.turn_right();
            game_state.needs_redraw = true;
        }
        input_state.turn_right = false;
    }
}

fn move_snake_system(
    mut game_state: ResMut<GameState>,
    mut head_query: Query<(&mut Position, &Direction), With<SnakeHead>>,
    mut segment_query: Query<(&mut Position, &SnakeSegment), (With<SnakeSegment>, Without<SnakeHead>)>,
) {
    if game_state.game_over {
        return;
    }

    game_state.frames_elapsed += 1;
    game_state.move_timer += 1;
    if game_state.move_timer < game_state.move_interval {
        return;
    }
    game_state.move_timer = 0;
    game_state.needs_redraw = true; // Mark that we need to redraw after movement

    let (mut head_pos, direction) = match head_query.single_mut() {
        Ok(h) => h,
        Err(_) => return,
    };

    // Calculate new head position
    let new_head_pos = match *direction {
        Direction::Up => Position {
            x: head_pos.x,
            y: head_pos.y - 1,
        },
        Direction::Down => Position {
            x: head_pos.x,
            y: head_pos.y + 1,
        },
        Direction::Left => Position {
            x: head_pos.x - 1,
            y: head_pos.y,
        },
        Direction::Right => Position {
            x: head_pos.x + 1,
            y: head_pos.y,
        },
    };

    // Store old head position BEFORE any updates
    let old_head_pos = *head_pos;
    
    // CRITICAL: Collect all segment data (index, old_position) BEFORE updating head or any segments
    // This ensures we have a snapshot of all positions before any mutations
    let mut segment_data: heapless::Vec<(usize, Position), 256> = heapless::Vec::new();
    for (pos, seg) in segment_query.iter() {
        segment_data.push((seg.index, *pos)).ok();
    }
    
    // Now update head to new position (after collecting segment positions)
    *head_pos = new_head_pos;
    
    // 2. Sort by index (ascending: 0, 1, 2...)
    segment_data.sort_unstable_by_key(|(idx, _)| *idx);
    
    // Validate segment_data: check for duplicate indices or missing indices
    let mut prev_idx = None;
    for (idx, _) in segment_data.iter() {
        if let Some(prev) = prev_idx {
            if *idx == prev {
                esp_println::println!("MOVE: ERROR - duplicate segment index {}", idx);
            } else if *idx != prev + 1 {
                esp_println::println!("MOVE: WARNING - non-contiguous indices: {} -> {}", prev, idx);
            }
        }
        prev_idx = Some(*idx);
    }
    
    // Debug: log collected segment data occasionally (kept lightweight)
    if segment_data.len() > 0 && game_state.frames_elapsed % 200 == 0 {
        esp_println::println!(
            "MOVE: head old=({}, {}) new=({}, {}) segs={}",
            old_head_pos.x,
            old_head_pos.y,
            new_head_pos.x,
            new_head_pos.y,
            segment_data.len()
        );
    }
    
    // 3. Update segments in REVERSE order (tail to head) to avoid overwriting positions we still need
    // This ensures we never overwrite a position that another segment still needs
    // By updating from tail to head, each segment gets the position of the segment ahead of it,
    // and we never overwrite a position we still need to read
    let max_index = segment_data.iter().map(|(idx, _)| *idx).max().unwrap_or(0);
    
    // Update segments in reverse order (highest index first, down to 0)
    for target_idx in (0..=max_index).rev() {
        // Find the segment with this index and update it
        for (mut pos, seg) in segment_query.iter_mut() {
            if seg.index == target_idx {
                if target_idx == 0 {
                    // Segment 0 gets the old head position
                    *pos = old_head_pos;
                } else {
                    // Segment i gets the old position of segment i-1
                    // Find the old position of segment (target_idx - 1) in segment_data
                    if let Some((_prev_idx, prev_pos)) = segment_data.iter().find(|(idx, _)| *idx == target_idx - 1) {
                        // Critical check: if the previous segment's old position is the new head position,
                        // this indicates a serious bug in the movement logic
                        if *prev_pos == new_head_pos {
                            esp_println::println!("MOVE: CRITICAL ERROR - segment {} prev_seg[{}] old_pos=({}, {}) == new_head=({}, {})", 
                                target_idx, target_idx - 1, prev_pos.x, prev_pos.y, new_head_pos.x, new_head_pos.y);
                            esp_println::println!("MOVE: This should never happen! segment_data may be corrupted.");
                            // Don't update - leave segment at current position to avoid crash
                        } else if *prev_pos == *pos {
                            // Another safety check: if we're trying to move a segment to where it already is,
                            // something is wrong (this can happen if segment_data is stale)
                            esp_println::println!("MOVE: WARNING - segment {} already at target position ({}, {})", target_idx, prev_pos.x, prev_pos.y);
                            // Still update to be safe, but log the warning
                            *pos = *prev_pos;
                        } else {
                            *pos = *prev_pos;
                            
                            // Final safety check: ensure no segment ends up at the new head position
                            if *pos == new_head_pos {
                                esp_println::println!("MOVE: CRITICAL ERROR - segment {} ended up at new head position ({}, {})!", 
                                    target_idx, pos.x, pos.y);
                                // Move it to a safe position (old head position as fallback)
                                *pos = old_head_pos;
                            }
                            
                            // Debug: log segment updates occasionally
                            if target_idx <= 2 && game_state.frames_elapsed % 50 == 0 {
                                esp_println::println!("MOVE: updated segment {} to ({}, {})", target_idx, pos.x, pos.y);
                            }
                        }
                    } else {
                        esp_println::println!("MOVE: ERROR - segment {} could not find previous segment {} in segment_data", target_idx, target_idx - 1);
                    }
                }
                
                // Found this segment (whether updated or not), move to next index
                break;
            }
        }
    }
}


fn dump_game_state(
    reason: &str,
    head_pos: &Position,
    head_dir: &Direction,
    game_state: &GameState,
    segment_query: &Query<(&Position, &SnakeSegment), (With<SnakeSegment>, Without<SnakeHead>)>,
    food_query: &Query<(Entity, &Position, &Food), With<Food>>,
) {
    const GRID_WIDTH: i32 = 46;
    const GRID_HEIGHT: i32 = 56;
    
    let dir_str = match head_dir {
        Direction::Up => "Up",
        Direction::Down => "Down",
        Direction::Left => "Left",
        Direction::Right => "Right",
    };
    
    esp_println::println!("=== GAME OVER: {} ===", reason);
    esp_println::println!("  Head: ({}, {}) facing {}", head_pos.x, head_pos.y, dir_str);
    esp_println::println!("  Grid: {}x{} (valid: 0..{}, 0..{})", GRID_WIDTH, GRID_HEIGHT, GRID_WIDTH-1, GRID_HEIGHT-1);
    esp_println::println!("  Score: {}, Frames: {}, Speed: {}", game_state.score, game_state.frames_elapsed, game_state.move_interval);
    
    // Collect and sort segments by index
    let mut segments: heapless::Vec<(usize, i32, i32), 64> = heapless::Vec::new();
    for (pos, seg) in segment_query.iter() {
        segments.push((seg.index, pos.x, pos.y)).ok();
    }
    segments.sort_unstable_by_key(|(idx, _, _)| *idx);
    
    esp_println::println!("  Segments ({}):", segments.len());
    for (idx, x, y) in segments.iter() {
        esp_println::println!("    [{}]: ({}, {})", idx, x, y);
    }
    
    // Food positions
    let mut food_count = 0;
    for (_, food_pos, food) in food_query.iter() {
        esp_println::println!("  Food: ({}, {}) type={:?}", food_pos.x, food_pos.y, food.food_type);
        food_count += 1;
    }
    if food_count == 0 {
        esp_println::println!("  Food: none");
    }
    esp_println::println!("=== END STATE DUMP ===");
}

fn collision_system(
    mut game_state: ResMut<GameState>,
    mut high_score: ResMut<HighScore>,
    head_query: Query<(&Position, &Direction), With<SnakeHead>>,
    segment_query: Query<(&Position, &SnakeSegment), (With<SnakeSegment>, Without<SnakeHead>)>,
    food_query: Query<(Entity, &Position, &Food), With<Food>>,
    mut commands: Commands,
) {
    if game_state.game_over {
        return;
    }

    // Don't check collisions until snake has moved at least once
    // This prevents false collisions on the first frame
    if game_state.frames_elapsed < game_state.move_interval {
        return;
    }

    let (head_pos, head_dir) = match head_query.single() {
        Ok((p, d)) => (*p, *d),
        Err(_) => return,
    };

    // Check wall collision
    const GRID_WIDTH: i32 = 46; // 368 / 8
    const GRID_HEIGHT: i32 = 56; // 448 / 8
    
    if head_pos.x < 0
        || head_pos.x >= GRID_WIDTH
        || head_pos.y < 0
        || head_pos.y >= GRID_HEIGHT
    {
        game_state.game_over = true;
        game_state.needs_redraw = true;
        // Check for new high score on game over
        if high_score.update(game_state.score) {
            game_state.new_high_score = true;
        }
        dump_game_state("Wall collision", &head_pos, &head_dir, &game_state, &segment_query, &food_query);
        return;
    }

    // Check self collision
    // Skip segment 0 (index 0) because it's always at the old head position right after movement
    // This prevents false collisions. Only check segments 1 and beyond.
    for (seg_pos, seg) in segment_query.iter() {
        if seg.index > 0 && *seg_pos == head_pos {
            game_state.game_over = true;
            game_state.needs_redraw = true;
            // Check for new high score on game over
            if high_score.update(game_state.score) {
                game_state.new_high_score = true;
            }
            esp_println::println!("=== GAME OVER: Self collision with segment {} ===", seg.index);
            dump_game_state("Self collision", &head_pos, &head_dir, &game_state, &segment_query, &food_query);
            return;
        }
    }

    // Check food collision
    for (food_entity, food_pos, food) in food_query.iter() {
        if *food_pos == head_pos {
            // Eat food - get points based on food type
            let points = food.food_type.points();
            commands.entity(food_entity).despawn();
            game_state.score += points;
            
            // Add new segment to snake
            // Find the last segment (highest index)
            // Use index to find tail, not position order which can be misleading if coiled
            let max_index_seg = segment_query
                .iter()
                .max_by_key(|(_, s)| s.index);
                
            let (last_pos, next_index) = if let Some((pos, s)) = max_index_seg {
                (*pos, s.index + 1)
            } else {
                (head_pos, 0)
            };
            
            // Safety check: ensure new segment is not spawned at head position
            // This should never happen, but if it does, spawn it at a safe position
            let spawn_pos = if last_pos == head_pos {
                esp_println::println!("FOOD: WARNING - tail at head position ({}, {}), using fallback", head_pos.x, head_pos.y);
                // Fallback: spawn behind the head in the opposite direction of movement
                // This is a safety measure and should never be needed
                match head_dir {
                    Direction::Up => Position { x: head_pos.x, y: head_pos.y + 1 },
                    Direction::Down => Position { x: head_pos.x, y: head_pos.y - 1 },
                    Direction::Left => Position { x: head_pos.x + 1, y: head_pos.y },
                    Direction::Right => Position { x: head_pos.x - 1, y: head_pos.y },
                }
            } else {
                last_pos
            };
            
            commands.spawn((
                SnakeSegment { index: next_index },
                spawn_pos,
            ));

            // Speed up slightly (more for special foods)
            let speed_boost = match food.food_type {
                FoodType::Regular => 1,
                FoodType::Golden => 1,
                FoodType::Special => 2,
            };
            for _ in 0..speed_boost {
                if game_state.move_interval > 2 {
                    game_state.move_interval -= 1;
                }
            }
            game_state.needs_redraw = true;

            esp_println::println!("Score: {} (+{} from {:?})", game_state.score, points, food.food_type);
        }
    }
}

fn restart_system(
    mut input_state: ResMut<InputState>,
    mut game_state: ResMut<GameState>,
    mut head_query: Query<&mut Position, With<SnakeHead>>,
    mut head_dir_query: Query<&mut Direction, With<SnakeHead>>,
    segment_query: Query<Entity, With<SnakeSegment>>,
    food_query: Query<Entity, With<Food>>,
    mut commands: Commands,
) {
    if !input_state.restart {
        return;
    }
    
    esp_println::println!("=== RESTART SYSTEM TRIGGERED ===");
    esp_println::println!("  game_over was: {}", game_state.game_over);
    
    // Clear restart flag immediately to prevent multiple restarts
    input_state.restart = false;
    
    // Reset game state (preserve high score)
    let old_high_score = game_state.new_high_score;
    *game_state = GameState::default();
    game_state.new_high_score = old_high_score; // Preserve flag during restart
    
    // Remove all segments
    for entity in segment_query.iter() {
        commands.entity(entity).despawn();
    }
    
    // Remove all food
    for entity in food_query.iter() {
        commands.entity(entity).despawn();
    }
    
    // Reset head position and direction
    if let Ok(mut head_pos) = head_query.single_mut() {
        *head_pos = Position { x: 23, y: 28 }; // Center of grid
    }
    if let Ok(mut dir) = head_dir_query.single_mut() {
        *dir = Direction::Right;
    }
    
    // Spawn initial segments
    commands.spawn((
        SnakeSegment { index: 0 },
        Position { x: 22, y: 28 },
    ));
    commands.spawn((
        SnakeSegment { index: 1 },
        Position { x: 21, y: 28 },
    ));
    
    game_state.needs_redraw = true;
    esp_println::println!("Game restarted!");
}

fn food_spawn_system(
    mut commands: Commands,
    food_query: Query<Entity, With<Food>>,
    all_positions: Query<&Position>,
    rng: ResMut<RngResource>,
    game_state: Res<GameState>,
) {
    if game_state.game_over {
        return;
    }

    // Only spawn food if none exists
    if food_query.iter().next().is_some() {
        return;
    }

    const GRID_WIDTH: i32 = 46;
    const GRID_HEIGHT: i32 = 56;

    // Determine food type based on weighted random
    let mut buf = [0u8; 4];
    rng.0.read(&mut buf);
    let rand_val = (buf[0] as u32 | ((buf[1] as u32) << 8)) % 100;
    
    let food_type = if rand_val < FoodType::Regular.spawn_chance() {
        FoodType::Regular
    } else if rand_val < FoodType::Regular.spawn_chance() + FoodType::Golden.spawn_chance() {
        FoodType::Golden
    } else {
        FoodType::Special
    };

    // Try to find a free position
    let mut attempts = 0;
    loop {
        rng.0.read(&mut buf);
        let x = ((buf[0] as u32 | ((buf[1] as u32) << 8)) % GRID_WIDTH as u32) as i32;
        rng.0.read(&mut buf);
        let y = ((buf[0] as u32 | ((buf[1] as u32) << 8)) % GRID_HEIGHT as u32) as i32;

        let pos = Position { x, y };
        let occupied = all_positions.iter().any(|&p| p == pos);

        if !occupied {
            commands.spawn((
                Food {
                    food_type,
                    animation_frame: 0,
                },
                pos,
            ));
            esp_println::println!("Food spawned: {:?} at ({}, {})", food_type, x, y);
            break;
        }

        attempts += 1;
        if attempts > 100 {
            // Give up if we can't find a spot
            break;
        }
    }
}

fn animate_food_system(
    mut food_query: Query<&mut Food>,
) {
    for mut food in food_query.iter_mut() {
        food.animation_frame = food.animation_frame.wrapping_add(1);
    }
}

fn render_system(
    mut display_res: NonSendMut<DisplayResource>,
    mut game_state: ResMut<GameState>,
    mut fb_res: ResMut<FrameBufferResource>,
    head_query: Query<&Position, With<SnakeHead>>,
    segment_query: Query<(&Position, &SnakeSegment), With<SnakeSegment>>,
    food_query: Query<(&Position, &Food), With<Food>>,
    high_score: Res<HighScore>,
    _perf_metrics: Option<Res<PerformanceMetrics>>,
) {
    // Only render when needed (after movement or state change)
    // For game over, only render once when it first happens
    if !game_state.needs_redraw {
        return;
    }
    game_state.needs_redraw = false;
    // Clear framebuffer
    fb_res.frame_buf.clear(Rgb888::BLACK).unwrap();

    const CELL_SIZE: i32 = 8;
    const GRID_OFFSET_X: i32 = 0;
    const GRID_OFFSET_Y: i32 = 0;
    
    // Direct framebuffer writes for game objects (much faster than embedded-graphics primitives)
    // Access the underlying array slice from HeapBuffer
    let fb_data: &mut [Rgb888] = &mut *fb_res.frame_buf.data;
    
    // Helper function to fill a cell in the framebuffer
    #[inline(always)]
    fn fill_cell(fb_data: &mut [Rgb888], x: usize, y: usize, color: Rgb888, cell_size: usize) {
        if x + cell_size <= LCD_H_RES && y + cell_size <= LCD_V_RES {
            for dy in 0..cell_size {
                let row_start = (y + dy) * LCD_H_RES + x;
                for dx in 0..cell_size {
                    fb_data[row_start + dx] = color;
                }
            }
        }
    }
    
    // Helper function to fill a cell with gradient (for snake segments)
    #[inline(always)]
    fn fill_cell_gradient(fb_data: &mut [Rgb888], x: usize, y: usize, base_color: Rgb888, intensity: u8, cell_size: usize) {
        if x + cell_size <= LCD_H_RES && y + cell_size <= LCD_V_RES {
            // Create gradient by adjusting brightness
            let r = ((base_color.r() as u16 * intensity as u16) / 255) as u8;
            let g = ((base_color.g() as u16 * intensity as u16) / 255) as u8;
            let b = ((base_color.b() as u16 * intensity as u16) / 255) as u8;
            let color = Rgb888::new(r, g, b);
            
            for dy in 0..cell_size {
                let row_start = (y + dy) * LCD_H_RES + x;
                for dx in 0..cell_size {
                    fb_data[row_start + dx] = color;
                }
            }
        }
    }
    
    // Helper function to draw animated food (pulsing effect)
    #[inline(always)]
    fn fill_cell_animated(fb_data: &mut [Rgb888], x: usize, y: usize, base_color: Rgb888, animation_frame: u8, cell_size: usize) {
        if x + cell_size <= LCD_H_RES && y + cell_size <= LCD_V_RES {
            // Create pulsing effect using sine wave approximation
            // animation_frame cycles 0-255, we want brightness 128-255
            let pulse = 128 + ((animation_frame.wrapping_mul(2) as u16 * 127) / 255) as u8;
            let r = ((base_color.r() as u16 * pulse as u16) / 255) as u8;
            let g = ((base_color.g() as u16 * pulse as u16) / 255) as u8;
            let b = ((base_color.b() as u16 * pulse as u16) / 255) as u8;
            let color = Rgb888::new(r, g, b);
            
            for dy in 0..cell_size {
                let row_start = (y + dy) * LCD_H_RES + x;
                for dx in 0..cell_size {
                    fb_data[row_start + dx] = color;
                }
            }
        }
    }

    // Draw food with animation
    for (food_pos, food) in food_query.iter() {
        let screen_x = GRID_OFFSET_X + food_pos.x * CELL_SIZE;
        let screen_y = GRID_OFFSET_Y + food_pos.y * CELL_SIZE;
        // Bounds check: skip if position is out of bounds (prevents panic from negative -> usize conversion)
        if screen_x >= 0 && screen_y >= 0 
            && (screen_x as usize) + (CELL_SIZE as usize) <= LCD_H_RES 
            && (screen_y as usize) + (CELL_SIZE as usize) <= LCD_V_RES {
            let x = screen_x as usize;
            let y = screen_y as usize;
            fill_cell_animated(fb_data, x, y, food.food_type.color(), food.animation_frame, CELL_SIZE as usize);
        }
    }

    // Draw snake segments with gradient (head to tail gets darker)
    // First, collect all segments with their indices
    let mut segments: heapless::Vec<(usize, i32, i32), 256> = heapless::Vec::new();
    for (seg_pos, seg) in segment_query.iter() {
        segments.push((seg.index, seg_pos.x, seg_pos.y)).ok();
    }
    segments.sort_unstable_by_key(|(idx, _, _)| *idx);
    let total_segments = segments.len().max(1);
    
    // Draw segments with gradient based on position in snake
    for (idx, seg_x, seg_y) in segments.iter() {
        let screen_x = GRID_OFFSET_X + seg_x * CELL_SIZE;
        let screen_y = GRID_OFFSET_Y + seg_y * CELL_SIZE;
        // Bounds check: skip if position is out of bounds (prevents panic from negative -> usize conversion)
        if screen_x >= 0 && screen_y >= 0 
            && (screen_x as usize) + (CELL_SIZE as usize) <= LCD_H_RES 
            && (screen_y as usize) + (CELL_SIZE as usize) <= LCD_V_RES {
            let x = screen_x as usize;
            let y = screen_y as usize;
            // Calculate gradient: head (index 0) is brightest, tail is darkest
            // Intensity ranges from 255 (head) to 128 (tail)
            let intensity = 128 + ((127 * (total_segments - idx)) / total_segments.max(1)) as u8;
            fill_cell_gradient(fb_data, x, y, Rgb888::new(0, 255, 0), intensity, CELL_SIZE as usize);
        }
    }

    // Draw head (darker green)
    if let Ok(head_pos) = head_query.single() {
        let screen_x = GRID_OFFSET_X + head_pos.x * CELL_SIZE;
        let screen_y = GRID_OFFSET_Y + head_pos.y * CELL_SIZE;
        // Bounds check: skip if position is out of bounds (prevents panic from negative -> usize conversion)
        if screen_x >= 0 && screen_y >= 0 
            && (screen_x as usize) + (CELL_SIZE as usize) <= LCD_H_RES 
            && (screen_y as usize) + (CELL_SIZE as usize) <= LCD_V_RES {
            let x = screen_x as usize;
            let y = screen_y as usize;
            fill_cell(fb_data, x, y, Rgb888::new(0, 200, 0), CELL_SIZE as usize);
        }
    }

    if game_state.game_over {
        // Draw game over screen centered
        let game_over_text = if game_state.new_high_score { "NEW HIGH SCORE!" } else { "GAME OVER" };
        let mut score_text = String::<20>::new();
        write!(score_text, "Score: {}", game_state.score).ok();
        let mut high_score_text = String::<30>::new();
        write!(high_score_text, "High: {}", high_score.get()).ok();
        let restart_text = "Hold button to restart";
        
        // Calculate text widths for centering (FONT_10X20 is 10 pixels wide per character)
        let game_over_width = game_over_text.len() as i32 * 10;
        let score_width = score_text.len() as i32 * 10;
        let high_score_width = high_score_text.len() as i32 * 10;
        let _restart_width = restart_text.len() as i32 * 10;
        
        let center_x = (LCD_H_RES as i32 - game_over_width) / 2;
        let center_y = LCD_V_RES as i32 / 2;
        
        // Draw "GAME OVER" or "NEW HIGH SCORE!" centered
        let title_color = if game_state.new_high_score {
            Rgb888::new(255, 215, 0) // Gold for new high score
        } else {
            Rgb888::new(255, 0, 0) // Red for game over
        };
        Text::new(
            game_over_text,
            Point::new(center_x, center_y - 50),
            MonoTextStyle::new(&FONT_10X20, title_color),
        )
        .draw(&mut fb_res.frame_buf)
        .ok();
        
        // Draw score centered below
        let score_center_x = (LCD_H_RES as i32 - score_width) / 2;
        Text::new(
            score_text.as_str(),
            Point::new(score_center_x, center_y - 20),
            MonoTextStyle::new(&FONT_10X20, Rgb888::WHITE),
        )
        .draw(&mut fb_res.frame_buf)
        .ok();
        
        // Draw high score
        let high_score_center_x = (LCD_H_RES as i32 - high_score_width) / 2;
        Text::new(
            high_score_text.as_str(),
            Point::new(high_score_center_x, center_y + 10),
            MonoTextStyle::new(&FONT_10X20, Rgb888::new(255, 215, 0)),
        )
        .draw(&mut fb_res.frame_buf)
        .ok();
        
        // Draw restart instructions centered below high score
        Text::new(
            "Hold both to restart",
            Point::new((LCD_H_RES as i32 - "Hold both to restart".len() as i32 * 10) / 2, center_y + 40),
            MonoTextStyle::new(&FONT_10X20, Rgb888::new(128, 128, 128)),
        )
        .draw(&mut fb_res.frame_buf)
        .ok();
    } else {
        // Draw score and high score during gameplay
        let mut score_str = String::<20>::new();
        write!(score_str, "Score: {}", game_state.score).ok();
        Text::new(
            score_str.as_str(),
            Point::new(8, LCD_V_RES as i32 - 40),
            MonoTextStyle::new(&FONT_10X20, Rgb888::WHITE),
        )
        .draw(&mut fb_res.frame_buf)
        .ok();
        
        let mut high_score_str = String::<30>::new();
        write!(high_score_str, "High: {}", high_score.get()).ok();
        Text::new(
            high_score_str.as_str(),
            Point::new(8, LCD_V_RES as i32 - 20),
            MonoTextStyle::new(&FONT_10X20, Rgb888::new(255, 215, 0)),
        )
        .draw(&mut fb_res.frame_buf)
        .ok();

        // Draw instructions
        Text::new(
            "Boot=Left Pwr=Right",
            Point::new(8, 8),
            MonoTextStyle::new(&FONT_10X20, Rgb888::new(128, 128, 128)),
        )
        .draw(&mut fb_res.frame_buf)
        .ok();
    }

    // Copy framebuffer to display - OPTIMIZED VERSION
    // Use direct framebuffer transfer: draw entire framebuffer efficiently
    // Removed redundant display.clear() - framebuffer is already cleared
    
    // Access the underlying array slice from HeapBuffer
    let fb_data: &[Rgb888] = &*fb_res.frame_buf.data;
    
    // Strategy: Draw row-by-row with run-length encoding
    // IMPORTANT: We MUST draw black pixels too because we removed display.clear()!
    // If we skip black pixels, the old snake position will remain on screen (artifacts).
    
    for y in 0..LCD_V_RES {
        let row_start = y * LCD_H_RES;
        let mut x = 0;
        while x < LCD_H_RES {
            let pixel = fb_data[row_start + x];
            
            // Find run of identical pixels (works for Black and Colors)
            let start_x = x;
            let mut end_x = x + 1;
            while end_x < LCD_H_RES && fb_data[row_start + end_x] == pixel {
                end_x += 1;
            }
            
            // Draw rectangle for this run
            // Drawing black rectangles effectively "clears" that part of the screen
            let width = end_x - start_x;
            Rectangle::new(
                Point::new(start_x as i32, y as i32),
                Size::new(width as u32, 1),
            )
            .into_styled(PrimitiveStyle::with_fill(pixel))
            .draw(&mut display_res.display)
            .ok();
            
            x = end_x;
        }
    }
    
    display_res.display.flush().ok();
}

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
    let lcd_spi = Spi::new(
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
            .with_software_timeout(SoftwareTimeout::Transaction(esp_hal::time::Duration::from_millis(50))),
    )
    .unwrap()
    .with_sda(peripherals.GPIO15)
    .with_scl(peripherals.GPIO14);
    esp_println::println!("BOOT: I2C ready");

    // Use StaticCell to make i2c_bus live for 'static
    // Use Rc to share the I2C bus between reset interface and button reading
    static I2C_BUS: static_cell::StaticCell<Rc<core::cell::RefCell<I2c<'static, esp_hal::Blocking>>>> = static_cell::StaticCell::new();
    let i2c_bus = I2C_BUS.init(Rc::new(core::cell::RefCell::new(i2c)));
    esp_println::println!("BOOT: probing TCA9554");
    
    // Probe TCA9554 to find the correct address
    let tca9554_addr = {
        let mut i2c_ref = i2c_bus.borrow_mut();
        let primary_res = i2c_ref.write(TCA9554_ADDR_PRIMARY, &[TCA9554_POLARITY, 0x00]);
        esp_println::println!("TCA9554 probe 0x{:02X}: {:?}", TCA9554_ADDR_PRIMARY, primary_res);
        
        let fallback_res = i2c_ref.write(TCA9554_ADDR_FALLBACK, &[TCA9554_POLARITY, 0x00]);
        esp_println::println!("TCA9554 probe 0x{:02X}: {:?}", TCA9554_ADDR_FALLBACK, fallback_res);
        
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
        if i2c_ref.write_read(AXP2101_ADDR, &[0x03], &mut chip_id).is_ok() {
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
        if i2c_ref.write(AXP2101_ADDR, &[AXP2101_INTEN2, AXP2101_PKEY_SHORT_IRQ_BIT]).is_ok() {
            esp_println::println!("AXP2101: power key IRQ enabled");
        } else {
            esp_println::println!("AXP2101: failed to enable power key IRQ");
        }
    }
    
    // Create reset interface using shared I2C bus (for display reset only)
    let reset = SharedTca9554Reset {
        i2c: Rc::clone(&i2c_bus),
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
    esp_println::println!("BTN: BOOT initial state = {}", if btn_left.is_low() { "pressed" } else { "released" });

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
        i2c: Rc::clone(&i2c_bus),
    });

    // Spawn initial snake
    world.spawn((
        SnakeHead,
        Position { x: 23, y: 28 }, // Center of grid
        Direction::Right,
    ));

    // Add initial segments
    world.spawn((
        SnakeSegment { index: 0 },
        Position { x: 22, y: 28 },
    ));
    world.spawn((
        SnakeSegment { index: 1 },
        Position { x: 21, y: 28 },
    ));

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
            perf.last_log_frame += 1;
            
            // Log performance every 300 frames (~6 seconds at 50 FPS)
            if perf.last_log_frame >= 300 {
                perf.log_performance();
                perf.last_log_frame = 0;
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
