//! # WiFi Analyzer Example
//!
//! This example creates a WiFi network analyzer that scans for nearby WiFi networks and displays
//! them in a visual graph showing signal strength by channel. It helps identify the best WiFi
//! channels to use and shows network congestion.
//!
//! ## What This Example Does
//!
//! - Sets WiFi to station mode and disconnects from any existing connection
//! - Calculates display layout: banner height, graph area, channel widths
//! - Draws banner with "ESP32 WiFi Analyzer" text in colored segments
//! - Scans for WiFi networks (non-async, showing hidden networks)
//! - Processes each network:
//!   - Tracks peak RSSI per channel
//!   - Counts APs per channel (avoiding duplicates by BSSID prefix)
//!   - Calculates noise contribution across overlapping channels (±4 channels)
//! - Draws signal bars as ellipses for each network
//! - Shows SSID, RSSI, and encryption status for strong signals (RSSI >= -70 dBm)
//! - Calculates and displays least noisy channels (channels 1-11)
//! - Draws channel numbers and AP counts at bottom
//! - Updates every 3 seconds
//!
//! ## Original Arduino Code Key Algorithms
//!
//! ### Noise Calculation
//! ```cpp
//! int32_t noise = rssi - RSSI_FLOOR;  // e.g., -80 - (-100) = 20
//! noise *= noise;  // 20 * 20 = 400
//! // Add to current channel and ±4 neighboring channels
//! if (channel > 4) noise_list[idx - 4] += noise;
//! if (channel > 3) noise_list[idx - 3] += noise;
//! if (channel > 2) noise_list[idx - 2] += noise;
//! if (channel > 1) noise_list[idx - 1] += noise;
//! noise_list[idx] += noise;
//! if (channel < 14) noise_list[idx + 1] += noise;
//! if (channel < 13) noise_list[idx + 2] += noise;
//! if (channel < 12) noise_list[idx + 3] += noise;
//! if (channel < 11) noise_list[idx + 4] += noise;
//! ```
//!
//! ### Signal Bar Height Calculation
//! ```cpp
//! height = constrain(map(rssi, RSSI_FLOOR, RSSI_CEILING, 1, graph_height), 1, graph_height);
//! // map: linear interpolation from RSSI_FLOOR..RSSI_CEILING to 1..graph_height
//! ```
//!
//! ### BSSID Duplicate Detection
//! ```cpp
//! bool matchBssidPrefix(uint8_t *a, uint8_t *b) {
//!   for (uint8_t i = 0; i < 5; i++) {  // only compare first 5 bytes
//!     if (a[i] != b[i]) return false;
//!   }
//!   return true;
//! }
//! ```

use embedded_graphics::{
    pixelcolor::Rgb888,
    prelude::*,
    primitives::{Rectangle, Line, Ellipse, PrimitiveStyle},
    text::Text,
    mono_font::MonoTextStyle,
};
use heapless::String;

// Constants from original example
const LCD_WIDTH: usize = 368;
const LCD_HEIGHT: usize = 448;

// RSSI range constants
const RSSI_CEILING: i32 = -30;   // Best signal strength (dBm)
const RSSI_SHOW_SSID: i32 = -70; // Show SSID if signal is this good (dBm)
const RSSI_FLOOR: i32 = -100;    // Worst signal strength (dBm)

// Channel mapping (14 channels mapped to display positions)
const CHANNEL_LEGEND: [u8; 14] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14];

// Channel colors (RGB565 values converted to Rgb888)
const CHANNEL_COLORS: [Rgb888; 14] = [
    Rgb888::new(255, 0, 0),       // Red
    Rgb888::new(255, 165, 0),     // Orange
    Rgb888::new(255, 255, 0),     // Yellow
    Rgb888::new(0, 255, 0),       // Lime
    Rgb888::new(0, 255, 255),     // Cyan
    Rgb888::new(0, 0, 255),       // Blue
    Rgb888::new(255, 0, 255),     // Magenta
    Rgb888::new(255, 0, 0),       // Red (repeat)
    Rgb888::new(255, 165, 0),     // Orange
    Rgb888::new(255, 255, 0),     // Yellow
    Rgb888::new(0, 255, 0),       // Lime
    Rgb888::new(0, 255, 255),    // Cyan
    Rgb888::new(0, 0, 255),       // Blue
    Rgb888::new(255, 0, 255),     // Magenta
];

