#pragma once

#include <cstdint>

namespace artillery {

class Rng {
public:
    explicit Rng(uint32_t seed) : state_(seed == 0 ? 0xA341316Cu : seed) {}

    uint32_t next_u32()
    {
        uint32_t x = state_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state_ = x;
        return x;
    }

    int next_int(int lo, int hi_inclusive)
    {
        const uint32_t span = static_cast<uint32_t>(hi_inclusive - lo + 1);
        return lo + static_cast<int>(next_u32() % span);
    }

    float next_float() { return (next_u32() >> 8) * (1.0f / 16777216.0f); }

private:
    uint32_t state_;
};

inline uint32_t mix_seed(uint32_t seed, uint32_t salt)
{
    uint32_t x = seed ^ (salt * 0x9E3779B9u);
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

}  // namespace artillery
