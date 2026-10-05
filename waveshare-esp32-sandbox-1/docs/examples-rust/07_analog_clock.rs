//! # Analog Clock Example
//!
//! This example displays a beautiful analog clock face with hour, minute, and second hands.
//! The clock updates smoothly every second, showing the current time with animated hands.
//!
//! ## What This Example Does
//!
//! - Calculates clock center and hand lengths based on display dimensions
//! - Draws 60 clock marks around the face (hour marks every 15 minutes, 5-minute marks, minute marks)
//! - Initializes time from compile time (or system time)
//! - Updates time every second
//! - Calculates hand positions using trigonometry with millisecond precision
//! - Uses cached pixel positions to efficiently erase old hand positions
//! - Handles hand overlap detection to prevent one hand from erasing another
//! - Only redraws when second hand position changes
//!
//! ## Original Arduino Code Key Algorithms
//!
//! ### Hand Position Calculation
//! ```cpp
//! // Second hand angle (includes millisecond fraction for smooth movement)
//! sdeg = SIXTIETH_RADIAN * ((0.001 * (cur_millis % 1000)) + ss);
//! nsx = cos(sdeg - RIGHT_ANGLE_RADIAN) * sHandLen + center;
//! nsy = sin(sdeg - RIGHT_ANGLE_RADIAN) * sHandLen + center;
//!
//! // Minute hand (includes second fraction)
//! mdeg = (SIXTIETH * sdeg) + (SIXTIETH_RADIAN * mm);
//! mdeg -= RIGHT_ANGLE_RADIAN;
//! nmx = cos(mdeg) * mHandLen + center;
//! nmy = sin(mdeg) * mHandLen + center;
//!
//! // Hour hand (includes minute fraction)
//! hdeg = (TWELFTH * mdeg) + (TWELFTH_RADIAN * hh);
//! hdeg -= RIGHT_ANGLE_RADIAN;
//! nhx = cos(hdeg) * hHandLen + center;
//! nhy = sin(hdeg) * hHandLen + center;
//! ```
//!
//! ### Cached Line Drawing (Bresenham's algorithm with caching)
//! The original uses a sophisticated cached line drawing algorithm that:
//! - Stores all pixels drawn in a cache array
//! - Only redraws pixels that changed position
//! - Erases old pixels by drawing them in background color
//! - Checks for hand overlap to prevent erasing one hand when drawing another

use embedded_graphics::{
    pixelcolor::Rgb888,
    prelude::*,
    primitives::{Line, PrimitiveStyle},
};
use libm::{cosf, sinf, fabsf};

// Constants from original example
const LCD_WIDTH: usize = 368;
const LCD_HEIGHT: usize = 448;

// Color constants
const BACKGROUND: Rgb888 = Rgb888::BLACK;
const MARK_COLOR: Rgb888 = Rgb888::WHITE;
const SUBMARK_COLOR: Rgb888 = Rgb888::new(64, 64, 64);  // Dark grey
const HOUR_COLOR: Rgb888 = Rgb888::WHITE;
const MINUTE_COLOR: Rgb888 = Rgb888::new(0, 0, 255);    // Blue
const SECOND_COLOR: Rgb888 = Rgb888::new(255, 0, 0);   // Red

// Mathematical constants (exact values from original)
const SIXTIETH: f32 = 0.016666667;        // 1/60
const TWELFTH: f32 = 0.08333333;          // 1/12
const SIXTIETH_RADIAN: f32 = 0.10471976;  // π/30
const TWELFTH_RADIAN: f32 = 0.52359878;   // π/6
const RIGHT_ANGLE_RADIAN: f32 = 1.5707963; // π/2

// Complete implementation:

