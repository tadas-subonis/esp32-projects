//! Configuration constants for the snake game.
//!
//! Contains I2C addresses, LCD dimensions, and game timing constants.

// --- TCA9554 I/O Expander Support (used for display reset) ---
// Address is 0x20 per official Arduino examples (pin_config.h)
pub const TCA9554_ADDR_PRIMARY: u8 = 0x20;
pub const TCA9554_ADDR_FALLBACK: u8 = 0x24;
pub const TCA9554_OUTPUT: u8 = 0x01;
pub const TCA9554_POLARITY: u8 = 0x02;
pub const TCA9554_CONFIG: u8 = 0x03;

// --- AXP2101 PMU Support (for power button) ---
pub const AXP2101_ADDR: u8 = 0x34;
// Interrupt Enable registers
pub const AXP2101_INTEN1: u8 = 0x40;
pub const AXP2101_INTEN2: u8 = 0x41;
#[allow(dead_code)]
pub const AXP2101_INTEN3: u8 = 0x42;
// Interrupt Status registers
#[allow(dead_code)]
pub const AXP2101_INTSTS1: u8 = 0x48;
pub const AXP2101_INTSTS2: u8 = 0x49;
#[allow(dead_code)]
pub const AXP2101_INTSTS3: u8 = 0x4A;
// INTEN2 / INTSTS2 bit masks for power key
pub const AXP2101_PKEY_SHORT_IRQ_BIT: u8 = 0x08; // Bit 3: POWERON Short Press IRQ
#[allow(dead_code)]
pub const AXP2101_PKEY_LONG_IRQ_BIT: u8 = 0x04; // Bit 2: POWERON Long Press IRQ

// --- Framebuffer / LCD Support ---
pub const LCD_H_RES: usize = 368;
pub const LCD_V_RES: usize = 448;
pub const LCD_BUFFER_SIZE: usize = LCD_H_RES * LCD_V_RES;

// --- Game Constants ---
pub const HOLD_TO_RESTART_FRAMES: u32 = 90; // ~1.5 seconds at 60fps to restart
pub const GRID_WIDTH: i32 = 46; // 368 / 8
pub const GRID_HEIGHT: i32 = 56; // 448 / 8
pub const CELL_SIZE: i32 = 8;
pub const GRID_OFFSET_X: i32 = 0;
pub const GRID_OFFSET_Y: i32 = 0;
