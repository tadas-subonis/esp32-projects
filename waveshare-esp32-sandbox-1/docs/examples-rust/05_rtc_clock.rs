//! # RTC Clock Display Example
//!
//! This example displays the current time from the PCF85063A real-time clock (RTC) on the display.
//! It updates the time display every second, showing the date and time in a formatted string.
//!
//! ## What This Example Does
//!
//! - Initializes the PCF85063A RTC via I2C (address 0x51)
//! - Sets an initial date and time: 2024-09-24 11:24:30
//! - Continuously reads the current time from the RTC every second
//! - Displays the time in format "YYYY-MM-DD HH:MM:SS" centered on the screen
//! - Only updates the display when the time string changes (to avoid flicker)
//! - Clears the previous time area (rectangle at y=150, height=50) before drawing new time
//! - Uses text size 3x3 (X scale, Y scale) with 0 pixel margin
//!
//! ## Original Arduino Code
//!
//! ```cpp
//! int16_t getCenteRGB565_REDX(const char *text, uint8_t textSize) {
//!   int16_t textWidth = strlen(text) * 6 * textSize;  // 6 pixels per character in default size
//!   return (LCD_WIDTH - textWidth) / 2;
//! }
//!
//! void setup() {
//!   if (!rtc.begin(Wire, IIC_SDA, IIC_SCL)) {
//!     while (1) { delay(1000); }
//!   }
//!
//!   rtc.setDateTime(2024, 9, 24, 11, 24, 30);
//!   gfx->begin();
//!   gfx->fillScreen(RGB565_WHITE);
//!   gfx->setBrightness(255);
//! }
//!
//! void loop() {
//!   if (millis() - lastMillis > 1000) {
//!     lastMillis = millis();
//!
//!     RTC_DateTime datetime = rtc.getDateTime();
//!
//!     char timeString[20];
//!     sprintf(timeString, "%04d-%02d-%02d %02d:%02d:%02d",
//!             datetime.getYear(), datetime.getMonth(), datetime.getDay(),
//!             datetime.getHour(), datetime.getMinute(), datetime.getSecond());
//!
//!     if (strcmp(timeString, previousTimeString) != 0) {
//!       gfx->fillRect(0, 150, LCD_WIDTH, 50, RGB565_WHITE);
//!       gfx->setTextColor(RGB565_BLACK);
//!       gfx->setTextSize(3,3,0);
//!       int16_t timeX = getCenteRGB565_REDX(timeString, 3);
//!       gfx->setCursor(timeX, 150);
//!       gfx->println(timeString);
//!       strcpy(previousTimeString, timeString);
//!     }
//!   }
//! }
//! ```

use embedded_graphics::{
    mono_font::{MonoTextStyle, ascii::FONT_10X20},
    pixelcolor::Rgb888,
    prelude::*,
    text::Text,
    primitives::{Rectangle, PrimitiveStyle},
};
use core::fmt::Write;
use heapless::String;
use esp_hal::{
    i2c::master::I2c,
    delay::Delay,
};

// Constants from original example
const PCF85063A_ADDR: u8 = 0x51;
const LCD_WIDTH: usize = 368;
const LCD_HEIGHT: usize = 448;

// Initial date/time (from original example)
const INIT_YEAR: u16 = 2024;
const INIT_MONTH: u8 = 9;
const INIT_DAY: u8 = 24;
const INIT_HOUR: u8 = 11;
const INIT_MINUTE: u8 = 24;
const INIT_SECOND: u8 = 30;

// Display constants
const TIME_AREA_Y: i32 = 150;
const TIME_AREA_HEIGHT: i32 = 50;
const TEXT_SIZE: u8 = 3;  // X and Y scale
const PIXEL_MARGIN: u8 = 0;
const CHAR_WIDTH_DEFAULT: usize = 6;  // Pixels per character in default size
const UPDATE_INTERVAL_MS: u32 = 1000;