// 1. Initialize clock geometry
//    fn init_clock_geometry() -> ClockGeometry {
//        let w = LCD_WIDTH;
//        let h = LCD_HEIGHT;
//        
//        // Center is the smaller dimension divided by 2
//        let center = if w < h { w / 2 } else { h / 2 };
//        
//        // Hand lengths as fractions of radius
//        let hour_hand_len = center * 3 / 8;    // 3/8 of radius
//        let minute_hand_len = center * 2 / 3;  // 2/3 of radius
//        let second_hand_len = center * 5 / 6;  // 5/6 of radius
//        let mark_len = second_hand_len / 6;
//        
//        // Allocate cache for hand pixel positions
//        // Cache size: (hour_len + 1 + minute_len + 1 + second_len + 1) * 2 * 2 bytes
//        // Each pixel needs 2 coordinates (x, y), each coordinate is 2 bytes (int16_t)
//        let cache_size = (hour_hand_len + 1 + minute_hand_len + 1 + second_hand_len + 1) * 2 * 2;
//        let cached_points = alloc::vec::Vec::<i16>::with_capacity(cache_size);
//        
//        ClockGeometry {
//            center: center as i32,
//            hour_hand_len,
//            minute_hand_len,
//            second_hand_len,
//            mark_len,
//            cached_points,
//        }
//    }

// 2. Draw clock face marks (60 marks total)
//    fn draw_clock_marks(
//        display: &mut impl DrawTarget<Color = Rgb888>,
//        geom: &ClockGeometry,
//    ) {
//        let center = geom.center;
//        
//        // Three different mark sizes:
//        // - Hour marks (every 15 minutes): innerR1 to outerR1
//        // - 5-minute marks: innerR2 to outerR2
//        // - Minute marks: innerR3 to outerR3
//        let inner_r1 = center - geom.mark_len;
//        let outer_r1 = center;
//        let inner_r2 = center - (geom.mark_len * 2 / 3);
//        let outer_r2 = center;
//        let inner_r3 = center - (geom.mark_len / 2);
//        let outer_r3 = center;
//        
//        for i in 0..60 {
//            let (inner_r, outer_r, color) = if i % 15 == 0 {
//                // Hour marks (12, 3, 6, 9 o'clock)
//                (inner_r1, outer_r1, MARK_COLOR)
//            } else if i % 5 == 0 {
//                // 5-minute marks
//                (inner_r2, outer_r2, MARK_COLOR)
//            } else {
//                // Minute marks
//                (inner_r3, outer_r3, SUBMARK_COLOR)
//            };
//            
//            // Calculate mark position using trigonometry
//            let angle = (SIXTIETH_RADIAN * i as f32) - RIGHT_ANGLE_RADIAN;
//            let cos_a = cosf(angle);
//            let sin_a = sinf(angle);
//            
//            let x0 = (cos_a * outer_r as f32) as i32 + center;
//            let y0 = (sin_a * outer_r as f32) as i32 + center;
//            let x1 = (cos_a * inner_r as f32) as i32 + center;
//            let y1 = (sin_a * inner_r as f32) as i32 + center;
//            
//            Line::new(Point::new(x0, y0), Point::new(x1, y1))
//                .into_styled(PrimitiveStyle::with_stroke(color, 1))
//                .draw(display)
//                .ok();
//        }
//    }

// 3. Convert compile-time string to number (from original conv2d function)
//    fn conv2d(s: &str) -> u8 {
//        // Converts 2-digit string to number
//        // e.g., "12" -> 12
//        if s.len() >= 2 {
//            let tens = s.chars().nth(0).unwrap_or('0') as u8 - b'0';
//            let ones = s.chars().nth(1).unwrap_or('0') as u8 - b'0';
//            tens * 10 + ones
//        } else {
//            0
//        }
//    }

