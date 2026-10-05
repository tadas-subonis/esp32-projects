# PaperColor — M5PM1 Power Management

**Source:** https://docs.m5stack.com/en/arduino/papercolor/m5pm1  
**Retrieved:** 2026-06-22

---

## Multi-level power switch

PaperColor integrates **M5PM1** (I²C `0x6E`, bus SDA=G3 / SCL=G2). Power levels are **not series-connected** — each level is independently switchable from the L0 (SYS_VBUS) source.

After M5PM1 powers on, **L1, L2, and L3A** are automatically enabled by default.

| Level | What's powered |
|-------|----------------|
| **L0** | M5PM1 off; RTC + battery maintain M5PM1 |
| **L1** | M5PM1 standby |
| **L2** | ESP32-S3 sleep: SHT40, button pull-ups (3V3_L2) |
| **L3A** | ESP32-S3 active |
| **L3B** | Peripherals: audio, e-paper, RGB, Grove, microSD |

### L3B GPIO control

| ESP32-S3 | Function |
|----------|----------|
| G45 (AUDIO_PWR_EN) | ES8311 + ES7210 + mic power |
| G46 (SPK_EN) | AW8737A speaker amp |

### M5PM1 PY GPIO (via I²C)

| Pin | Function |
|-----|----------|
| PYG0 (PY_EPD_EN) | E-paper power |
| PYG2 (RTC_IRQ) | RTC interrupt |
| PYG3 (PY_SD_PWR_EN) | microSD power |
| PYG4 (PY_SD_DET_EN) | SD detect pull-up |
| PYG1 (CARD_DEC) | SD insertion detect |
| PY_MPWR_EN | 3V3_L2 switch |
| PY_RGB_PWR_EN | RGB LED power |
| PY_GROVE_OUT_EN | Grove 5 V direction |

**Library:** [M5PM1 Arduino](https://github.com/m5stack/M5PM1)

## Manual sleep

```cpp
pm1.shutdown();
```

Default shutdown falls back to L0 (only M5PM1 + RTC). For ESP32 deep sleep with peripheral retention, configure power-switch pin states before sleep.

## I2C idle sleep

```cpp
m5pm1_err_t setI2cSleepTime(uint8_t seconds);
```

First I²C transaction after wake will fail (wakes M5PM1); retry on next transaction.

## Timer

```cpp
m5pm1_err_t timerSet(uint32_t seconds, m5pm1_tim_action_t action);
```

Actions: `M5PM1_TIM_ACTION_FLAG`, `REBOOT`, `POWERON`, `POWEROFF`.

## Init pattern

```cpp
#include <M5PM1.h>
M5PM1 pm1;

m5pm1_err_t err = pm1.begin(&M5.In_I2C, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K);
if (err == M5PM1_OK) {
    pm1.setLdoEnable(true);
}
```

## Critical for e-ink bring-up

E-paper will not update if **PY_EPD_EN** (PMIC GPIO0) is not asserted. M5Unified/M5PM1 libraries handle this when `M5.begin()` is used correctly; custom firmware must enable the EPD rail explicitly.

See [hardware spec](../m5paper-color.md) and [community PMIC notes](../community/papercolor-esphome-hardware-notes.md).
