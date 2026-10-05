//! # Hello World Example
//!
//! This example demonstrates the most basic display functionality: initializing the SH8601 AMOLED
//! display and drawing text on it. It shows "Hello World!" in red text initially, then continuously
//! displays the text at random positions with random colors and sizes.
//!
//! ## What This Example Does
//!
//! - Initializes the display via QSPI bus
//! - Sets display brightness to maximum (255)
//! - Draws "Hello World!" text at position (10, 10) in red initially
//! - Waits 5 seconds
//! - In the main loop, continuously draws the text at random positions with:
//!   - Random X/Y coordinates (0 to width/height)
//!   - Random text colors (0x0000 to 0xFFFF for RGB565)
//!   - Random text sizes: X scale (1-6), Y scale (1-6), pixel margin (0-2)
//!   - 200ms delay between updates
//!
//! ## When This Could Be Useful
//!
//! - Testing display initialization and basic text rendering
//! - Verifying QSPI bus communication with the SH8601 controller
//! - Demonstrating text drawing capabilities
//! - As a starting point for more complex graphics applications
//!
//! ## Original Arduino Code Structure
//!
//! ```cpp
//! void setup(void) {
//!   gfx->begin();
//!   gfx->fillScreen(RGB565_BLACK);
//!   gfx->setBrightness(255);
//!   gfx->setCursor(10, 10);
//!   gfx->setTextColor(RGB565_RED);
//!   gfx->println("Hello World!");
//!   delay(5000);
//! }
//!
//! void loop() {
//!   gfx->setCursor(random(gfx->width()), random(gfx->height()));
//!   gfx->setTextColor(random(0xffff), random(0xffff));
//!   gfx->setTextSize(random(6) /* x scale */, random(6) /* y scale */, random(2) /* pixel_margin */);
//!   gfx->println("Hello World!");
//!   delay(200);
//! }
//! ```

use embedded_graphics::{
    mono_font::{MonoTextStyle, ascii::FONT_10X20},
    pixelcolor::Rgb888,
    prelude::*,
    text::Text,
};
use esp_hal::rng::Rng;

// Constants from original example
const LCD_WIDTH: usize = 368;
const LCD_HEIGHT: usize = 448;
const INITIAL_X: i32 = 10;
const INITIAL_Y: i32 = 10;
const INITIAL_DELAY_MS: u32 = 5000;
const LOOP_DELAY_MS: u32 = 200;
const MAX_BRIGHTNESS: u8 = 255;

// RGB565 color constants (converted to Rgb888 for embedded-graphics)
const RGB565_BLACK: Rgb888 = Rgb888::BLACK;
const RGB565_RED: Rgb888 = Rgb888::new(255, 0, 0);

// Complete implementation:

// 1. Setup/Initialization
//    fn setup(display: &mut DisplayDriver, rng: &mut Rng) {
//        // Initialize display
//        display.begin().unwrap();
//        
//        // Fill screen black
//        display.fill_screen(RGB565_BLACK).unwrap();
//        
//        // Set brightness to maximum
//        display.set_brightness(MAX_BRIGHTNESS).unwrap();
//        
//        // Draw initial "Hello World!" text
//        draw_text_at_position(
//            display,
//            "Hello World!",
//            INITIAL_X,
//            INITIAL_Y,
//            RGB565_RED,
//        ).unwrap();
//        
//        // Wait 5 seconds
//        Timer::after_millis(INITIAL_DELAY_MS).await;
//    }

// 2. Main loop - random text display
//    fn main_loop(display: &mut DisplayDriver, rng: &mut Rng) {
//        loop {
//            // Generate random position
//            let x = (rng.random() as usize % LCD_WIDTH) as i32;
//            let y = (rng.random() as usize % LCD_HEIGHT) as i32;
//            
//            // Generate random colors (RGB565 format: 16-bit)
//            let fg_color_raw = rng.random() as u16 & 0xFFFF;
//            let bg_color_raw = rng.random() as u16 & 0xFFFF;
//            
//            // Convert RGB565 to Rgb888
//            // RGB565: RRRRR GGGGGG BBBBB (5-6-5 bits)
//            let fg_r = ((fg_color_raw >> 11) & 0x1F) as u8;
//            let fg_g = ((fg_color_raw >> 5) & 0x3F) as u8;
//            let fg_b = (fg_color_raw & 0x1F) as u8;
//            let fg_color = Rgb888::new(
//                (fg_r << 3) | (fg_r >> 2),  // Expand 5-bit to 8-bit
//                (fg_g << 2) | (fg_g >> 4),  // Expand 6-bit to 8-bit
//                (fg_b << 3) | (fg_b >> 2),  // Expand 5-bit to 8-bit
//            );
//            
//            let bg_r = ((bg_color_raw >> 11) & 0x1F) as u8;
//            let bg_g = ((bg_color_raw >> 5) & 0x3F) as u8;
//            let bg_b = (bg_color_raw & 0x1F) as u8;
//            let bg_color = Rgb888::new(
//                (bg_r << 3) | (bg_r >> 2),
//                (bg_g << 2) | (bg_g >> 4),
//                (bg_b << 3) | (bg_b >> 2),
//            );
//            
//            // Generate random text size parameters
//            // Original: random(6) for x_scale, random(6) for y_scale, random(2) for pixel_margin
//            let x_scale = (rng.random() as usize % 6) + 1;  // 1-6
//            let y_scale = (rng.random() as usize % 6) + 1;  // 1-6
//            let pixel_margin = rng.random() as usize % 2;   // 0-1
//            
//            // Note: embedded-graphics MonoTextStyle doesn't support arbitrary scaling
//            // You would need to use different font sizes or implement scaling manually
//            // For now, we'll use a fixed font size
//            
//            // Clear screen (or just clear area around text for better effect)
//            display.fill_screen(RGB565_BLACK).unwrap();
//            
//            // Draw text at random position with random color
//            draw_text_at_position(display, "Hello World!", x, y, fg_color).unwrap();
//            
//            // Wait before next update
//            Timer::after_millis(LOOP_DELAY_MS).await;
//        }
//    }

// Helper function to draw text
// fn draw_text_at_position(
//     display: &mut impl DrawTarget<Color = Rgb888>,
//     text: &str,
//     x: i32,
//     y: i32,
//     color: Rgb888,
// ) -> Result<(), DisplayError> {
//     Text::new(
//         text,
//         Point::new(x, y),
//         MonoTextStyle::new(&FONT_10X20, color),
//     )
//     .draw(display)?;
//     Ok(())
// }

// Random number generation helper
// fn random_u16(rng: &mut Rng) -> u16 {
//     let mut buf = [0u8; 2];
//     rng.read(&mut buf);
//     u16::from_le_bytes(buf)
// }
//
// fn random_u32(rng: &mut Rng) -> u32 {
//     let mut buf = [0u8; 4];
//     rng.read(&mut buf);
//     u32::from_le_bytes(buf)
// }

// Notes:
// - The original Arduino example uses `random()` which generates values from 0 to RAND_MAX
// - In Rust, use `esp_hal::rng::Rng` for hardware random number generation
// - RGB565 color format uses 16 bits: 5 bits red, 6 bits green, 5 bits blue
// - Text scaling in embedded-graphics requires using different font sizes or manual scaling
// - The original uses `setTextSize(x_scale, y_scale, pixel_margin)` which embedded-graphics
//   doesn't directly support - you'd need to implement scaling manually or use different fonts
// - Consider using a framebuffer for smoother updates and to avoid full screen clears
