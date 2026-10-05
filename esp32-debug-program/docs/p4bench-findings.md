# P4Bench findings — SPI prototype

Measured on the Waveshare ESP32-P4-WIFI6-POE-ETH Rev 2.0 + 3.5" ILI9488 SPI panel.

| | |
|---|---|
| Dates | 2026-09-20 (baseline + full suite + SPI clock sweep) |
| ESP-IDF | v5.5.1 |
| Chip | ESP32-P4 rev 1.3, 2 cores @ 360 MHz, 32 MB PSRAM |
| Panel | 480×320, ILI9488, COLMOD RGB666 |
| SPI (everyday) | **60 MHz** (eye-STABLE overclock; datasheet max write 20 MHz) |
| SPI (max eye-STABLE) | **60 MHz** (80 MHz UNSTABLE on this unit) |
| Pins | CS22 RST5 DC4 MOSI36 SCK32 |
| Firmware | `TEST_MODE 4` (P4Bench) |
| Logs | [`logs/p4bench-baseline.txt`](../logs/p4bench-baseline.txt), [`logs/p4bench-full.txt`](../logs/p4bench-full.txt), monitor log `logs/20260920_233112_COM5.log` |
| How to run | [`docs/p4bench.md`](p4bench.md) |

---

## Milestone status

| Milestone | Code | Hardware measured |
|---|---|---|
| M1 Instrumentation / console / metrics / overlay / pattern | yes | yes |
| M2 LCD throughput + chunk sweep | yes | yes |
| M3 Framebuffer + sprites + auto | yes | yes (full + dirty) |
| M4 Dirty vs full crossover | yes (`crossover`) | yes |
| M5 Particles / tiles / compose | yes | yes |
| M6 Headless TD simulation | yes | yes |
| M7 Visual TD + chaos | yes | yes |
| M8 Audio / Wi-Fi / buttons interference | out of scope for v1 | — |

---

## Summary card

```text
ESP32-P4 CURRENT SPI PROTOTYPE

LCD payload @ 10 MHz:        ~1.18 MB/s  (RGB666)
LCD payload @ 60 MHz*:       ~6.9 MB/s   (*overclock; eye-STABLE)
LCD payload @ 80 MHz:        ~9.1 MB/s   (eye-UNSTABLE — do not use)
Full-frame wire bytes:       460800
Full-frame FPS @ 10 MHz:     ~2.7
Full-frame FPS @ 60 MHz*:    ~15.7
Datasheet SPI write max:     20 MHz
Max eye-STABLE overclock:    60 MHz (operator confirmed 2026-09-20)

Recommended game FPS:        15–30 with dirty/partial LCD
                             (full-frame alone only reaches ~16 FPS even @ 60 MHz)

Dirty still beats full:      through ~60%+ nominal dirty
Practical 30 FPS dirty @10M: ≲ ~5–9% of screen changed
Practical 30 FPS dirty @60M: ≲ ~10–15% (measured 20% dirty ≈ 50 FPS)

32×32 sprites @ ≥15 FPS:     ~10  (dirty @ 10 MHz)
Primary bottleneck:          SPI LCD transfer (scales with clock until ~60 MHz)
PSRAM FB memcpy:             ~11–13 MB/s (not the wall)

Recommended renderer:
  Dirty-region RGB565 PSRAM compose
  + partial CASET/RASET SPI windows
  + ~16-row DMA chunks
  Everyday SPI **60 MHz** on this unit (OC; drop to 20 MHz if text/glyph sparkle is unacceptable)
  Never full-framebuffer every frame if you want 30 FPS
```

---

## 1. LCD throughput (M2)

| | Bytes |
|---|---|
| RGB565 framebuffer | 307200 |
| RGB666 SPI wire (measured) | **460800** |

### Chunk sweep

| Chunk rows | Frame ms | Equiv FPS | MB/s |
|---|---|---|---|
| 1 | 394.5 | 2.53 | 1.11 |
| 4 | 374.5 | 2.67 | 1.17 |
| 8 | 372.0 | 2.69 | 1.18 |
| **16** | **371.0** | **2.70** | **1.18** |
| 32 | 372.3 | 2.69 | 1.18 |
| 64 | 373.1 | 2.68 | 1.18 |

Default: **16-row** DMA chunks. SPI clock trade-offs: see §12.

---

## 12. SPI clock sweep (operator + TX)

Command: `spisweep` (interactive `s`/`p`/`u` after animated proof).  
Date: 2026-09-20. Datasheet ILI9488 max write clock: **20 MHz**; higher = overclock.

