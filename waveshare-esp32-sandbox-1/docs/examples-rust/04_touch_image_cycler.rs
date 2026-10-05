//! # Touch Image Cycler Example
//!
//! This example displays images on the screen and cycles through them when you touch the display.
//! Each touch advances to the next image in a sequence, creating an interactive image gallery.
//!
//! ## What This Example Does
//!
//! - Initializes TCA9554 expander and FT3168 touch controller
//! - Performs reset sequence for display and touch
//! - Initializes display with brightness fade-in (0-255 over ~1.3 seconds, 5ms steps)
//! - Shows color test sequence: RED (1s) → GREEN (1s) → BLUE (1s) → WHITE
//! - Displays "Touch me" text at position (60, 200) with text size 5, black color
//! - Reads and displays FT3168 touch controller ID (for debugging)
//! - Waits for touch events
//! - When touched, reads touch coordinates and finger count
//! - Cycles through 4 images (Image_Flag 0-3) when finger_count > 0
//! - Each image is displayed full-screen using draw16bitRGBBitmap
//! - 200ms debounce delay after each touch
//!
//! ## Original Arduino Code
//!
//! ```cpp
//! static uint8_t Image_Flag = 0;
//!
//! void setup() {
//!   // ... initialization ...
//!   
//!   gfx->begin();
//!   gfx->fillScreen(RGB565_WHITE);
//!
//!   for (int i = 0; i <= 255; i++) {
//!     gfx->setBrightness(i);
//!     delay(5);
//!   }
//!
//!   gfx->fillScreen(RGB565_RED);
//!   delay(1000);
//!   gfx->fillScreen(RGB565_GREEN);
//!   delay(1000);
//!   gfx->fillScreen(RGB565_BLUE);
//!   delay(1000);
//!
//!   gfx->fillScreen(RGB565_WHITE);
//!   gfx->setCursor(60, 100);
//!   gfx->setTextSize(5);
//!   gfx->setTextColor(RGB565_BLACK);
//!   gfx->setCursor(60, 200);
//!   gfx->setTextSize(5);
//!   gfx->setTextColor(RGB565_BLACK);
//!   gfx->printf("Touch me");
//!
//!   USBSerial.printf("ID: %#X \n\n", (int32_t)FT3168->IIC_Read_Device_ID());
//!   delay(1000);
//! }
//!
//! void loop() {
//!   if (FT3168->IIC_Interrupt_Flag == true) {
//!     FT3168->IIC_Interrupt_Flag = false;
//!
//!     int32_t touch_x = FT3168->IIC_Read_Device_Value(TOUCH_COORDINATE_X);
//!     int32_t touch_y = FT3168->IIC_Read_Device_Value(TOUCH_COORDINATE_Y);
//!     uint8_t fingers_number = FT3168->IIC_Read_Device_Value(TOUCH_FINGER_NUMBER);
//!
//!     if (fingers_number > 0) {
//!       switch (Image_Flag) {
//!         case 0: gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)gImage_1, LCD_WIDTH, LCD_HEIGHT); break;
//!         case 1: gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)gImage_2, LCD_WIDTH, LCD_HEIGHT); break;
//!         case 2: gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)gImage_3, LCD_WIDTH, LCD_HEIGHT); break;
//!         case 3: gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)gImage_4, LCD_WIDTH, LCD_HEIGHT); break;
//!       }
//!
//!       Image_Flag++;
//!       if (Image_Flag > 3) {
//!         Image_Flag = 0;
//!       }
//!       delay(200);
//!     }
//!   }
//! }
//! ```

use embedded_graphics::{
    pixelcolor::Rgb888,
    prelude::*,
    text::Text,
    mono_font::MonoTextStyle,
};

// Constants from original example
const LCD_WIDTH: usize = 368;
const LCD_HEIGHT: usize = 448;
const NUM_IMAGES: usize = 4;

// Brightness fade constants
const BRIGHTNESS_FADE_STEPS: u8 = 255;
const BRIGHTNESS_FADE_DELAY_MS: u32 = 5;
const BRIGHTNESS_FADE_TOTAL_MS: u32 = BRIGHTNESS_FADE_STEPS as u32 * BRIGHTNESS_FADE_DELAY_MS; // ~1.3 seconds

// Color test delays
const COLOR_TEST_DELAY_MS: u32 = 1000;

// Touch debounce
const TOUCH_DEBOUNCE_MS: u32 = 200;

// Text position
const TEXT_X: i32 = 60;
const TEXT_Y: i32 = 200;
const TEXT_SIZE: u8 = 5;

// Colors
const RGB565_WHITE: Rgb888 = Rgb888::WHITE;
const RGB565_BLACK: Rgb888 = Rgb888::BLACK;
const RGB565_RED: Rgb888 = Rgb888::new(255, 0, 0);
const RGB565_GREEN: Rgb888 = Rgb888::new(0, 255, 0);
const RGB565_BLUE: Rgb888 = Rgb888::new(0, 0, 255);

// Image data would be embedded as:
// static IMAGE_1: &[u8] = include_bytes!("image1.rgb565");
// static IMAGE_2: &[u8] = include_bytes!("image2.rgb565");
// static IMAGE_3: &[u8] = include_bytes!("image3.rgb565");
// static IMAGE_4: &[u8] = include_bytes!("image4.rgb565");
//
// Each image is 368 * 448 * 2 = 329,728 bytes (RGB565 format, 2 bytes per pixel)

// Complete implementation:

// 1. Setup with brightness fade and color test
//    async fn setup(
//        display: &mut DisplayDriver,
//        i2c: &mut I2c,
//        touch: &mut TouchController,
//    ) -> Result<(), Error> {
//        // Initialize hardware (TCA9554, FT3168, display)
//        init_hardware(i2c, touch, display).await?;
//        
//        // Brightness fade-in
//        for brightness in 0..=BRIGHTNESS_FADE_STEPS {
//            display.set_brightness(brightness)?;
//            Timer::after_millis(BRIGHTNESS_FADE_DELAY_MS).await;
//        }
//        
//        // Color test sequence
//        display.fill_screen(RGB565_RED)?;
//        Timer::after_millis(COLOR_TEST_DELAY_MS).await;
//        
//        display.fill_screen(RGB565_GREEN)?;
//        Timer::after_millis(COLOR_TEST_DELAY_MS).await;
//        
//        display.fill_screen(RGB565_BLUE)?;
//        Timer::after_millis(COLOR_TEST_DELAY_MS).await;
//        
//        // Draw "Touch me" prompt
//        display.fill_screen(RGB565_WHITE)?;
//        draw_text(
//            display,
//            "Touch me",
//            Point::new(TEXT_X, TEXT_Y),
//            RGB565_BLACK,
//            TEXT_SIZE,
//        )?;
//        
//        // Read and display touch controller ID
//        let touch_id = touch.read_device_id()?;
//        esp_println::println!("FT3168 ID: 0x{:X}", touch_id);
//        
//        Timer::after_millis(1000).await;
//        
//        Ok(())
//    }

// 2. Convert RGB565 to RGB888 and draw bitmap
//    fn draw_rgb565_bitmap(
//        display: &mut impl DrawTarget<Color = Rgb888>,
//        x: i32,
//        y: i32,
//        rgb565_data: &[u8],
//        width: usize,
//        height: usize,
//    ) -> Result<(), DisplayError> {
//        // RGB565 format: 2 bytes per pixel
//        // Format: RRRRR GGGGGG BBBBB (5-6-5 bits, big-endian or little-endian)
//        // The original uses draw16bitRGBBitmap which expects uint16_t array
//        
//        for (i, chunk) in rgb565_data.chunks_exact(2).enumerate() {
//            // Convert from little-endian (or big-endian, check datasheet)
//            let pixel = u16::from_le_bytes([chunk[0], chunk[1]]);
//            
//            // Extract color components
//            let r5 = ((pixel >> 11) & 0x1F) as u8;
//            let g6 = ((pixel >> 5) & 0x3F) as u8;
//            let b5 = (pixel & 0x1F) as u8;
//            
//            // Expand to 8-bit
//            let r8 = (r5 << 3) | (r5 >> 2);
//            let g8 = (g6 << 2) | (g6 >> 4);
//            let b8 = (b5 << 3) | (b5 >> 2);
//            
//            // Calculate pixel position
//            let px = (i % width) as i32;
//            let py = (i / width) as i32;
//            
//            // Draw pixel
//            display.set_pixel(
//                Point::new(x + px, y + py),
//                Rgb888::new(r8, g8, b8),
//            )?;
//        }
//        
//        Ok(())
//    }

// 3. Main loop - image cycling
//    async fn main_loop(
//        display: &mut DisplayDriver,
//        touch: &mut TouchController,
//        images: &[&[u8]; NUM_IMAGES],
//    ) -> ! {
//        let mut image_flag: u8 = 0;
//        
//        loop {
//            // Check for touch interrupt
//            if touch.is_interrupt_set() {
//                touch.clear_interrupt();
//                
//                // Read touch data
//                let touch_x = touch.read_coordinate_x()?;
//                let touch_y = touch.read_coordinate_y()?;
//                let finger_count = touch.read_finger_count()?;
//                
//                esp_println::println!("Touch: X={}, Y={}, Fingers={}", touch_x, touch_y, finger_count);
//                
//                // Only cycle if at least one finger detected
//                if finger_count > 0 {
//                    // Select image based on current flag
//                    let image_data = match image_flag {
//                        0 => images[0],
//                        1 => images[1],
//                        2 => images[2],
//                        3 => images[3],
//                        _ => images[0],  // Wrap around
//                    };
//                    
//                    // Draw image full-screen
//                    draw_rgb565_bitmap(
//                        display,
//                        0,
//                        0,
//                        image_data,
//                        LCD_WIDTH,
//                        LCD_HEIGHT,
//                    )?;
//                    
//                    // Advance to next image (with wrap-around)
//                    image_flag = (image_flag + 1) % NUM_IMAGES as u8;
//                    
//                    // Debounce delay
//                    Timer::after_millis(TOUCH_DEBOUNCE_MS).await;
//                }
//            }
//            
//            Timer::after_millis(10).await;  // Check every 10ms
//        }
//    }

// Notes:
// - Images must be in RGB565 format (16-bit, 2 bytes per pixel)
// - Total image size: 368 * 448 * 2 = 329,728 bytes per image
// - 4 images = ~1.3 MB total (requires PSRAM on ESP32-S3)
// - The original uses draw16bitRGBBitmap which directly draws RGB565 data
// - Image data is typically embedded as a header file (16Bit_368x448px.h)
// - In Rust, use `include_bytes!` macro to embed image data
// - Consider using image compression (JPEG/PNG) and decompressing on-the-fly to save memory
// - The touch controller ID is read for debugging/verification
// - Color test sequence helps verify display colors are working correctly
// - Brightness fade creates a nice startup effect
// - The original has commented-out cases for images 5 and 6 (can be enabled)
