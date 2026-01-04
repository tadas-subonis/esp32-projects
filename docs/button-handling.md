# Button Handling on ESP32-S3

Complete guide to implementing reliable button handling on ESP32-S3 with embedded Rust, covering GPIO configuration, debouncing, sleep/wake patterns, and display management.

## Table of Contents

- [Quick Start](#quick-start)
- [Button Configuration](#button-configuration)
- [Debouncing](#debouncing)
- [Long Press Detection](#long-press-detection)
- [Sleep and Wake Patterns](#sleep-and-wake-patterns)
- [Display Management with Sleep](#display-management-with-sleep)
- [Button Types: Direct GPIO vs I2C Expander](#button-types-direct-gpio-vs-i2c-expander)
- [Common Mistakes](#common-mistakes)
- [Complete Working Example](#complete-working-example)

## Quick Start

For a simple button with long-press sleep functionality:

```rust
use esp_hal::gpio::{Input, InputConfig, Pull};
use embassy_time::{Duration, Instant, Timer};

// 1. Configure button (active-low with pull-up)
let btn_cfg = InputConfig::default().with_pull(Pull::Up);
let btn = Input::new(peripherals.GPIO0, btn_cfg);

// 2. Debounce: require 3 consecutive polls (60ms at 20ms polling)
// 3. Long press: measure duration after debounce
// 4. Sleep entry: wait for button release, flush display, then sleep
// 5. Sleep wake: debounce wake detection
```

**Critical requirements:**
- Always wait for button release before entering sleep
- Always flush display updates before sleep
- Always debounce button state changes
- Verify button wiring with hardware, don't assume pin functions

### Simple Button Pattern (Blocking)

For basic button handling without async, see the [Getting Started Guide](./getting-started.md):

```rust
let button = Input::new(peripherals.GPIO0, InputConfig::default());
let delay = Delay::new();

loop {
    if button.is_low() {
        println!("Button Pressed");
        delay.delay_millis(50);  // Simple debounce
    }
}
```

**Note:** This simple pattern is fine for basic use cases. For production code with long-press, sleep/wake, and proper debouncing, use the patterns in this guide.

## Button Configuration

### Active-Low Buttons (Most Common)

Most buttons are **active-low**: pressed = LOW (0), released = HIGH (1).

```rust
use esp_hal::gpio::{Input, InputConfig, Pull};

// Correct configuration for active-low button
let btn_cfg = InputConfig::default().with_pull(Pull::Up);
let btn = Input::new(peripherals.GPIO0, btn_cfg);

// Check pressed state
if btn.is_low() {
    // Button is pressed
}
```

**Why pull-up?** The pull-up resistor ensures the GPIO reads HIGH when the button is released. Without it, the pin floats and can read unpredictable values.

### Active-High Buttons (Less Common)

Some buttons are **active-high**: pressed = HIGH (1), released = LOW (0).

```rust
// Configuration for active-high button
let btn_cfg = InputConfig::default().with_pull(Pull::Down);
let btn = Input::new(peripherals.GPIO0, btn_cfg);

// Check pressed state
if btn.is_high() {
    // Button is pressed
}
```

### Determining Button Polarity

**Methods:**
1. **Hardware schematic** - Check the board documentation
2. **Multimeter test** - Measure voltage when button pressed vs released
3. **Logic analyzer** - Capture GPIO state transitions
4. **Software logging** - Log GPIO state and observe behavior

**Default assumption:** If unsure, start with active-low (pull-up). Most buttons follow this pattern.

### Verifying Button Wiring

**Critical:** Always verify which GPIO pin is actually connected to your button.

**Common mistake:** Assuming a GPIO pin labeled "PWRON" or "BUTTON" in documentation is actually wired to a physical button. Documentation may be incorrect or refer to a different board revision.

**Verification steps:**
1. Check board schematic (if available)
2. Test with logging: press button and observe GPIO state changes
3. Use a multimeter to verify continuity
4. Check board revision matches documentation

**Example verification code:**
```rust
loop {
    let state = btn.is_low();
    if state != last_state {
        esp_println::println!("Button state changed: {} -> {}", 
            if last_state { "LOW" } else { "HIGH" },
            if state { "LOW" } else { "HIGH" });
        last_state = state;
    }
    Timer::after(Duration::from_millis(100)).await;
}
```

## Debouncing

Hardware buttons have mechanical bounce - the contacts make/break multiple times before settling. Software debouncing filters out these false state changes.

### Consecutive Polls Method (Recommended)

Require the button to maintain its state across multiple consecutive polls before accepting it as valid.

```rust
const POLL_INTERVAL: Duration = Duration::from_millis(20);
const DEBOUNCE_THRESHOLD: u32 = 3; // 3 polls × 20ms = 60ms debounce

let mut consecutive_low = 0u32;

loop {
    let button_low = btn.is_low();
    
    if button_low {
        consecutive_low += 1;
        
        // Only act after button has been low for threshold polls
        if consecutive_low >= DEBOUNCE_THRESHOLD {
            // Button is reliably pressed (debounced)
            handle_button_press();
        }
    } else {
        consecutive_low = 0; // Reset on any HIGH reading
    }
    
    Timer::after(POLL_INTERVAL).await;
}
```

**Why this works:**
- Catches transient noise (single poll glitches are ignored)
- Simple to implement and understand
- Effective for most use cases
- Low CPU overhead

**Debounce timing:** 3 polls × 20ms = 60ms is typically sufficient. Adjust based on button quality and requirements.

### Time-Based Debouncing

Alternative approach using time windows:

```rust
let mut last_state_change = Instant::now();
let mut last_state = btn.is_low();
const DEBOUNCE_TIME: Duration = Duration::from_millis(50);

loop {
    let current_state = btn.is_low();
    
    if current_state != last_state {
        last_state_change = Instant::now();
        last_state = current_state;
    }
    
    // Only accept state if it's been stable for debounce time
    if Instant::now().duration_since(last_state_change) >= DEBOUNCE_TIME {
        if current_state {
            handle_button_press();
        }
    }
    
    Timer::after(POLL_INTERVAL).await;
}
```

**When to use:** When you need precise timing control or more complex debounce logic.

## Long Press Detection

Detect when a button is held for a specific duration (e.g., 2 seconds for sleep mode).

### Pattern: Start Timer After Debounce

```rust
const LONG_PRESS_DURATION: Duration = Duration::from_millis(2000);
let mut button_held_since: Option<Instant> = None;
let mut consecutive_low = 0u32;

loop {
    let button_low = btn.is_low();
    
    if button_low {
        consecutive_low += 1;
        
        // Start timer only after debounce confirms button is pressed
        if consecutive_low >= DEBOUNCE_THRESHOLD {
            let since = button_held_since.get_or_insert_with(|| {
                Instant::now() // Start timer on first confirmed press
            });
            
            let held_duration = Instant::now().duration_since(*since);
            
            if held_duration >= LONG_PRESS_DURATION {
                handle_long_press();
                // Reset timer to prevent repeated triggers
                button_held_since = None;
            }
        }
    } else {
        // Reset on release
        consecutive_low = 0;
        button_held_since = None;
    }
    
    Timer::after(POLL_INTERVAL).await;
}
```

**Key points:**
- Start timer **after** debounce confirms press (not on first poll)
- Use `Option<Instant>` to track timer state
- Reset timer when button is released
- Reset timer after long press to prevent repeated triggers

## Sleep and Wake Patterns

### Sleep Entry: Wait for Button Release

**Critical requirement:** Always wait for the button to be released before entering sleep mode.

**Why?** If the button is still held when sleep starts, the sleep loop will immediately detect it as pressed and wake up, creating an infinite sleep/wake loop.

```rust
if held_duration >= LONG_PRESS_DURATION {
    // Wait for button release
    while btn.is_low() {
        Timer::after(POLL_INTERVAL).await;
    }
    
    // Small stability delay after release
    Timer::after(Duration::from_millis(100)).await;
    
    // Now safe to enter sleep
    enter_sleep_mode().await;
}
```

**Why the stability delay?** Ensures button state has fully settled after release before entering sleep.

### Sleep Mode Implementation

```rust
async fn enter_light_sleep<D>(
    display: &mut D,
    btn: &Input<'_>,
) where
    D: embedded_graphics::prelude::DrawTarget<Color = Rgb888>,
{
    esp_println::println!("SLEEP: entering light sleep mode");
    
    // Poll less frequently in sleep to save power
    const SLEEP_POLL: Duration = Duration::from_millis(100);
    
    loop {
        if btn.is_low() {
            // Debounce wake detection
            Timer::after(Duration::from_millis(50)).await;
            
            // Confirm wake (button still pressed after debounce)
            if btn.is_low() {
                break; // Wake confirmed
            }
        }
        
        Timer::after(SLEEP_POLL).await;
    }
    
    esp_println::println!("SLEEP: waking up");
}
```

**Key points:**
- Poll less frequently in sleep (100ms vs 20ms) to save power
- Debounce wake detection to avoid false wakes from noise
- Confirm wake with second check after debounce delay

## Display Management with Sleep

### Critical: Flush Display Before Sleep

**Common mistake:** Clearing the display framebuffer but forgetting to flush, so the screen never actually updates.

```rust
// ❌ WRONG: Display won't update
display.clear(Rgb888::BLACK).ok();
enter_sleep_mode().await;

// ✅ CORRECT: Flush before sleep
display.clear(Rgb888::BLACK).unwrap();
display.flush().unwrap(); // Critical: updates physical display
enter_sleep_mode().await;
```

**Why flush is needed:** `display.clear()` only updates the framebuffer in memory. `display.flush()` sends the framebuffer to the display hardware. Without flush, the screen shows the previous frame.

### Complete Sleep Entry Sequence

```rust
if held_duration >= LONG_PRESS_DURATION {
    // 1. Wait for button release
    while btn.is_low() {
        Timer::after(POLL_INTERVAL).await;
    }
    
    // 2. Stability delay
    Timer::after(Duration::from_millis(100)).await;
    
    // 3. Clear and flush display (in this order!)
    display.clear(Rgb888::BLACK).unwrap();
    display.flush().unwrap(); // Must flush to update screen
    
    // 4. Enter sleep
    enter_light_sleep(&mut display, &btn).await;
    
    // 5. Restore display after wake
    display.clear(Rgb888::BLACK).unwrap();
    // ... draw content ...
    display.flush().unwrap();
}
```

## Button Types: Direct GPIO vs I2C Expander

### Direct GPIO Buttons (Recommended)

**Use when:** You have available GPIO pins and need simple, fast button handling.

**Configuration:**
```rust
let btn_cfg = InputConfig::default().with_pull(Pull::Up);
let btn = Input::new(peripherals.GPIO0, btn_cfg);
```

**Pros:**
- Simple, fast, reliable
- No I2C overhead
- Direct hardware access
- Low latency

**Cons:**
- Limited by available GPIO pins
- May conflict with other peripherals

### I2C Expander Buttons (TCA9554, etc.)

**Use when:** You need many buttons or GPIO pins are limited.

**Configuration:**
```rust
// 1. Configure expander pin as input
const TCA9554_CONFIG: u8 = 0x03; // Configuration register
const TCA9554_INPUT: u8 = 0x00;  // Input register
const EXIO4_MASK: u8 = 1 << 4;   // EXIO4 bit

// Set EXIO4 as input (1 = input, 0 = output)
let mut config = [0u8; 1];
i2c.write_read(expander_addr, &[TCA9554_CONFIG], &mut config)?;
config[0] |= EXIO4_MASK; // Set bit 4 to 1 (input)
i2c.write(expander_addr, &[TCA9554_CONFIG, config[0]])?;

// 2. Read input register
let mut input = [0u8; 1];
i2c.write_read(expander_addr, &[TCA9554_INPUT], &mut input)?;

// 3. Check button state (active-low: bit 4 = 0 means pressed)
let button_pressed = (input[0] & EXIO4_MASK) == 0;
```

**Important considerations:**
- **Pin direction:** Must configure pin as input (not output)
- **Polarity:** Verify if button is active-low or active-high
- **Baseline calibration:** Some boards idle in unexpected states
- **I2C latency:** Slower than direct GPIO (adds ~1-5ms per read)

**Common pitfalls:**
- Forgetting to configure pin direction (defaults to output)
- Assuming polarity without verification
- Not handling I2C errors gracefully
- Baseline calibration issues (button always reads as pressed)

**When to avoid:** If you only need 1-2 buttons, prefer direct GPIO for simplicity.

## Common Mistakes

### 1. Forgetting Display Flush

```rust
// ❌ WRONG
display.clear(Rgb888::BLACK).ok();
enter_sleep_mode().await; // Screen still shows old content!

// ✅ CORRECT
display.clear(Rgb888::BLACK).unwrap();
display.flush().unwrap(); // Updates physical display
enter_sleep_mode().await;
```

### 2. Entering Sleep with Button Held

```rust
// ❌ WRONG: Immediate wake-up loop
if held_duration >= LONG_PRESS_DURATION {
    enter_sleep_mode().await; // Button still LOW!
}

// ✅ CORRECT: Wait for release
if held_duration >= LONG_PRESS_DURATION {
    while btn.is_low() {
        Timer::after(POLL_INTERVAL).await;
    }
    enter_sleep_mode().await;
}
```

### 3. Wrong Pull Configuration

```rust
// ❌ WRONG: Active-low button with pull-down
let btn = Input::new(peripherals.GPIO0,
    InputConfig::default().with_pull(Pull::Down));

// ✅ CORRECT: Active-low button with pull-up
let btn = Input::new(peripherals.GPIO0,
    InputConfig::default().with_pull(Pull::Up));
```

### 4. No Debouncing

```rust
// ❌ WRONG: Noise causes false triggers
if btn.is_low() {
    do_action(); // Triggers on every bounce!
}

// ✅ CORRECT: Debounce first
if consecutive_low >= 3 && btn.is_low() {
    do_action(); // Only after stable press
}
```

### 5. Assuming GPIO Pin Function

```rust
// ❌ WRONG: Assuming GPIO21 is a button
let btn = Input::new(peripherals.GPIO21, btn_cfg);
// GPIO21 might not be connected to anything!

// ✅ CORRECT: Verify with hardware/logs
// Test: does GPIO21 actually change when button pressed?
// Check: board schematic, revision, test with logging
```

### 6. Starting Timer Before Debounce

```rust
// ❌ WRONG: Timer starts on first poll (noise triggers it)
if btn.is_low() {
    let since = timer.get_or_insert_with(|| Instant::now());
    // Timer started on first bounce, not real press
}

// ✅ CORRECT: Start timer after debounce
if consecutive_low >= DEBOUNCE_THRESHOLD {
    let since = timer.get_or_insert_with(|| Instant::now());
    // Timer only starts after button confirmed pressed
}
```

### 7. Not Resetting Timer After Long Press

```rust
// ❌ WRONG: Timer keeps running, triggers repeatedly
if held_duration >= LONG_PRESS_DURATION {
    handle_long_press();
    // Timer still active, will trigger again immediately
}

// ✅ CORRECT: Reset timer after handling
if held_duration >= LONG_PRESS_DURATION {
    handle_long_press();
    button_held_since = None; // Reset to prevent repeat
}
```

## Complete Working Example

Complete implementation combining all patterns:

```rust
use embassy_executor::Spawner;
use embassy_time::{Duration, Instant, Timer};
use embedded_graphics::{
    mono_font::{MonoTextStyle, ascii::FONT_10X20},
    pixelcolor::Rgb888,
    prelude::*,
    text::Text,
};
use esp_hal::{
    gpio::{Input, InputConfig, Pull},
    // ... other imports
};

#[esp_rtos::main]
async fn main(spawner: Spawner) -> ! {
    // ... initialization code ...
    
    // Button configuration
    let btn_cfg = InputConfig::default().with_pull(Pull::Up);
    let btn = Input::new(peripherals.GPIO0, btn_cfg);
    
    // Timing constants
    const LONG_PRESS_DURATION: Duration = Duration::from_millis(2000);
    const POLL_INTERVAL: Duration = Duration::from_millis(20);
    const DEBOUNCE_THRESHOLD: u32 = 3;
    const LOG_INTERVAL: Duration = Duration::from_millis(1000);
    const PROGRESS_LOG_INTERVAL: Duration = Duration::from_millis(200);
    
    // State tracking
    let mut button_held_since: Option<Instant> = None;
    let mut consecutive_low = 0u32;
    let mut last_log = Instant::now();
    let mut last_progress_log = Instant::now();
    let mut last_state = btn.is_low();
    
    loop {
        let button_low = btn.is_low();
        
        // Debouncing: require consecutive polls
        if button_low {
            consecutive_low += 1;
            
            // Start timer only after debounce confirms press
            if consecutive_low >= DEBOUNCE_THRESHOLD {
                let since = button_held_since.get_or_insert_with(|| {
                    esp_println::println!("BTN: button pressed, starting hold timer");
                    Instant::now()
                });
                
                let held_duration = Instant::now().duration_since(*since);
                
                // Log progress
                if Instant::now().duration_since(last_progress_log) >= PROGRESS_LOG_INTERVAL {
                    esp_println::println!("BTN: holding... {:?} / {:?}", 
                        held_duration, LONG_PRESS_DURATION);
                    last_progress_log = Instant::now();
                }
                
                // Long press detected
                if held_duration >= LONG_PRESS_DURATION {
                    esp_println::println!("PWR: button held for {:?} -> waiting for release", 
                        held_duration);
                    
                    // CRITICAL: Wait for release before sleep
                    while btn.is_low() {
                        Timer::after(POLL_INTERVAL).await;
                    }
                    esp_println::println!("PWR: button released, entering sleep mode");
                    
                    // Stability delay
                    Timer::after(Duration::from_millis(100)).await;
                    
                    // CRITICAL: Clear and flush display
                    display.clear(Rgb888::BLACK).unwrap();
                    display.flush().unwrap();
                    
                    // Enter sleep
                    enter_light_sleep(&mut display, &btn).await;
                    
                    // Restore display after wake
                    display.clear(Rgb888::BLACK).unwrap();
                    let style = MonoTextStyle::new(&FONT_10X20, Rgb888::WHITE);
                    Text::new("Hello World", Point::new(20, 40), style)
                        .draw(&mut display)
                        .unwrap();
                    display.flush().unwrap();
                    esp_println::println!("SLEEP: display restored, resuming normal operation");
                    
                    // Reset state
                    consecutive_low = 0;
                    button_held_since = None;
                    last_state = btn.is_low();
                }
            }
        } else {
            // Reset on release
            consecutive_low = 0;
            button_held_since = None;
        }
        
        // Periodic logging
        if Instant::now().duration_since(last_log) >= LOG_INTERVAL {
            if button_low || button_held_since.is_some() {
                esp_println::println!("BTN: state={} held_for={:?}",
                    if button_low { "LOW" } else { "HIGH" },
                    button_held_since.map(|s| Instant::now().duration_since(s))
                );
            }
            last_log = Instant::now();
        }
        
        Timer::after(POLL_INTERVAL).await;
    }
}

async fn enter_light_sleep<D>(
    display: &mut D,
    btn: &Input<'_>,
) where
    D: embedded_graphics::prelude::DrawTarget<Color = Rgb888>,
{
    esp_println::println!("SLEEP: entering light sleep mode");
    
    const SLEEP_POLL: Duration = Duration::from_millis(100);
    
    loop {
        if btn.is_low() {
            esp_println::println!("SLEEP: wake detected");
            // Debounce wake detection
            Timer::after(Duration::from_millis(50)).await;
            if btn.is_low() {
                break; // Confirmed wake
            }
        }
        Timer::after(SLEEP_POLL).await;
    }
    
    esp_println::println!("SLEEP: waking up");
}
```

## Summary Checklist

When implementing button handling, ensure:

- [ ] **Button wiring verified** - Test GPIO pin actually changes when button pressed
- [ ] **Correct pull configuration** - Pull::Up for active-low, Pull::Down for active-high
- [ ] **Debouncing implemented** - Consecutive polls method recommended
- [ ] **Timer starts after debounce** - Not on first poll
- [ ] **Button release waited** - Before entering sleep mode
- [ ] **Display flushed** - After clear, before sleep
- [ ] **Wake detection debounced** - In sleep mode
- [ ] **State reset properly** - After long press, after release
- [ ] **Error handling** - For I2C expander reads, display operations
- [ ] **Logging for debugging** - Button state changes, timing

## References

- [ESP-HAL GPIO Documentation](https://docs.rs/esp-hal/)
- [Embedded Graphics Display API](https://docs.rs/embedded-graphics/)
- Board-specific documentation: `docs/devices/waveshare-esp32-s3-touch-amoled-1.8.md`
- [Embassy Time Documentation](./crates/embassy-time.md)
