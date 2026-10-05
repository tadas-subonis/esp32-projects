#include "artillery/world.hpp"

#include "artillery/rng.hpp"

#include <cmath>

namespace artillery {

uint16_t World::height_at(int x) const
{
    if (x < 0 || x >= kWidth) {
        return static_cast<uint16_t>(kHeight - 1);
    }
    return height_[static_cast<size_t>(x)];
}

void World::generate(uint32_t seed)
{
    seed_ = seed == 0 ? 1 : seed;
    Rng rng(mix_seed(seed_, 0x51u));

    std::array<float, kWidth> raw{};
    for (int x = 0; x < kWidth; ++x) {
        const float t = static_cast<float>(x);
        float h = 168.0f;
        h += 42.0f * std::sin(t * 0.018f + (seed_ % 97) * 0.11f);
        h += 24.0f * std::sin(t * 0.041f + (seed_ % 53) * 0.17f);
        h += 12.0f * std::sin(t * 0.09f + (seed_ % 29) * 0.23f);
        h += (rng.next_float() - 0.5f) * 18.0f;
        raw[static_cast<size_t>(x)] = h;
    }

    for (int pass = 0; pass < 6; ++pass) {
        std::array<float, kWidth> tmp = raw;
        for (int x = 1; x < kWidth - 1; ++x) {
            raw[static_cast<size_t>(x)] =
                tmp[static_cast<size_t>(x - 1)] * 0.25f + tmp[static_cast<size_t>(x)] * 0.5f +
                tmp[static_cast<size_t>(x + 1)] * 0.25f;
        }
    }

    for (int x = 0; x < kWidth; ++x) {
        int h = static_cast<int>(raw[static_cast<size_t>(x)] + 0.5f);
        h = clamp_int(h, kPlayTop + 70, kHeight - 18);
        height_[static_cast<size_t>(x)] = static_cast<uint16_t>(h);
    }

    tanks_[0] = Tank{};
    tanks_[1] = Tank{};
    tanks_[0].x = static_cast<float>(kTankInset);
    tanks_[1].x = static_cast<float>(kWidth - 1 - kTankInset);
    tanks_[0].hp = kTankHp;
    tanks_[1].hp = kTankHp;
    tanks_[0].alive = true;
    tanks_[1].alive = true;
    settle_tank(tanks_[0]);
    settle_tank(tanks_[1]);
    flatten_platform(static_cast<int>(tanks_[0].x));
    flatten_platform(static_cast<int>(tanks_[1].x));
    settle_tank(tanks_[0]);
    settle_tank(tanks_[1]);
}

void World::flatten_platform(int cx)
{
    const int facing = cx < kWidth / 2 ? 1 : -1;
    const int back = kTankW / 2 + 6;
    const int ahead = 52;
    const uint16_t h = height_at(cx);
    const int x0 = clamp_int(cx - (facing > 0 ? back : ahead), 0, kWidth - 1);
    const int x1 = clamp_int(cx + (facing > 0 ? ahead : back), 0, kWidth - 1);
    for (int x = x0; x <= x1; ++x) {
        height_[static_cast<size_t>(x)] = h;
    }
}

void World::settle_tank(Tank& tank) const
{
    const int x = clamp_int(static_cast<int>(tank.x + 0.5f), 0, kWidth - 1);
    tank.x = static_cast<float>(x);
    tank.y = static_cast<float>(height_at(x) - 1);
}

bool World::solid_at(float x, float y) const
{
    if (x < 0.0f || x >= static_cast<float>(kWidth) || y >= static_cast<float>(kHeight)) {
        return true;
    }
    if (y < static_cast<float>(kPlayTop)) {
        return false;
    }
    return y >= static_cast<float>(height_at(static_cast<int>(x)));
}

void World::apply_crater(int cx, int cy, int radius)
{
    const int r2 = radius * radius;
    const int x0 = clamp_int(cx - radius, 0, kWidth - 1);
    const int x1 = clamp_int(cx + radius, 0, kWidth - 1);
    for (int x = x0; x <= x1; ++x) {
        const int dx = x - cx;
        const int remain = r2 - dx * dx;
        if (remain < 0) {
            continue;
        }
        const int dy = static_cast<int>(std::sqrt(static_cast<float>(remain)) + 0.5f);
        const int crater_bottom = cy + dy;
        if (height_[static_cast<size_t>(x)] < crater_bottom) {
            int nh = crater_bottom;
            nh = clamp_int(nh, kPlayTop + 40, kHeight - 8);
            height_[static_cast<size_t>(x)] = static_cast<uint16_t>(nh);
        }
    }
    settle_tank(tanks_[0]);
    settle_tank(tanks_[1]);
}

void World::replace_heights(const std::array<uint16_t, kWidth>& heights)
{
    height_ = heights;
}

}  // namespace artillery
