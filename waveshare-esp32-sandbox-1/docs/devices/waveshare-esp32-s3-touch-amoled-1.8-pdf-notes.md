# Waveshare ESP32-S3 Touch AMOLED 1.8" – PDF extraction notes

Source PDF: https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.8/ESP32-S3-Touch-AMOLED-1.8.pdf

This file captures quick references pulled from the vendor schematic PDF for later cross-checking against our bring-up docs.

## MCU and storage
- MCU: ESP32-S3R8 (8 MB PSRAM implied by part number).
- External flash: W25Q128JV (128 Mbit QSPI flash).

## Buttons and power-related signals

### Physical Buttons
- **`GPIO0` (BOOT)** is the **only physical user button** on this board. It's an active-low tactile switch with internal pull-up.
- `PWRON` is labeled on `GPIO21` and feeds the AXP2101 PMU; **this is NOT a user button**. It's the PMU power-enable input.
- The PDF does not show any other dedicated user GPIO buttons; general-purpose GPIOs (e.g., `GPIO38/39/41/42`) remain available for external buttons.

### AXP2101 Power Key (Second Input)
The AXP2101 PMU has a dedicated **power key** input that can be used as a second "button":
- Press events are detected via the PMU's interrupt status registers
- Read `INTSTS2` register (0x49) and check bit 3 (`PKEY_SHORT_IRQ`)
- Clear the interrupt by writing back to the status register
- This is **software-detected**, not a direct GPIO—requires I2C polling

**Example (Rust):**
```rust
const AXP2101_ADDR: u8 = 0x34;
const AXP2101_INTSTS2: u8 = 0x49;
const PKEY_SHORT_IRQ_BIT: u8 = 1 << 3;

// Read interrupt status
let mut status = [0u8; 1];
i2c.write_read(AXP2101_ADDR, &[AXP2101_INTSTS2], &mut status)?;
let power_key_pressed = (status[0] & PKEY_SHORT_IRQ_BIT) != 0;

// Clear the interrupt
if power_key_pressed {
    i2c.write(AXP2101_ADDR, &[AXP2101_INTSTS2, 0xFF])?;
}
```

### Common Mistakes
- **GPIO21 is NOT a button**: Don't try to read it as GPIO input—it's the PMU PWRON line.
- **EXIO4/EXIO5 are NOT buttons**: EXIO4 is backlight control, EXIO5 is PMU IRQ line.

## TCA9554 expander (EXIO lines)
- The expander U7 (TCA9554PWR) provides lines `EXIO0..EXIO7`.
- Signals shown on EXIO lines include panel/SD/touch control: `LCD_RESET`, `DSI_PWR_EN`, `TP_RESET`, `TP_INT`, `SDCS` (via `EXIO7`), and the QSPI LCD CS/IO lines (CS, SIO0..3, SCL) are listed adjacent to the EXIO block in the schematic text.
- Confirm the exact EXIO-to-signal mapping against the schematic revision before changing wiring; addresses commonly use 0x24/0x20 straps (see main device doc for runtime probing guidance).

## Display and touch buses (from schematic text)
- Display QSPI lines are called out as `QSPI_SIO0/1/2/3`, `QSPI_SCL`, and `LCD_CS`, with `LCD_RESET` and `LCD_TE` also referenced near the EXIO block.
- Touch controller lines appear as `TP_SCL`, `TP_SDA`, `TP_INT`, `TP_RESET`.

## Connectivity notes
- USB: `GPIO19` (D-), `GPIO20` (D+).
- SD (SDIO-style) shows `GPIO1/2/3` plus `SDCS` on `EXIO7`.
- Crystal: 40 MHz main XTAL, 32.768 kHz RTC XTAL present.

## How to use this info
- Use this as a schematic-derived sanity check. For implementation details and known-good mappings, follow `docs/devices/waveshare-esp32-s3-touch-amoled-1.8.md`.
- For buttons: prefer `GPIO0` (BOOT) as the guaranteed tactile input; treat `GPIO21` as PMU PWRON, not a user button, unless verified on your board revision.
