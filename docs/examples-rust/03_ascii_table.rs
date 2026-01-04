//! # ASCII Table Example
//!
//! This example displays a complete ASCII character table on the display, showing all
//! printable characters in a grid layout with row and column headers.
//!
//! ## What This Example Does
//!
//! - Calculates grid dimensions: numCols = LCD_WIDTH / 8, numRows = LCD_HEIGHT / 10
//! - Draws row headers (hexadecimal numbers 0-F) in GREEN along the left edge
//!   - Position: (10 + x * 8, 2) for each column index x
//! - Draws column headers (hexadecimal numbers 0-F) in BLUE along the top edge
//!   - Position: (2, 12 + y * 10) for each row index y
//! - Displays all ASCII characters (0-255) in a grid starting from character 0
//!   - Each character drawn at (10 + x * 8, 12 + y * 10)
//!   - White text on black background
//!   - Character code increments from 0, wrapping through the grid
//! - Displays for 5 seconds, then loop is empty
//!
//! ## When This Could Be Useful
//!
//! - Testing font rendering and character display capabilities
//! - Verifying that all ASCII characters render correctly
//! - Debugging text rendering issues
//! - As a reference for available characters in the font
//! - Educational purposes to understand character encoding
//!
//! ## Original Arduino Code
//!
//! ```cpp
//! void setup(void) {
//!   int numCols = LCD_WIDTH / 8;   // 368 / 8 = 46
//!   int numRows = LCD_HEIGHT / 10; // 448 / 10 = 44
//!
//!   gfx->begin();
//!   gfx->fillScreen(RGB565_BLACK);
//!   gfx->setBrightness(255);
//!
//!   // Draw column headers (top row) in GREEN
//!   gfx->setTextColor(RGB565_GREEN);
//!   for (int x = 0; x < numRows; x++) {
//!     gfx->setCursor(10 + x * 8, 2);
//!     gfx->print(x, 16);  // Print in hexadecimal
//!   }
//!
//!   // Draw row headers (left column) in BLUE
//!   gfx->setTextColor(RGB565_BLUE);
//!   for (int y = 0; y < numCols; y++) {
//!     gfx->setCursor(2, 12 + y * 10);
//!     gfx->print(y, 16);  // Print in hexadecimal
//!   }
//!
//!   // Draw ASCII characters
//!   char c = 0;
//!   for (int y = 0; y < numRows; y++) {
//!     for (int x = 0; x < numCols; x++) {
//!       gfx->drawChar(10 + x * 8, 12 + y * 10, c++, RGB565_WHITE, RGB565_BLACK);
//!     }
//!   }
//!
//!   delay(5000);
//! }
//! ```

use embedded_graphics::{
    mono_font::{MonoTextStyle, ascii::FONT_10X20},
    pixelcolor::Rgb888,
    prelude::*,
    text::Text,
};
use core::fmt::Write;
use heapless::String;

// Constants from original example
const LCD_WIDTH: usize = 368;
const LCD_HEIGHT: usize = 448;
const CHAR_WIDTH: usize = 8;   // Character width in pixels (from original: 8)
const CHAR_HEIGHT: usize = 10; // Character height in pixels (from original: 10)
const HEADER_OFFSET_X: i32 = 10;  // X offset for column headers and characters
const HEADER_OFFSET_Y: i32 = 2;   // Y offset for column headers
const ROW_HEADER_OFFSET_X: i32 = 2;  // X offset for row headers
const ROW_HEADER_OFFSET_Y: i32 = 12; // Y offset for row headers and characters
const DISPLAY_DURATION_MS: u32 = 5000;

// Colors
const RGB565_BLACK: Rgb888 = Rgb888::BLACK;
const RGB565_WHITE: Rgb888 = Rgb888::WHITE;
const RGB565_GREEN: Rgb888 = Rgb888::new(0, 255, 0);
const RGB565_BLUE: Rgb888 = Rgb888::new(0, 0, 255);

// Complete implementation:

// 1. Calculate grid dimensions
//    fn calculate_grid_dimensions() -> (usize, usize) {
//        let num_cols = LCD_WIDTH / CHAR_WIDTH;   // 368 / 8 = 46 columns
//        let num_rows = LCD_HEIGHT / CHAR_HEIGHT; // 448 / 10 = 44 rows
//        (num_cols, num_rows)
//    }