const SCAN_INTERVAL_MS: u32 = 3000;

// Complete implementation:

// 1. Calculate display layout (from setup)
//    fn calculate_layout() -> Layout {
//        let w = LCD_WIDTH;
//        let h = LCD_HEIGHT;
//        let banner_text_size = if w < 300 { 1 } else { 2 };
//        let banner_height = (banner_text_size * 8) + 2;
//        let graph_height = h - banner_height - 30;  // minus 3 text lines
//        let graph_baseline = banner_height + 10 + graph_height;
//        let channel_width = w / 16;
//        let signal_width = channel_width * 2;
//        
//        Layout {
//            banner_text_size,
//            banner_height,
//            graph_height,
//            graph_baseline,
//            channel_width,
//            signal_width,
//        }
//    }

// 2. Draw banner (from setup)
//    fn draw_banner(display: &mut impl DrawTarget<Color = Rgb888>, layout: &Layout) {
//        // Fill banner area with purple
//        Rectangle::new(
//            Point::new(0, 0),
//            Size::new(LCD_WIDTH as u32, layout.banner_height as u32),
//        )
//        .into_styled(PrimitiveStyle::with_fill(Rgb888::new(128, 0, 128)))
//        .draw(display)
//        .ok();
//        
//        // Draw "ESP32 WiFi Analyzer" text with colored segments
//        // " ESP" in white on red
//        // "32 " in white on dark orange
//        // " WiFi " in white on medium blue
//        // " Analyzer" in white on purple
//        // (Implementation would draw each segment with different background)
//    }

// 3. Map RSSI to bar height
//    fn map_rssi_to_height(rssi: i32, graph_height: usize) -> usize {
//        // Linear interpolation: map from RSSI_FLOOR..RSSI_CEILING to 1..graph_height
//        let rssi_range = RSSI_CEILING - RSSI_FLOOR;  // 70
//        let height_range = graph_height - 1;
//        
//        let normalized = (rssi - RSSI_FLOOR) as usize;
//        let height = ((normalized * height_range) / rssi_range as usize) + 1;
//        
//        // Constrain to valid range
//        height.max(1).min(graph_height)
//    }

// 4. Check if BSSID prefixes match (first 5 bytes)
//    fn match_bssid_prefix(a: &[u8], b: &[u8]) -> bool {
//        if a.len() < 5 || b.len() < 5 {
//            return false;
//        }
//        for i in 0..5 {
//            if a[i] != b[i] {
//                return false;
//            }
//        }
//        true
//    }

// 5. Calculate noise contribution for a channel
//    fn update_noise_list(noise_list: &mut [i32; 14], channel: u8, rssi: i32) {
//        let idx = (channel - 1) as usize;
//        
//        // Calculate noise: (rssi - RSSI_FLOOR)^2
//        let noise = (rssi - RSSI_FLOOR).pow(2);
//        
//        // Add noise to current channel and ±4 neighboring channels
//        // (WiFi channels overlap, so a signal on one channel affects neighbors)
//        if channel > 4 && idx >= 4 {
//            noise_list[idx - 4] += noise;
//        }
//        if channel > 3 && idx >= 3 {
//            noise_list[idx - 3] += noise;
//        }
//        if channel > 2 && idx >= 2 {
//            noise_list[idx - 2] += noise;
//        }
//        if channel > 1 && idx >= 1 {
//            noise_list[idx - 1] += noise;
//        }
//        noise_list[idx] += noise;
//        if channel < 14 && idx < 13 {
//            noise_list[idx + 1] += noise;
//        }
//        if channel < 13 && idx < 12 {
//            noise_list[idx + 2] += noise;
//        }
//        if channel < 12 && idx < 11 {
//            noise_list[idx + 3] += noise;
//        }
//        if channel < 11 && idx < 10 {
//            noise_list[idx + 4] += noise;
//        }
//    }

