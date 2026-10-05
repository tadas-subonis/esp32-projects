# Rust Examples for Waveshare ESP32-S3 Touch AMOLED 1.8"

This directory contains detailed Rust conversions of the Arduino examples from the Waveshare ESP32-S3 Touch AMOLED 1.8" demo package. Each example includes comprehensive documentation, complete algorithms, exact constants, and implementation patterns extracted from the original Arduino code.

## Examples Overview

### Basic Display Examples

1. **[01_hello_world.rs](01_hello_world.rs)** - Basic display text example
   - Initializes display and shows "Hello World!" text
   - Continuously displays text at random positions with random colors
   - Includes exact RGB565 color conversion algorithms
   - Random number generation patterns
   - Good starting point for display testing

2. **[02_drawing_board.rs](02_drawing_board.rs)** - Touch drawing board
   - Interactive drawing using FT3168 touch controller
   - Complete TCA9554 expander initialization sequence
   - FT3168 touch controller initialization with retry logic
   - Touch coordinate reading algorithms
   - Brightness fade-in animation with "Loading board" text
   - Draws circles at touch positions

3. **[03_ascii_table.rs](03_ascii_table.rs)** - ASCII character table display
   - Shows all ASCII characters (0-255) in a grid layout
   - Includes row and column headers in hexadecimal
   - Exact grid calculation algorithms
   - Character positioning and rendering patterns

### Advanced Graphics Examples

4. **[04_touch_image_cycler.rs](04_touch_image_cycler.rs)** - Touch to cycle through images
   - Complete brightness fade-in sequence
   - Color test pattern (RED → GREEN → BLUE → WHITE)
   - RGB565 to RGB888 bitmap conversion algorithm
   - Image cycling logic with touch detection
   - FT3168 device ID reading

5. **[05_rtc_clock.rs](05_rtc_clock.rs)** - RTC clock display
   - Complete PCF85063A RTC initialization
   - BCD (Binary Coded Decimal) conversion functions
   - Date/time setting and reading algorithms
   - Text centering calculation
   - Efficient screen update patterns (only when time changes)
   - Exact register addresses and I2C operations

6. **[06_wifi_analyzer.rs](06_wifi_analyzer.rs)** - WiFi network analyzer
   - Complete WiFi scanning implementation
   - RSSI to bar height mapping algorithm
   - Noise calculation across overlapping channels (±4 channels)
   - BSSID duplicate detection (first 5 bytes)
   - Channel color mapping and visualization
   - Least noisy channel calculation
   - Signal bar drawing as ellipses
   - SSID display with encryption indicators

7. **[07_analog_clock.rs](07_analog_clock.rs)** - Analog clock display
   - Complete trigonometric hand position calculations
   - Millisecond-precision smooth animation
   - Cached line drawing algorithm (Bresenham's with caching)
   - Hand overlap detection and prevention
   - Clock face mark drawing (60 marks, 3 sizes)
   - Efficient pixel update patterns
   - Exact mathematical constants from original

### LVGL Examples (Placeholders)

**Note**: LVGL is a C library and is not directly available in Rust. These examples document what the original Arduino examples do and suggest Rust-native alternatives.

8. **[08_lvgl_animation.rs](08_lvgl_animation.rs)** - LVGL animation demo
9. **[09_lvgl_background.rs](09_lvgl_background.rs)** - LVGL background change
10. **[10_lvgl_rtc_clock.rs](10_lvgl_rtc_clock.rs)** - LVGL RTC clock
11. **[11_lvgl_imu_ui.rs](11_lvgl_imu_ui.rs)** - LVGL IMU UI
12. **[12_lvgl_pmu_adc.rs](12_lvgl_pmu_adc.rs)** - LVGL PMU ADC display
13. **[13_lvgl_widgets.rs](13_lvgl_widgets.rs)** - LVGL widgets demo
14. **[14_lvgl_sd_test.rs](14_lvgl_sd_test.rs)** - LVGL SD card test
16. **[16_lvgl_square_project.rs](16_lvgl_square_project.rs)** - LVGL square project

### Audio Example

15. **[15_audio_echo.rs](15_audio_echo.rs)** - Audio codec echo test
   - Complete ES8311 codec initialization
   - I2S interface configuration (16kHz, 16-bit, stereo)
   - MCLK frequency calculation (sample_rate * 256)
   - Microphone gain and volume control
   - Audio buffer management (10KB buffer)
   - Real-time echo/loopback implementation
   - Test sound playback patterns

## Code Detail Level

Each example includes:

- **Complete algorithms** - All calculations, formulas, and logic from the original
- **Exact constants** - All numeric values, delays, sizes, etc.
- **Register addresses** - I2C register maps, device addresses
- **Implementation patterns** - Complete function implementations with error handling
- **Original code references** - Key sections from Arduino code with explanations
- **Conversion notes** - Differences between Arduino and Rust implementations
- **Hardware details** - Pin assignments, bus configurations, timing requirements

## Usage

These examples are **detailed reference implementations** showing:

1. **Exact algorithms** - Mathematical calculations, signal processing
2. **Hardware protocols** - I2C register operations, I2S configuration, touch reading
3. **Graphics patterns** - Drawing algorithms, caching strategies, animation techniques
4. **Integration patterns** - How to combine multiple peripherals and features

To use these examples:

1. Read the example file to understand the complete implementation
2. Extract the relevant algorithms and patterns
3. Adapt to your specific use case
4. Integrate into your `main.rs` with proper:
   - Async/await patterns (Embassy)
   - Error handling
   - Resource management
   - Hardware initialization

## Key Libraries Used

- **embedded-graphics** - Graphics primitives and text rendering
- **embedded-graphics-framebuf** - Framebuffer support
- **sh8601-rs** - SH8601 display driver
- **esp-hal** - ESP32-S3 hardware abstraction
- **embassy** - Async runtime
- **libm** - Mathematical functions (sin, cos, etc.)

## Hardware Requirements

All examples are designed for:
- **Waveshare ESP32-S3 Touch AMOLED 1.8"** development board
- **SH8601** AMOLED display (368×448 pixels, QSPI)
- **FT3168** touch controller (I2C, address 0x38)
- **TCA9554** GPIO expander (I2C, address 0x20)
- **PCF85063A** RTC (I2C, address 0x51) - for clock examples
- **QMI8658C** IMU (I2C, address 0x6A/0x6B) - for IMU examples
- **AXP2101** PMU (I2C, address 0x34) - for power management examples
- **ES8311** audio codec (I2C, address 0x18) - for audio example

## Implementation Notes

- Examples use `embedded-graphics` for drawing, which is Rust-native
- LVGL examples are placeholders since LVGL is C-only
- Audio example requires ES8311 driver implementation (original uses C library)
- All examples assume proper hardware initialization (see main.rs patterns)
- Consider using async/await for non-blocking operations
- Add proper error handling in production code
- Many examples include exact algorithms that can be directly implemented

## Related Documentation

- [Device documentation](../../devices/waveshare-esp32-s3-touch-amoled-1.8.md) - Hardware reference
- [Display and graphics patterns](../../display-graphics.md) - Graphics programming guide
- [Arduino patterns reference](../../devices/waveshare-amoled-1.8-arduino-patterns.md) - Original Arduino patterns