// 2. Draw column headers (top row, hexadecimal 0-F)
//    fn draw_column_headers(
//        display: &mut impl DrawTarget<Color = Rgb888>,
//        num_rows: usize,
//    ) -> Result<(), DisplayError> {
//        let text_style = MonoTextStyle::new(&FONT_10X20, RGB565_GREEN);
//        
//        for x in 0..num_rows {
//            // Format as hexadecimal
//            let mut hex_str = String::<2>::new();
//            write!(hex_str, "{:X}", x).ok();
//            
//            Text::new(
//                hex_str.as_str(),
//                Point::new(HEADER_OFFSET_X + (x * CHAR_WIDTH) as i32, HEADER_OFFSET_Y),
//                text_style,
//            )
//            .draw(display)?;
//        }
//        
//        Ok(())
//    }

// 3. Draw row headers (left column, hexadecimal 0-F)
//    fn draw_row_headers(
//        display: &mut impl DrawTarget<Color = Rgb888>,
//        num_cols: usize,
//    ) -> Result<(), DisplayError> {
//        let text_style = MonoTextStyle::new(&FONT_10X20, RGB565_BLUE);
//        
//        for y in 0..num_cols {
//            // Format as hexadecimal
//            let mut hex_str = String::<2>::new();
//            write!(hex_str, "{:X}", y).ok();
//            
//            Text::new(
//                hex_str.as_str(),
//                Point::new(ROW_HEADER_OFFSET_X, ROW_HEADER_OFFSET_Y + (y * CHAR_HEIGHT) as i32),
//                text_style,
//            )
//            .draw(display)?;
//        }
//        
//        Ok(())
//    }

// 4. Draw ASCII character grid
//    fn draw_ascii_grid(
//        display: &mut impl DrawTarget<Color = Rgb888>,
//        num_cols: usize,
//        num_rows: usize,
//    ) -> Result<(), DisplayError> {
//        let text_style = MonoTextStyle::new(&FONT_10X20, RGB565_WHITE);
//        let mut char_code: u8 = 0;
//        
//        for y in 0..num_rows {
//            for x in 0..num_cols {
//                if char_code < 256 {
//                    // Convert character code to char (if printable)
//                    let ch = match core::char::from_u32(char_code as u32) {
//                        Some(c) if c.is_ascii() && (c.is_ascii_graphic() || c == ' ') => c,
//                        _ => '?',  // Non-printable or invalid
//                    };
//                    
//                    // Create string with single character
//                    let mut char_str = String::<2>::new();
//                    char_str.push(ch).ok();
//                    
//                    // Draw character at grid position
//                    Text::new(
//                        char_str.as_str(),
//                        Point::new(
//                            HEADER_OFFSET_X + (x * CHAR_WIDTH) as i32,
//                            ROW_HEADER_OFFSET_Y + (y * CHAR_HEIGHT) as i32,
//                        ),
//                        text_style,
//                    )
//                    .draw(display)?;
//                    
//                    char_code += 1;
//                }
//            }
//        }
//        
//        Ok(())
//    }

// 5. Complete setup function
//    async fn setup(display: &mut DisplayDriver) -> Result<(), Error> {
//        // Initialize display
//        display.begin()?;
//        display.fill_screen(RGB565_BLACK)?;
//        display.set_brightness(255)?;
//        
//        // Calculate grid dimensions
//        let (num_cols, num_rows) = calculate_grid_dimensions();
//        esp_println::println!("ASCII Table: {} cols x {} rows", num_cols, num_rows);
//        
//        // Draw headers
//        draw_column_headers(display, num_rows)?;
//        draw_row_headers(display, num_cols)?;
//        
//        // Draw ASCII character grid
//        draw_ascii_grid(display, num_cols, num_rows)?;
//        
//        // Display for 5 seconds
//        Timer::after_millis(DISPLAY_DURATION_MS).await;
//        
//        Ok(())
//    }

// Notes:
// - The original example uses `drawChar(x, y, char, fg_color, bg_color)` which draws
//   a character with both foreground and background colors specified
// - In embedded-graphics, text is drawn on the existing background, so you need to
//   ensure the background is black before drawing
// - Character codes 0-255 cover the full extended ASCII range
// - Non-printable characters (0-31, 127) may display as spaces or special symbols
// - The grid layout assumes 8x10 pixel characters; adjust if using different fonts
// - The original prints numbers in base 16 (hexadecimal) using `print(x, 16)`
// - Total characters displayed: num_cols * num_rows (up to 256)