| Requested | Actual (`spi_device_get_actual_freq`) | Full@16 FPS | MB/s | Dirty 5% FPS | Dirty 20% FPS | Visual |
|---|---|---|---|---|---|---|
| 10 MHz | 10.00 MHz | 2.70 | 1.18 | 49.7 | 10.2 | **SUSPECT** |
| 16 MHz | 16.00 MHz | 4.30 | 1.89 | 72.0 | 16.3 | **STABLE** |
| 20 MHz | 20.00 MHz | 5.36 | 2.35 | 84.2 | 20.4 | **STABLE** |
| 26.67 MHz | 26.666 MHz | 7.11 | 3.13 | 103.3 | 26.6 | **STABLE** (OC) |
| 32 MHz | **26.666 MHz** | 7.11 | 3.13 | 103.3 | 26.6 | **SUSPECT** (same clock as above) |
| 40 MHz | 40.00 MHz | 10.58 | 4.65 | 135.8 | 38.2 | **STABLE** (OC) |
| 48 MHz | 48.00 MHz | 12.64 | 5.55 | 153.4 | 43.8 | **STABLE** (OC) |
| 60 MHz | 60.00 MHz | 15.68 | 6.89 | 180.4 | 50.0 | **STABLE** (OC) |
| 80 MHz | 80.00 MHz | 20.65 | 9.07 | 211.2 | 59.8 | **UNSTABLE** |

**MAX VERIFIED STABLE: 60 MHz.** Everyday firmware default is now **60 MHz** (`LCD_SPI_HZ_DEFAULT`).

### Findings

- Throughput scales roughly with SPI clock until the eye fails (~80 MHz on this unit).
- Requesting **32 MHz** collapses to **26.666 MHz** actual (SPI divider). Duplicate row in the summary is expected.
- **80 MHz** was eye-UNSTABLE; panel recovered cleanly at known-good 60 MHz then 10 MHz (during the sweep restore path).
- Even at 60 MHz, full-frame is only ~16 FPS — dirty rects remain mandatory for playable artillery.
- Operator saw **glyph/text sparkle** at all clocks (worse edges on letters); 60 MHz still judged best overall. Likely panel + long SPI wiring margin, not only software — revisit packing/partial text blits if Stage 2 HUD is unreadable.
- After each reclock the suite does HW panel re-init + solid color proof + animation; soft re-init alone previously left a white screen.

---

## 2. Memory / framebuffer (M2/M3)

PSRAM RGB565 FB (307200 bytes). Timed ops (`membench`):

| Op | ms | MB/s |
|---|---|---|
| memset clear full FB | 23.05 | 12.7 |
| memcpy FB → PSRAM tmp | 26.09 | 11.2 |
| memcpy PSRAM → FB | 25.41 | 11.5 |
| fill_rect full | 23.17 | 12.6 |
| fill_rect 280×160 | 1.56 | 54.7 |

Memory is ~10× faster than SPI payload rate. Double-buffering will not fix the SPI ceiling.

---

## 3. Dirty vs full crossover (M4)

Command: `crossover 2`

| Dirty % | Dirty FPS | Dirty LCD ms | Dirty bytes | Full FPS | Full LCD ms |
|---|---|---|---|---|---|
| 1 | **321.5** | 3.0 | 3415 | 2.7 | 353 |
| 5 | **46.4** | 21.2 | 24260 | 2.7 | 353 |
| 10 | **23.2** | 42.4 | 47838 | 2.7 | 353 |
| 20 | **11.1** | 87.5 | 101035 | 2.7 | 353 |
| 40 | **5.0** | 186.6 | 221943 | 2.6 | 353 |
| 60 | 2.7 | 344 | 434809 | 2.6 | 353 |
| 80 | 2.6 | 344 | 434809 | 2.6 | 353 |
| 100 | 2.6 | 344 | 434809 | 2.5 | 353 |

**Findings**

- Dirty is dramatically faster for small/medium change sets.
- Above ~60% nominal dirty, merged rectangles approach a full-frame transfer (~435 KB vs 461 KB).
- Dirty never meaningfully loses to full on this panel; at high dirty % both sit at ~2.6 FPS.
- For **~30 FPS**, keep transferred area near the **5%** row (measured 46 FPS @ 5%, 23 FPS @ 10%).

---

## 4. Sprites (M3)

### Full-frame mode

LCD alone ≈ 353 ms → **~2.7 FPS** regardless of modest sprite counts. `auto sprites 20` → **0** at ≥20 FPS (correct).

