# CoreS3 — Arduino Program Compilation & Upload

**Source:** https://docs.m5stack.com/en/arduino/m5cores3/program  
**Retrieved:** 2026-06-22

---

## 1. Preparation

1. **Arduino IDE** — install per M5Stack guide.
2. **Board manager** — install M5Stack boards; select **`M5CoreS3`**.
3. **Libraries** — install `M5Unified` and `M5GFX` (+ dependencies).

## 2. Download mode

Long-press **RESET** ~2 s until internal **green LED** lights, then release.

## 3. Port selection

Connect USB; after entering download mode, select COM port in Arduino IDE.

## 4. First flash

Open example **BarGraph** from M5Unified/M5GFX library examples → Upload.

## 5. Related resources

- [M5Unified](https://github.com/m5stack/M5Unified)
- [M5GFX](https://github.com/m5stack/M5GFX)
- [M5CoreS3](https://github.com/m5stack/M5CoreS3)
- [Camera guide](./cores3-camera.md)
- [Hardware spec](../cores3.md)

### Arduino API topics (official)

Button, Camera, Display, LTR553, Mic, RTC, SD card, Speaker, Touch, IMU, Wakeup, Power