// 4. Calculate hand positions with millisecond precision
//    fn calculate_hand_positions(
//        hour: u8,
//        minute: u8,
//        second: u8,
//        millis: u32,
//        geom: &ClockGeometry,
//    ) -> HandPositions {
//        let center = geom.center as f32;
//        
//        // Second hand angle (includes millisecond fraction for smooth movement)
//        let millis_fraction = (millis % 1000) as f32 * 0.001;
//        let sdeg = SIXTIETH_RADIAN * (millis_fraction + second as f32);
//        let second_x = (cosf(sdeg - RIGHT_ANGLE_RADIAN) * geom.second_hand_len as f32) as i32 + geom.center;
//        let second_y = (sinf(sdeg - RIGHT_ANGLE_RADIAN) * geom.second_hand_len as f32) as i32 + geom.center;
//        
//        // Minute hand angle (includes second fraction)
//        let mdeg = (SIXTIETH * sdeg) + (SIXTIETH_RADIAN * minute as f32);
//        let mdeg_adjusted = mdeg - RIGHT_ANGLE_RADIAN;
//        let minute_x = (cosf(mdeg_adjusted) * geom.minute_hand_len as f32) as i32 + geom.center;
//        let minute_y = (sinf(mdeg_adjusted) * geom.minute_hand_len as f32) as i32 + geom.center;
//        
//        // Hour hand angle (includes minute fraction)
//        let hdeg = (TWELFTH * mdeg) + (TWELFTH_RADIAN * hour as f32);
//        let hdeg_adjusted = hdeg - RIGHT_ANGLE_RADIAN;
//        let hour_x = (cosf(hdeg_adjusted) * geom.hour_hand_len as f32) as i32 + geom.center;
//        let hour_y = (sinf(hdeg_adjusted) * geom.hour_hand_len as f32) as i32 + geom.center;
//        
//        HandPositions {
//            second: Point::new(second_x, second_y),
//            minute: Point::new(minute_x, minute_y),
//            hour: Point::new(hour_x, hour_y),
//        }
//    }

// 5. Draw and erase cached line (Bresenham's algorithm with caching)
//    fn draw_and_erase_cached_line(
//        display: &mut impl DrawTarget<Color = Rgb888>,
//        x0: i32,
//        y0: i32,
//        x1: i32,
//        y1: i32,
//        color: Rgb888,
//        cache: &mut [i16],
//        cache_len: usize,
//        cross_check_second: bool,
//        cross_check_hour: bool,
//        second_cache: &[i16],
//        hour_cache: &[i16],
//    ) {
//        // Determine if line is steep (more vertical than horizontal)
//        let steep = fabsf((y1 - y0) as f32) > fabsf((x1 - x0) as f32);
//        
//        let (mut x0, mut y0, mut x1, mut y1) = if steep {
//            (y0, x0, y1, x1)  // Swap x and y
//        } else {
//            (x0, y0, x1, y1)
//        };
//        
//        let dx = (x1 - x0).abs();
//        let dy = (y1 - y0).abs();
//        
//        let mut err = dx / 2;
//        let xstep = if x0 < x1 { 1 } else { -1 };
//        let ystep = if y0 < y1 { 1 } else { -1 };
//        let x1_end = x1 + xstep;
//        
//        // Draw line using Bresenham's algorithm
//        for i in 0..=dx {
//            let (x, y) = if steep {
//                (y0, x0)  // Unswap
//            } else {
//                (x0, y0)
//            };
//            
//            // Get old position from cache
//            let cache_idx = (i * 2) as usize;
//            let ox = if cache_idx < cache.len() { cache[cache_idx] } else { 0 };
//            let oy = if cache_idx + 1 < cache.len() { cache[cache_idx + 1] } else { 0 };
//            
//            if x == ox && y == oy {
//                // Pixel hasn't moved, but may need to check for overlap
//                if cross_check_second || cross_check_hour {
//                    write_cache_pixel(
//                        display,
//                        x,
//                        y,
//                        color,
//                        cross_check_second,
//                        cross_check_hour,
//                        second_cache,
//                        hour_cache,
//                    );
//                }
//            } else {
//                // Pixel moved - draw new position, erase old
//                write_cache_pixel(
//                    display,
//                    x,
//                    y,
//                    color,
//                    cross_check_second,
//                    cross_check_hour,
//                    second_cache,
//                    hour_cache,
//                );
//                
//                if ox > 0 || oy > 0 {
//                    // Erase old position
//                    write_cache_pixel(
//                        display,
//                        ox as i32,
//                        oy as i32,
//                        BACKGROUND,
//                        cross_check_second,
//                        cross_check_hour,
//                        second_cache,
//                        hour_cache,
//                    );
//                }
//                
//                // Update cache
//                if cache_idx < cache.len() {
//                    cache[cache_idx] = x;
//                }
//                if cache_idx + 1 < cache.len() {
//                    cache[cache_idx + 1] = y;
//                }
//            }
//            
//            // Bresenham's algorithm step
//            if err < dy {
//                y0 += ystep;
//                err += dx;
//            }
//            err -= dy;
//            x0 += xstep;
//        }
//        
//        // Clear remaining cache entries (for when hand gets shorter)
//        for i in (dx + 1)..cache_len {
//            let cache_idx = (i * 2) as usize;
//            let ox = if cache_idx < cache.len() { cache[cache_idx] } else { 0 };
//            let oy = if cache_idx + 1 < cache.len() { cache[cache_idx + 1] } else { 0 };
//            
//            if ox > 0 || oy > 0 {
//                write_cache_pixel(
//                    display,
//                    ox as i32,
//                    oy as i32,
//                    BACKGROUND,
//                    cross_check_second,
//                    cross_check_hour,
//                    second_cache,
//                    hour_cache,
//                );
//            }
//            
//            // Clear cache entry
//            if cache_idx < cache.len() {
//                cache[cache_idx] = 0;
//            }
//            if cache_idx + 1 < cache.len() {
//                cache[cache_idx + 1] = 0;
//            }
//        }
//    }