// Colors
const RGB565_WHITE: Rgb888 = Rgb888::WHITE;
const RGB565_BLACK: Rgb888 = Rgb888::BLACK;

// PCF85063A register addresses
const RTC_CONTROL_1: u8 = 0x00;
const RTC_CONTROL_2: u8 = 0x01;
const RTC_SECONDS: u8 = 0x02;
const RTC_MINUTES: u8 = 0x03;
const RTC_HOURS: u8 = 0x04;
const RTC_DAYS: u8 = 0x05;
const RTC_WEEKDAYS: u8 = 0x06;
const RTC_MONTHS: u8 = 0x07;
const RTC_YEARS: u8 = 0x08;

// BCD conversion helpers
fn to_bcd(value: u8) -> u8 {
    ((value / 10) << 4) | (value % 10)
}

fn from_bcd(bcd: u8) -> u8 {
    ((bcd >> 4) * 10) + (bcd & 0x0F)
}

// Helper function to center text horizontally (from original)
fn get_center_x(text: &str, text_size: u8) -> i32 {
    let text_width = (text.len() * CHAR_WIDTH_DEFAULT * text_size as usize) as i32;
    (LCD_WIDTH as i32 - text_width) / 2
}

// Complete implementation:

// 1. Initialize RTC
//    fn init_rtc(i2c: &mut I2c) -> Result<(), RtcError> {
//        // Check if RTC is present by reading control register
//        let mut status = [0u8; 1];
//        i2c.write_read(PCF85063A_ADDR, &[RTC_CONTROL_1], &mut status)
//            .map_err(|_| RtcError::Communication)?;
//        
//        // RTC found, proceed
//        Ok(())
//    }

// 2. Set RTC date and time
//    fn set_rtc_datetime(
//        i2c: &mut I2c,
//        year: u16,
//        month: u8,
//        day: u8,
//        hour: u8,
//        minute: u8,
//        second: u8,
//    ) -> Result<(), RtcError> {
//        // Convert to BCD format
//        let sec_bcd = to_bcd(second);
//        let min_bcd = to_bcd(minute);
//        let hour_bcd = to_bcd(hour);
//        let day_bcd = to_bcd(day);
//        let month_bcd = to_bcd(month);
//        let year_bcd = to_bcd((year % 100) as u8);  // Year as 2-digit
//        
//        // Write to RTC registers starting from seconds
//        i2c.write(
//            PCF85063A_ADDR,
//            &[
//                RTC_SECONDS,
//                sec_bcd,
//                min_bcd,
//                hour_bcd,
//                day_bcd,
//                month_bcd,
//                year_bcd,
//            ],
//        )
//        .map_err(|_| RtcError::Communication)?;
//        
//        Ok(())
//    }

// 3. Read RTC date and time
//    fn read_rtc_datetime(i2c: &mut I2c) -> Result<DateTime, RtcError> {
//        let mut data = [0u8; 7];
//        
//        // Read time registers (seconds through years)
//        i2c.write_read(PCF85063A_ADDR, &[RTC_SECONDS], &mut data)
//            .map_err(|_| RtcError::Communication)?;
//        
//        // Convert from BCD
//        let second = from_bcd(data[0] & 0x7F);  // Mask out OS bit
//        let minute = from_bcd(data[1] & 0x7F);
//        let hour = from_bcd(data[2] & 0x3F);     // 24-hour format
//        let day = from_bcd(data[3] & 0x3F);
//        let month = from_bcd(data[4] & 0x1F);
//        let year = 2000 + from_bcd(data[5]) as u16;  // Assume 2000-2099
//        
//        Ok(DateTime {
//            year,
//            month,
//            day,
//            hour,
//            minute,
//            second,
//        })
//    }

// 4. Format datetime as string
//    fn format_datetime(dt: &DateTime) -> String<20> {
//        let mut time_string = String::<20>::new();
//        write!(
//            time_string,
//            "{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
//            dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second
//        )
//        .ok();
//        time_string
//    }

