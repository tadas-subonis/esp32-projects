#pragma once

#include <cstdint>

namespace artillery {

inline constexpr int kWidth = 480;
inline constexpr int kHeight = 320;
inline constexpr int kHudHeight = 22;
inline constexpr int kPlayTop = kHudHeight;
inline constexpr int kPlayHeight = kHeight - kHudHeight;
inline constexpr int kPixelCount = kWidth * kHeight;

inline constexpr int kTankW = 18;
inline constexpr int kTankH = 10;
inline constexpr int kBarrelLen = 14;
inline constexpr int kTankInset = 48;

inline constexpr int kMinAngle = 5;
inline constexpr int kMaxAngle = 175;
inline constexpr int kMinPower = 8;
inline constexpr int kMaxPower = 100;
inline constexpr int kDefaultPower = 55;
inline constexpr int kDefaultAngle = 45;

inline constexpr int kTankHp = 100;
inline constexpr int kDirectHitDamage = 100;
inline constexpr int kSplashRadius = 36;
inline constexpr int kCraterRadius = 16;

inline constexpr float kGravity = 0.28f;
inline constexpr float kPowerScale = 0.105f;
inline constexpr float kWindScale = 0.018f;
inline constexpr float kProjectileRadius = 2.2f;
inline constexpr int kMaxShotSteps = 2500;

// Live match runs at 70% of wall time (30% slower shots/turns). Present still targets 16 ms.
inline constexpr int kSimSpeedNum = 7;
inline constexpr int kSimSpeedDen = 10;
inline constexpr int kShotStepSimMs = 8;

inline uint32_t sim_dt_ms(uint32_t wall_ms)
{
    const uint32_t s = wall_ms * static_cast<uint32_t>(kSimSpeedNum) / static_cast<uint32_t>(kSimSpeedDen);
    return (s == 0 && wall_ms > 0) ? 1 : s;
}

inline constexpr uint16_t rgb565(int r, int g, int b)
{
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

inline constexpr uint16_t kColorSky = rgb565(46, 92, 158);
inline constexpr uint16_t kColorSkyHi = rgb565(110, 168, 220);
inline constexpr uint16_t kColorSkyLo = rgb565(36, 72, 128);
inline constexpr uint16_t kColorGround = rgb565(92, 148, 64);
inline constexpr uint16_t kColorGroundDark = rgb565(54, 92, 40);
inline constexpr uint16_t kColorGrass = rgb565(124, 184, 72);
inline constexpr uint16_t kColorDirt = rgb565(132, 96, 52);
inline constexpr uint16_t kColorDirtDark = rgb565(84, 58, 34);
inline constexpr uint16_t kColorStone = rgb565(156, 148, 128);
inline constexpr uint16_t kColorHud = rgb565(18, 20, 28);
inline constexpr uint16_t kColorHudHi = rgb565(42, 46, 58);
inline constexpr uint16_t kColorText = rgb565(236, 234, 220);
inline constexpr uint16_t kColorMuted = rgb565(160, 168, 176);
inline constexpr uint16_t kColorP0 = rgb565(232, 124, 36);
inline constexpr uint16_t kColorP0Dk = rgb565(148, 64, 16);
inline constexpr uint16_t kColorP1 = rgb565(36, 196, 204);
inline constexpr uint16_t kColorP1Dk = rgb565(16, 108, 124);
inline constexpr uint16_t kColorProj = rgb565(255, 224, 72);
inline constexpr uint16_t kColorBoom = rgb565(240, 64, 40);
inline constexpr uint16_t kColorBoomHi = rgb565(255, 180, 64);
inline constexpr uint16_t kColorTitle = rgb565(255, 214, 96);
inline constexpr uint16_t kColorSun = rgb565(255, 212, 64);
inline constexpr uint16_t kColorSunCore = rgb565(255, 248, 210);
inline constexpr uint16_t kColorCloud = rgb565(240, 244, 252);
inline constexpr uint16_t kColorCloudSh = rgb565(176, 192, 216);
inline constexpr uint16_t kColorWind = rgb565(255, 236, 96);

}  // namespace artillery
