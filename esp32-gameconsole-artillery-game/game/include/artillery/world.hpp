#pragma once

#include "artillery/types.hpp"

#include <array>
#include <cstdint>

namespace artillery {

class World {
public:
    void generate(uint32_t seed);

    uint32_t seed() const { return seed_; }
    uint16_t height_at(int x) const;
    void apply_crater(int cx, int cy, int radius);
    void settle_tank(Tank& tank) const;
    bool solid_at(float x, float y) const;

    Tank& tank(PlayerId id) { return tanks_[static_cast<int>(id)]; }
    const Tank& tank(PlayerId id) const { return tanks_[static_cast<int>(id)]; }

    const std::array<uint16_t, kWidth>& heights() const { return height_; }
    void replace_heights(const std::array<uint16_t, kWidth>& heights);

private:
    void flatten_platform(int cx);

    uint32_t seed_ = 1;
    std::array<uint16_t, kWidth> height_{};
    Tank tanks_[2]{};
};

}  // namespace artillery