// 5. Setup function
//    async fn setup(
//        display: &mut DisplayDriver,
//        i2c: &mut I2c,
//    ) -> Result<(), Error> {
//        // Initialize RTC
//        init_rtc(i2c)?;
//        esp_println::println!("RTC initialized");
//        
//        // Set initial date/time
//        set_rtc_datetime(
//            i2c,
//            INIT_YEAR,
//            INIT_MONTH,
//            INIT_DAY,
//            INIT_HOUR,
//            INIT_MINUTE,
//            INIT_SECOND,
//        )?;
//        esp_println::println!("RTC set to {}-{:02}-{:02} {:02}:{:02}:{:02}",
//            INIT_YEAR, INIT_MONTH, INIT_DAY, INIT_HOUR, INIT_MINUTE, INIT_SECOND);
//        
//        // Initialize display
//        display.begin()?;
//        display.fill_screen(RGB565_WHITE)?;
//        display.set_brightness(255)?;
//        
//        Ok(())
//    }

// 6. Main loop
//    async fn main_loop(
//        display: &mut DisplayDriver,
//        i2c: &mut I2c,
//    ) -> ! {
//        let mut last_update_ms = 0u32;
//        let mut previous_time_string = String::<20>::new();
//        
//        loop {
//            let current_ms = get_millis();
//            
//            // Update every second
//            if current_ms - last_update_ms >= UPDATE_INTERVAL_MS {
//                last_update_ms = current_ms;
//                
//                // Read current time from RTC
//                match read_rtc_datetime(i2c) {
//                    Ok(datetime) => {
//                        // Format as string
//                        let time_string = format_datetime(&datetime);
//                        
//                        // Only update display if time string changed
//                        if time_string != previous_time_string {
//                            // Clear previous time area
//                            Rectangle::new(
//                                Point::new(0, TIME_AREA_Y),
//                                Size::new(LCD_WIDTH as u32, TIME_AREA_HEIGHT as u32),
//                            )
//                            .into_styled(PrimitiveStyle::with_fill(RGB565_WHITE))
//                            .draw(display)
//                            .ok();
//                            
//                            // Calculate centered X position
//                            let center_x = get_center_x(time_string.as_str(), TEXT_SIZE);
//                            
//                            // Draw new time (using larger font for size 3)
//                            // Note: embedded-graphics doesn't support arbitrary scaling,
//                            // so you'd need to use a larger font or implement scaling
//                            Text::new(
//                                time_string.as_str(),
//                                Point::new(center_x, TIME_AREA_Y),
//                                MonoTextStyle::new(&FONT_10X20, RGB565_BLACK),
//                            )
//                            .draw(display)
//                            .ok();
//                            
//                            // Save current time string
//                            previous_time_string = time_string;
//                        }
//                    }
//                    Err(e) => {
//                        esp_println::println!("RTC read error: {:?}", e);
//                    }
//                }
//            }
//            
//            Timer::after_millis(100).await;  // Check every 100ms
//        }
//    }

// DateTime structure
// #[derive(Debug, Clone, Copy, PartialEq, Eq)]
// struct DateTime {
//     year: u16,
//     month: u8,
//     day: u8,
//     hour: u8,
//     minute: u8,
//     second: u8,
// }

// Notes:
// - PCF85063A uses BCD (Binary Coded Decimal) format for all time values
// - The RTC maintains time even when ESP32 is powered off (if battery is connected)
// - The original example uses `setTextSize(3,3,0)` which scales text 3x in both dimensions
// - For embedded-graphics, you'd need to use a larger font or implement manual scaling
// - The time format "YYYY-MM-DD HH:MM:SS" is 19 characters
// - Text centering calculation: (LCD_WIDTH - text_width) / 2
// - Only updating when time changes prevents unnecessary screen flicker
// - Consider adding timezone support for international applications
// - For production, synchronize with NTP for accurate time
