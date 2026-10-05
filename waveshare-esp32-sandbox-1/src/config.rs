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
pub const AXP2101_PMU_STATUS1: u8 = 0x00;
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
pub const AXP2101_BATTERY_PERCENT: u8 = 0xA4;
pub const AXP2101_BATTERY_PRESENT_BIT: u8 = 0x08; // PmuStatus1 bit 3

// --- Framebuffer / LCD Support ---
pub const LCD_H_RES: usize = 368;
pub const LCD_V_RES: usize = 448;
pub const LCD_BUFFER_SIZE: usize = LCD_H_RES * LCD_V_RES;

// --- Game Constants ---
pub const HOLD_TO_RESTART_FRAMES: u32 = 90; // ~1.5 seconds at 60fps to restart
pub const CELL_SIZE: i32 = 32; // 4x scale from 8px cells
pub const GRID_WIDTH: i32 = LCD_H_RES as i32 / CELL_SIZE;
pub const GRID_HEIGHT: i32 = LCD_V_RES as i32 / CELL_SIZE;
pub const GRID_OFFSET_X: i32 = (LCD_H_RES as i32 - (GRID_WIDTH * CELL_SIZE)) / 2;
pub const GRID_OFFSET_Y: i32 = (LCD_V_RES as i32 - (GRID_HEIGHT * CELL_SIZE)) / 2;
pub const UI_PADDING_X: i32 = 12;
pub const UI_PADDING_Y: i32 = 12;
pub const UI_TEXT_SCALE_NUM: i32 = 3;
pub const UI_TEXT_SCALE_DEN: i32 = 2;
