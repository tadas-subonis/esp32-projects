# Snake Game Performance Optimizations

## Analysis of Demo Code vs Current Implementation

After reviewing the ESP-IDF demo code (`ESP32-S3-Touch-AMOLED-1.8-Demo/ESP-IDF-v5.3.2/05_LVGL_WITH_RAM/main/example_qspi_with_ram.c`), several key performance optimizations were identified.

## Current Performance Issues

1. **Pixel-by-pixel rendering**: Drawing many small rectangles (even with RLE) is slow
2. **Multiple display operations**: `clear()` + many `draw()` calls + `flush()` creates overhead
3. **No bulk transfer**: Not using DMA-accelerated bulk framebuffer transfer
4. **Full screen updates**: Always updating entire screen even when only small areas change

## Key Optimizations from Demo Code

### 1. Bulk Framebuffer Transfer (CRITICAL)

**Demo approach:**
```c
// LVGL flush callback - transfers entire buffer at once
esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
```

**Current approach:**
- Draws many small rectangles individually
- Each rectangle triggers a separate SPI transaction
- High overhead from command/address setup

**Recommendation:**
- Write directly to framebuffer memory
- Transfer entire framebuffer in one DMA operation
- Use `sh8601_rs` driver's bulk transfer API if available

### 2. Direct Framebuffer Writing

**Current code:**
```rust
// Draws using embedded-graphics primitives (slow)
Rectangle::new(...).draw(&mut display_res.display)
```

**Optimized approach:**
```rust
// Write directly to framebuffer array (fast)
let fb = &mut fb_res.frame_buf.data;
let idx = (y * LCD_H_RES + x) as usize;
fb[idx] = color;
```

### 3. Eliminate Redundant Operations

**Current code:**
```rust
display_res.display.clear(Rgb888::BLACK).ok();  // Unnecessary!
// ... many draw() calls ...
display_res.display.flush().ok();
```

**Optimized approach:**
- Clear framebuffer in memory (already done with `fb_res.frame_buf.clear()`)
- Skip `display.clear()` call
- Single bulk transfer at the end

### 4. Partial Screen Updates

**Demo approach:**
- LVGL only updates changed regions
- Uses `lv_area_t` to specify dirty rectangles
- Transfers only changed portions

**Recommendation:**
- Track dirty regions (bounding box of changed pixels)
- Only transfer changed regions to display
- More complex but significant speedup for small changes

### 5. SPI Frequency

**Current:** 40 MHz (line 921)
**Demo:** Uses default from `SH8601_PANEL_BUS_QSPI_CONFIG` (likely higher)

**Recommendation:**
- Try increasing to 60-80 MHz if display supports it
- Verify with display datasheet

## Implementation Strategy

### Phase 1: Direct Framebuffer Access (Easiest, High Impact)

Replace embedded-graphics drawing with direct framebuffer writes:

```rust
fn render_system(...) {
    if !game_state.needs_redraw {
        return;
    }
    game_state.needs_redraw = false;
    
    // Clear framebuffer in memory
    fb_res.frame_buf.clear(Rgb888::BLACK).unwrap();
    
    const CELL_SIZE: i32 = 8;
    let fb_data = &mut fb_res.frame_buf.data;
    
    // Direct framebuffer writes (much faster than embedded-graphics primitives)
    for food_pos in food_query.iter() {
        let x = (food_pos.x * CELL_SIZE) as usize;
        let y = (food_pos.y * CELL_SIZE) as usize;
        // Fill 8x8 cell
        for dy in 0..CELL_SIZE {
            for dx in 0..CELL_SIZE {
                let idx = ((y + dy) * LCD_H_RES + (x + dx)) as usize;
                if idx < LCD_BUFFER_SIZE {
                    fb_data[idx] = Rgb888::new(255, 0, 0);
                }
            }
        }
    }
    
    // Similar for snake segments and head...
    
    // Single bulk transfer - use existing flush() but skip clear()
    // The display.clear() call is redundant since we already cleared the framebuffer
    display_res.display.flush().ok();
}
```

**Current bottleneck analysis:**
- Lines 847-877: Drawing many rectangles (even with RLE) is slow
- Each `Rectangle::draw()` call triggers SPI transactions
- `display.clear()` is unnecessary overhead