// 6. Write pixel with overlap checking
//    fn write_cache_pixel(
//        display: &mut impl DrawTarget<Color = Rgb888>,
//        x: i32,
//        y: i32,
//        color: Rgb888,
//        cross_check_second: bool,
//        cross_check_hour: bool,
//        second_cache: &[i16],
//        hour_cache: &[i16],
//    ) {
//        // Check if this pixel is part of second hand (don't overwrite)
//        if cross_check_second {
//            for i in 0..second_cache.len() / 2 {
//                let cache_x = second_cache[i * 2];
//                let cache_y = second_cache[i * 2 + 1];
//                if cache_x == x as i16 && cache_y == y as i16 {
//                    return;  // Don't overwrite second hand
//                }
//            }
//        }
//        
//        // Check if this pixel is part of hour hand (don't overwrite)
//        if cross_check_hour {
//            for i in 0..hour_cache.len() / 2 {
//                let cache_x = hour_cache[i * 2];
//                let cache_y = hour_cache[i * 2 + 1];
//                if cache_x == x as i16 && cache_y == y as i16 {
//                    return;  // Don't overwrite hour hand
//                }
//            }
//        }
//        
//        // Safe to draw pixel
//        display.set_pixel(Point::new(x, y), color).ok();
//    }

// 7. Main loop
//    async fn main_loop(
//        display: &mut DisplayDriver,
//        geom: &mut ClockGeometry,
//    ) -> ! {
//        // Initialize time (from compile time or system time)
//        let mut hour = 12;  // Would be from __TIME__ or RTC
//        let mut minute = 0;
//        let mut second = 0;
//        let mut target_time_ms = get_millis() + 1000;
//        
//        // Previous hand positions
//        let mut old_second = Point::new(0, 0);
//        let mut old_minute = Point::new(0, 0);
//        let mut old_hour = Point::new(0, 0);
//        
//        // Cache offsets
//        let second_cache_offset = 0;
//        let hour_cache_offset = (geom.second_hand_len + 1) * 2;
//        let minute_cache_offset = hour_cache_offset + (geom.hour_hand_len + 1) * 2;
//        
//        loop {
//            let current_ms = get_millis();
//            
//            // Advance time every second
//            if current_ms >= target_time_ms {
//                target_time_ms += 1000;
//                second += 1;
//                if second >= 60 {
//                    second = 0;
//                    minute += 1;
//                    if minute >= 60 {
//                        minute = 0;
//                        hour = (hour + 1) % 24;
//                    }
//                }
//            }
//            
//            // Calculate new hand positions
//            let positions = calculate_hand_positions(
//                hour,
//                minute,
//                second,
//                current_ms,
//                geom,
//            );
//            
//            // Only redraw if second hand moved
//            if positions.second != old_second {
//                // Draw hands with caching and overlap detection
//                display.start_write();
//                
//                // Second hand (no overlap check needed - drawn first)
//                draw_and_erase_cached_line(
//                    display,
//                    geom.center,
//                    geom.center,
//                    positions.second.x,
//                    positions.second.y,
//                    SECOND_COLOR,
//                    &mut geom.cached_points[second_cache_offset..],
//                    geom.second_hand_len + 1,
//                    false,
//                    false,
//                    &[],
//                    &[],
//                );
//                
//                // Hour hand (check against second hand)
//                draw_and_erase_cached_line(
//                    display,
//                    geom.center,
//                    geom.center,
//                    positions.hour.x,
//                    positions.hour.y,
//                    HOUR_COLOR,
//                    &mut geom.cached_points[hour_cache_offset..],
//                    geom.hour_hand_len + 1,
//                    true,  // cross_check_second
//                    false,
//                    &geom.cached_points[second_cache_offset..second_cache_offset + (geom.second_hand_len + 1) * 2],
//                    &[],
//                );
//                
//                // Minute hand (check against both second and hour hands)
//                draw_and_erase_cached_line(
//                    display,
//                    geom.center,
//                    geom.center,
//                    positions.minute.x,
//                    positions.minute.y,
//                    MINUTE_COLOR,
//                    &mut geom.cached_points[minute_cache_offset..],
//                    geom.minute_hand_len + 1,
//                    true,  // cross_check_second
//                    true,  // cross_check_hour
//                    &geom.cached_points[second_cache_offset..second_cache_offset + (geom.second_hand_len + 1) * 2],
//                    &geom.cached_points[hour_cache_offset..hour_cache_offset + (geom.hour_hand_len + 1) * 2],
//                );
//                
//                display.end_write();
//                
//                // Update old positions
//                old_second = positions.second;
//                old_minute = positions.minute;
//                old_hour = positions.hour;
//            }
//            
//            Timer::after_millis(1).await;  // Check every millisecond for smooth animation
//        }
//    }

// Structures
// #[derive(Debug, Clone, Copy)]
// struct ClockGeometry {
//     center: i32,
//     hour_hand_len: i32,
//     minute_hand_len: i32,
//     second_hand_len: i32,
//     mark_len: i32,
//     cached_points: alloc::vec::Vec<i16>,
// }
//
// #[derive(Debug, Clone, Copy, PartialEq, Eq)]
// struct HandPositions {
//     second: Point,
//     minute: Point,
//     hour: Point,
// }

// Notes:
// - The original uses malloc for cache allocation; in Rust use Vec or fixed-size arrays
// - Hand positions are calculated with millisecond precision for smooth animation
// - The cached drawing algorithm only updates pixels that changed, making it very efficient
// - Overlap detection prevents one hand from erasing another when they cross
// - The clock starts from compile time (__TIME__) but could use RTC or NTP
// - RIGHT_ANGLE_RADIAN (π/2) is subtracted to rotate clock so 12 o'clock is at top
// - Hand lengths are fractions of the clock radius for proper proportions
// - The original uses `writePixel()` for individual pixel writes (very low-level)
// - Consider using a framebuffer for better performance with frequent updates