// 6. Main scan and display loop
//    async fn main_loop(
//        display: &mut DisplayDriver,
//        wifi: &mut WifiStation,
//        layout: &Layout,
//    ) -> ! {
//        loop {
//            // Initialize statistics arrays
//            let mut ap_count_list = [0u8; 14];
//            let mut noise_list = [RSSI_FLOOR; 14];
//            let mut peak_list = [RSSI_FLOOR; 14];
//            let mut peak_id_list = [-1i16; 14];
//            
//            // Scan for networks (non-async, show hidden)
//            let networks = wifi.scan_networks(false, true).await;
//            
//            // Clear graph area
//            Rectangle::new(
//                Point::new(0, layout.banner_height),
//                Size::new(LCD_WIDTH as u32, (LCD_HEIGHT - layout.banner_height) as u32),
//            )
//            .into_styled(PrimitiveStyle::with_fill(Rgb888::BLACK))
//            .draw(display)
//            .ok();
//            
//            if networks.is_empty() {
//                // Draw "No networks found"
//                Text::new(
//                    "No networks found",
//                    Point::new(0, layout.banner_height),
//                    MonoTextStyle::new(&FONT, Rgb888::WHITE),
//                )
//                .draw(display)
//                .ok();
//            } else {
//                // First pass: collect statistics
//                for (i, network) in networks.iter().enumerate() {
//                    let channel = network.channel;
//                    let idx = (channel - 1) as usize;
//                    let rssi = network.rssi;
//                    let bssid = &network.bssid;
//                    
//                    // Track peak RSSI per channel
//                    if !network.ssid.is_empty() && rssi > peak_list[idx] {
//                        peak_list[idx] = rssi;
//                        peak_id_list[idx] = i as i16;
//                    }
//                    
//                    // Check for duplicate BSSID (same AP on same channel)
//                    let mut is_duplicate = false;
//                    for j in 0..i {
//                        if networks[j].channel == channel
//                            && match_bssid_prefix(&networks[j].bssid, bssid)
//                        {
//                            is_duplicate = true;
//                            break;
//                        }
//                    }
//                    
//                    if !is_duplicate {
//                        ap_count_list[idx] += 1;
//                        update_noise_list(&mut noise_list, channel, rssi);
//                    }
//                }
//                
//                // Second pass: draw signal bars
//                for (i, network) in networks.iter().enumerate() {
//                    let channel = network.channel;
//                    let idx = (channel - 1) as usize;
//                    let mut rssi = network.rssi.clamp(RSSI_FLOOR, RSSI_CEILING);
//                    let color = CHANNEL_COLORS[idx];
//                    
//                    // Calculate bar height
//                    let height = map_rssi_to_height(rssi, layout.graph_height);
//                    let offset = (idx + 2) * layout.channel_width;
//                    
//                    // Draw signal bar as ellipse
//                    Ellipse::new(
//                        Point::new(offset as i32, (layout.graph_baseline + 1) as i32),
//                        Size::new(layout.signal_width as u32, height as u32),
//                    )
//                    .into_styled(PrimitiveStyle::with_fill(color))
//                    .draw(display)
//                    .ok();
//                    
//                    // Show SSID for strong signals
//                    if rssi >= RSSI_SHOW_SSID && i == peak_id_list[idx] as usize {
//                        let ssid = if network.ssid.is_empty() {
//                            format_bssid(&network.bssid)
//                        } else {
//                            network.ssid.clone()
//                        };
//                        
//                        let text_width = (ssid.len() + 6) * 6;  // "+6" for "(RSSI)"
//                        let mut text_offset = offset - layout.signal_width;
//                        
//                        // Adjust text position to fit on screen
//                        if text_width > LCD_WIDTH {
//                            text_offset = 0;
//                        } else if (text_offset + text_width) > LCD_WIDTH {
//                            text_offset = LCD_WIDTH - text_width;
//                        }
//                        
//                        let text_y = if (height + 8) > layout.graph_height {
//                            layout.graph_baseline - layout.graph_height
//                        } else {
//                            layout.graph_baseline - 10 - height
//                        };
//                        
//                        // Draw SSID, RSSI, and encryption indicator
//                        let mut label = String::<64>::new();
//                        write!(label, "{}({})", ssid, rssi).ok();
//                        if network.encryption == WifiEncryption::Open {
//                            label.push('*').ok();
//                        }
//                        
//                        Text::new(
//                            label.as_str(),
//                            Point::new(text_offset as i32, text_y as i32),
//                            MonoTextStyle::new(&FONT, color),
//                        )
//                        .draw(display)
//                        .ok();
//                    }
//                }
//            }
//            
//            // Find and display least noisy channels
//            let min_noise = *noise_list[1..=11].iter().min().unwrap_or(&noise_list[0]);
//            let mut best_channels = heapless::Vec::<u8, 11>::new();
//            for ch in 1..=11 {
//                let idx = (ch - 1) as usize;
//                if noise_list[idx] == min_noise {
//                    best_channels.push(ch).ok();
//                }
//            }
//            
//            // Draw statistics text
//            let mut stat_text = String::<64>::new();
//            write!(stat_text, "{} networks found, lesser noise channels: ", networks.len()).ok();
//            for (i, ch) in best_channels.iter().enumerate() {
//                if i > 0 {
//                    stat_text.push_str(", ").ok();
//                }
//                write!(stat_text, "{}", ch).ok();
//            }
//            
//            Text::new(
//                stat_text.as_str(),
//                Point::new(0, layout.banner_height),
//                MonoTextStyle::new(&FONT, Rgb888::WHITE),
//            )
//            .draw(display)
//            .ok();
//            
//            // Draw channel numbers and AP counts
//            Line::new(
//                Point::new(0, layout.graph_baseline),
//                Point::new(LCD_WIDTH as i32, layout.graph_baseline),
//            )
//            .into_styled(PrimitiveStyle::with_stroke(Rgb888::WHITE, 1))
//            .draw(display)
//            .ok();
//            
//            for idx in 0..14 {
//                let channel = CHANNEL_LEGEND[idx];
//                let offset = (idx + 2) * layout.channel_width;
//                
//                if channel > 0 {
//                    // Draw channel number
//                    let mut ch_str = String::<3>::new();
//                    write!(ch_str, "{}", channel).ok();
//                    let ch_x = offset - if channel < 10 { 3 } else { 6 };
//                    
//                    Text::new(
//                        ch_str.as_str(),
//                        Point::new(ch_x as i32, (layout.graph_baseline + 2) as i32),
//                        MonoTextStyle::new(&FONT, CHANNEL_COLORS[idx]),
//                    )
//                    .draw(display)
//                    .ok();
//                }
//                
//                // Draw AP count
//                if ap_count_list[idx] > 0 {
//                    let mut count_str = String::<3>::new();
//                    write!(count_str, "{}", ap_count_list[idx]).ok();
//                    let count_x = offset - if ap_count_list[idx] < 10 { 3 } else { 6 };
//                    
//                    Text::new(
//                        count_str.as_str(),
//                        Point::new(count_x as i32, (layout.graph_baseline + 10) as i32),
//                        MonoTextStyle::new(&FONT, Rgb888::new(192, 192, 192)),  // Light grey
//                    )
//                    .draw(display)
//                    .ok();
//                }
//            }
//            
//            // Wait before next scan
//            Timer::after_millis(SCAN_INTERVAL_MS).await;
//        }
//    }

// Layout structure
// #[derive(Debug, Clone, Copy)]
// struct Layout {
//     banner_text_size: u8,
//     banner_height: usize,
//     graph_height: usize,
//     graph_baseline: usize,
//     channel_width: usize,
//     signal_width: usize,
// }

// Network structure
// #[derive(Debug, Clone)]
// struct Network {
//     ssid: String<32>,
//     bssid: [u8; 6],
//     rssi: i32,
//     channel: u8,
//     encryption: WifiEncryption,
// }

// Notes:
// - WiFi channels overlap: channels 1, 6, 11 are non-overlapping in 2.4 GHz
// - Noise calculation uses squared difference to emphasize stronger signals
// - Signal bars are drawn as ellipses (half-ellipse actually, using writeEllipseHelper)
// - Only channels 1-11 are considered for "best channel" (12-14 may not be available)
// - BSSID prefix matching (first 5 bytes) helps identify same AP on different channels
// - The original uses `constrain()` and `map()` Arduino functions for value mapping
// - SSID display is positioned above the signal bar, adjusted to fit on screen
// - Open networks are marked with '*' after the RSSI value