### Dirty mode (`set mode dirty`, 32×32 opaque)

| Count | FPS | sim ms | lcd ms | LCD bytes/frame |
|---|---|---|---|---|
| 10 | **29.6** | 0.01 | 32.8 | (partial) |
| 25 | 9.3 | 0.04 | 104.6 | ~123441 |

`auto sprites 15` → **10 sprites @ ≥15 FPS** (next step 25 fails).

60 FPS sprite counts are not achievable on this SPI panel even with dirty rects unless each sprite dirties almost nothing (impractical for bouncing sprites).

---

## 5. Particles / tiles / compose (M5)

| Test | Mode | Result |
|---|---|---|
| Particles auto @ 10 FPS | full | **0** — 100 particles still ~2.7 FPS (LCD floor); sim 0.02 ms |
| Tiles (2 layers) | full | 2.5 FPS; render 47.9 ms + LCD 353 ms |
| Compose (opaque→alpha cycle) | full | 2.6 FPS; render 25.6 ms + LCD 353 ms |

Particle **simulation** is cheap; visual FPS is SPI-bound unless dirty/partial paths are used. Tile scroll is inherently near-full-frame.

---

## 6. Headless TD simulation (M6)

No LCD. Entity scale: `enemies=N`, `towers=N/5`, `projectiles=N`.

| Target | Best N (enemies) | Notes |
|---|---|---|
| ≥60 updates/s | **800** (76 FPS @ 800; 34.5 FPS @ 1200) | sim 13.1 ms @ 800 |
| ≥30 updates/s | **1200** (34.5 FPS; 12.7 FPS @ 2000) | sim 29.0 ms @ 1200 |

Naive tower×enemy targeting dominates at high N. CPU is fine for a local artillery match; visual TD on SPI is still LCD-limited.

---

## 7. Visual TD + chaos (M7)

| Test | FPS | sim ms | render ms | lcd ms |
|---|---|---|---|---|
| Visual TD auto @ 10 (full) | 2.7 | 0.10 | 20.8 | 353 |
| Chaos (200e/40t/200p/40spr/300part, full) | 2.6 | 0.90 | 30.4 | 353 |

Combined load does not crash; FPS stays on the SPI floor. Raise entity counts only after dirty rendering is in the game path.

---

## 8. Recommended renderer architecture

1. **Compose** in PSRAM RGB565 FB.
2. **Dirty rectangles** + partial SPI windows (`0x2C` / `0x3C`).
3. **~16-row** DMA chunks inside each window.
4. Budget **≲5–10%** of the panel changed per frame for a 20–30 FPS feel.
5. Prefer **opaque / color-key**; measure alpha (`run compose`) before using it everywhere.
6. Keep heavy sim (TD) headroom — P4 CPU is not the SPI-era limiter.
7. Re-run this suite on the future **I80/RGB** panel for a direct before/after.

### Do not

- Expect full-frame 30/60 FPS on this SPI POC.
- Size bandwidth from RGB565 byte counts (wire is RGB666).
- Call a faster SPI clock a win without a visual correctness check.

---

## 9. Stage 2 (tank artillery) impact

| Topic | Guidance |
|---|---|
| HAL | ILI9488 RGB666 + `0x2C`/`0x3C` @ **60 MHz** everyday on this POC (OC); fall back toward 20 MHz if glyph noise is too bad |
| Frame path | Dirty-rect compose is mandatory (full-frame ≤16 FPS even @ 60 MHz) |
| FPS goal | Design around partial updates; ~15–30 FPS if dirty stays small |
| Sim | Ballistics/AI unlikely to hit CPU wall before LCD |

---

## 10. How to reproduce

```powershell
.\make.ps1 flash PORT=COM5
python scripts/p4bench_full_capture.py COM5
# or interactive:
.\make.ps1 monitor PORT=COM5
```

Useful commands: `membench`, `crossover 2`, `chunks`, `spisweep`, `set mode dirty`, `auto sprites 15 2`, `auto simulation 60 2`, `run chaos`.

---

## 11. Still optional / not done

- [x] SPI clock sweep with UNSTABLE marking (2026-09-20; max STABLE 60 MHz)
- [ ] 60 MHz endurance (45 min continuous + reset/cold/power suite) — `endurance` / `scripts/p4bench_endurance.py`
- [ ] Dirty-mode particle auto
- [ ] Spatial-grid vs naive TD targeting comparison
- [ ] Overlapped render/DMA (architecture D)
- [ ] M8 interference (audio / Wi-Fi / buttons)
- [ ] Same suite on I80/RGB production panel