**Quick win:** Remove `display.clear()` call (line 847) since framebuffer is already cleared.

### Phase 2: Bulk Transfer API

**Current approach (lines 849-876):**
- Iterates through framebuffer row by row
- Draws rectangles for each non-black pixel run
- Each rectangle triggers embedded-graphics drawing which uses SPI

**Better approach:**
The `sh8601_rs` driver implements `embedded_graphics::DrawTarget`, which means:
- `flush()` should handle the final transfer
- But individual `draw()` calls may be inefficient

**Investigation needed:**
1. Check if `Sh8601Driver` has a direct framebuffer write method
2. Look for `write_window()` or `set_window()` + bulk transfer
3. Consider accessing underlying SPI bus directly for bulk DMA transfer

**Demo code pattern:**
```c
// ESP-IDF uses esp_lcd_panel_draw_bitmap() which:
// 1. Sets display window (CASET/PASET commands)
// 2. Sends RAMWR command
// 3. Transfers entire buffer via DMA in one operation
```

**Rust equivalent needed:**
- Window set commands (CASET/PASET)
- RAM write command
- Bulk SPI transfer of framebuffer data

### Phase 3: Partial Updates (Advanced)

Track dirty regions:
```rust
#[derive(Resource)]
struct DirtyRegion {
    min_x: usize,
    min_y: usize,
    max_x: usize,
    max_y: usize,
    has_changes: bool,
}
```

Only transfer changed regions.

## Expected Performance Gains

1. **Direct framebuffer writes**: 5-10x faster than embedded-graphics primitives
2. **Bulk transfer**: 10-20x faster than many small transfers
3. **Skip redundant clear()**: ~5-10ms saved per frame
4. **Partial updates**: 2-5x faster for small changes (snake movement)

**Combined:** Should achieve 30-60 FPS instead of current ~7 FPS.

## Prioritized Action Plan

### Immediate (Low Effort, Medium Impact)

1. **Remove redundant `display.clear()` call** (line 847)
   - Framebuffer is already cleared on line 734
   - Saves ~5-10ms per frame
   - **Effort:** 1 line change
   - **Impact:** ~10% improvement

2. **Increase SPI frequency** (line 921)
   - Try 60-80 MHz (verify display supports it)
   - **Effort:** 1 line change
   - **Impact:** 20-50% improvement if display supports it

### Short Term (Medium Effort, High Impact)

3. **Direct framebuffer writes for game objects**
   - Replace embedded-graphics `Rectangle::draw()` with direct array writes
   - Write 8x8 cells directly to framebuffer array
   - **Effort:** ~50 lines of code
   - **Impact:** 5-10x faster rendering

4. **Bulk framebuffer transfer**
   - Investigate `sh8601_rs` for bulk transfer API
   - Or implement window set + bulk SPI transfer
   - **Effort:** ~100-200 lines (depends on API availability)
   - **Impact:** 10-20x faster than current approach

### Long Term (High Effort, Very High Impact)

5. **Partial screen updates**
   - Track dirty regions (bounding box of changes)
   - Only transfer changed regions
   - **Effort:** ~200-300 lines + state management
   - **Impact:** 2-5x faster for small changes (most snake movements)

6. **Double buffering**
   - Render to back buffer while front buffer transfers
   - Overlaps rendering and display transfer
   - **Effort:** Significant refactoring
   - **Impact:** Eliminates display transfer blocking

## Quick Wins Summary

**Do these first (5 minutes):**
1. Remove `display_res.display.clear(Rgb888::BLACK).ok();` on line 847
2. Try increasing SPI frequency to 60 MHz (line 921)

**Then (30-60 minutes):**
3. Replace embedded-graphics drawing with direct framebuffer writes
4. Investigate bulk transfer API in `sh8601_rs`

**Expected result:** 3-5x performance improvement with minimal effort.

## References

- Demo code: `/home/tadas/tmp/ESP32-S3-Touch-AMOLED-1.8-Demo/ESP-IDF-v5.3.2/05_LVGL_WITH_RAM/main/example_qspi_with_ram.c`
- ESP-IDF LCD panel API: `esp_lcd_panel_draw_bitmap()` for bulk transfers
- Current implementation: `src/bin/main.rs` lines 718-878
- SH8601 datasheet: Check for maximum SPI frequency and bulk transfer commands
