# 60 fps on SPI ILI9488

The P4 is not the bottleneck. A full 480×320 RGB666 frame is 460,800 bytes. At 10 MHz SPI that is ~370 ms of wire time (~2.7 fps). Compose of a full RGB565 framebuffer in PSRAM was ~27 ms at 20 MHz HEX PSRAM. The present path therefore **must not** upload or redraw the whole screen every tick.

Idle on the POC board is overlay-only: `fps≈62`, `compose_ms=0`, `flush_ms≈1–3`, `dirty_px=4560`. Measure with `.\make cmd CMD=status` (close `flash-monitor` first).

Present still targets **16 ms wall time**. Live `Match::tick` runs at **70%** of that (`kSimSpeedNum/kSimSpeedDen`) so shots and turns feel slower without dropping FPS. `artillery-sim` / `simulate_shot` are unscaled (golden physics).

## What we do not do

- Full-screen blit every frame (cap is ~11 fps even at 40 MHz).
- SDL / PPA / a logical 320×240 scale on this SPI panel ([game-stack.md](game-stack.md)).
- A second physics implementation for “slow motion”.

## Techniques

### 1. Dual buffers (`present_frame`)

| Buffer | Contents |
|--------|----------|
| `scene` | Last full compose **without** projectile or debug overlay |
| `frame` | What SPI uploads (`scene` + projectile + overlay) |

Idle frames restore the overlay rect from `scene`, draw the overlay, and flush that rect. The shell restores its previous rect from `scene`, draws the new blob, flushes both.

### 2. Three dirty classes

| When | Compose | SPI |
|------|---------|-----|
| Map / title / crater / turn (`scene_dirty_`) | Full `compose_scene` + memcpy to `frame` | Full 480×320 (one hitch, ~90 ms at 40 MHz) |
| Angle / power (`hud_dirty_`) | Full compose into `scene` | HUD strip + both tank/barrel rects + overlay |
| Shell in the air | No world compose | Previous + current projectile rects + overlay |
| Nothing moved | Overlay only | `kDebugOverlayRect` (~152×30) |

Angle/power still compose the whole scene in PSRAM (so barrels erase correctly) but **do not** re-send terrain on SPI.

### 3. SPI clock and strip writes

ILI9488 RGB666, `0x2C` then `0x3C` for a window. Clock is **40 MHz** (10 MHz was only for first bring-up). Color is sent in **16-line DMA strips**, not 320 single-row transactions.

Window X/width is snapped to a multiple of 4 so `w * 3` is 4-byte aligned for SPI DMA.

### 4. Ping-pong DMA buffers

`esp_lcd_panel_io_tx_color` **queues** DMA and returns while the strip buffer is still in flight. Filling that same buffer for the next strip tears the image. Dirty rects then **freeze** the garbage because terrain is never uploaded again.

Fix: two strip buffers, `trans_queue_depth = 1` (next `0x2C` waits for the previous color trans), ping-pong, `NOP` at the end of a rect to drain.

### 5. Faster PSRAM

`sdkconfig.defaults` had `CONFIG_SPIRAM_SPEED_200M`, which this HEX PSRAM Kconfig ignores. The board was running **20 MHz**. Set `CONFIG_SPIRAM_SPEED_80M`. Full compose dropped from ~27 ms to ~11–20 ms, so HUD dirty frames stay under 33 ms.

### 6. Present vs simulate

The firmware/emu loop still wakes every 16 ms and always calls `present()`. `Match::tick` converts wall dt with `sim_dt_ms` (7/10). Projectile steps use an 8 ms-of-sim-time accumulator (`kShotStepSimMs`), so flight is ~30% longer on the clock while the overlay still updates at 60 Hz.

## Budget (idle vs aim vs new map)

| Path | compose | flush | fps |
|------|---------|-------|-----|
| Idle / overlay | ~0 ms | ~1–3 ms | ~60 |
| Nudge angle | ~11 ms | ~12 ms | ~40 |
| `new` / crater (full) | ~20 ms | ~90 ms | one hitch |

If idle `dirty_px` is 153600, the present path fell back to a full blit. If the LCD sparkles after a full blit, check DMA ping-pong before lowering SPI.
