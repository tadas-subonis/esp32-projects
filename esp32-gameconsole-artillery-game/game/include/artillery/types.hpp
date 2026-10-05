#pragma once

#include "artillery/config.hpp"

#include <cstdint>

namespace artillery {

enum class Phase : uint8_t {
    Title = 0,
    Aiming,
    Firing,
    Resolving,
    GameOver,
};

enum class Mode : uint8_t {
    VsBot = 0,
    Hotseat,
    Pvp,
};

enum class TitleUi : uint8_t {
    LocalSelect = 0,
    Connecting,
    Waiting,
    Starting,
    Rejected,
};

enum class PlayerId : uint8_t { P0 = 0, P1 = 1 };

struct Vec2 {
    float x = 0;
    float y = 0;
};

struct Tank {
    float x = 0;
    float y = 0;
    int hp = kTankHp;
    bool alive = true;
};

struct Shot {
    int angle_deg = 45;
    int power = kDefaultPower;
};

struct Projectile {
    Vec2 pos;
    Vec2 vel;
    bool alive = false;
    PlayerId shooter = PlayerId::P0;
};

struct ShotOutcome {
    Vec2 impact{};
    bool hit_terrain = false;
    bool left_map = false;
    bool hit_tank[2] = {false, false};
    int steps = 0;
    int damage[2] = {0, 0};
};

struct Buttons {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool a = false;
    bool b = false;
};

struct ButtonEdges {
    Buttons down;
    Buttons pressed;
    Buttons released;
};

inline PlayerId other_player(PlayerId p)
{
    return p == PlayerId::P0 ? PlayerId::P1 : PlayerId::P0;
}

inline int clamp_int(int v, int lo, int hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

}  // namespace artillery
