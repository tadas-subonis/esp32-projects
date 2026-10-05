#pragma once

#include "artillery/types.hpp"

#include <cstdint>

namespace artillery {

struct ClientIntent {
    enum Kind : uint8_t {
        ToggleSelect = 0,
        Start,
        Rematch,
        ToTitle,
        SetAngle,
        SetPower,
        NudgeAngle,
        NudgePower,
        Fire,
    };

    Kind kind = Fire;
    int value = 0;
    uint32_t seed = 0;
};

struct ApplyResult {
    bool accepted = false;
    const char* error = "";
};

inline ApplyResult apply_ok() { return ApplyResult{true, ""}; }

inline ApplyResult apply_reject(const char* error) { return ApplyResult{false, error}; }

inline const char* mode_name(Mode m)
{
    switch (m) {
        case Mode::VsBot:
            return "bot";
        case Mode::Hotseat:
            return "hotseat";
        case Mode::Pvp:
            return "pvp";
        default:
            return "unknown";
    }
}

inline Mode mode_from_name(const char* s)
{
    if (s == nullptr) {
        return Mode::VsBot;
    }
    if (s[0] == 'h') {
        return Mode::Hotseat;
    }
    if (s[0] == 'p') {
        return Mode::Pvp;
    }
    return Mode::VsBot;
}

inline const char* phase_name(Phase p)
{
    switch (p) {
        case Phase::Title:
            return "title";
        case Phase::Aiming:
            return "aiming";
        case Phase::Firing:
            return "firing";
        case Phase::Resolving:
            return "resolving";
        case Phase::GameOver:
            return "gameover";
        default:
            return "unknown";
    }
}

inline const char* player_name(PlayerId p)
{
    return p == PlayerId::P0 ? "P0" : "P1";
}

inline const char* intent_name(ClientIntent::Kind k)
{
    switch (k) {
        case ClientIntent::ToggleSelect:
            return "select";
        case ClientIntent::Start:
            return "start";
        case ClientIntent::Rematch:
            return "rematch";
        case ClientIntent::ToTitle:
            return "title";
        case ClientIntent::SetAngle:
            return "angle";
        case ClientIntent::SetPower:
            return "power";
        case ClientIntent::NudgeAngle:
            return "nudge_angle";
        case ClientIntent::NudgePower:
            return "nudge_power";
        case ClientIntent::Fire:
            return "fire";
        default:
            return "unknown";
    }
}

}  // namespace artillery
