#pragma once

#include "artillery/command.hpp"
#include "artillery/config.hpp"
#include "artillery/match.hpp"
#include "artillery_protocol.h"
#include "bytes.hpp"

#include <array>
#include <cstdint>

namespace artillery {

enum class WireMsg : uint8_t {
    Hello = 1,
    Welcome = 2,
    Error = 3,
    Intent = 4,
    Cmd = 5,
    Aim = 6,
    State = 7,
    Sync = 8,
};

enum WireStateFlags : uint8_t {
    WireStateFullHeights = 1u << 0,
    WireStateDeltas = 1u << 1,
    WireStateHaveWinner = 1u << 2,
    WireStateAlive0 = 1u << 3,
    WireStateAlive1 = 1u << 4,
    WireStateFiring = 1u << 5,
};

struct WireHello {
    uint8_t protocol = ARTILLERY_PROTOCOL_VERSION;
    uint8_t want = 0;  // 0 bot, 1 hotseat, 2 pvp
    char token[ARTILLERY_TOKEN_BYTES + 1]{};
    uint32_t seq = 0;
    uint32_t eid = 0;
    uint32_t t_ms = 0;
};

struct WireWelcome {
    int8_t seat = 0;  // -1 = both (hotseat)
    uint8_t want = 0;
    uint32_t seq = 0;
    char token[ARTILLERY_TOKEN_BYTES + 1]{};
    uint32_t eid = 0;
    uint32_t t_ms = 0;
};

struct WireError {
    uint8_t cmd = 0;
    char error[ARTILLERY_ERROR_BYTES + 1]{};
    uint32_t eid = 0;
    uint32_t t_ms = 0;
};

struct WireIntent {
    ClientIntent intent{};
};

struct WireCmd {
    uint32_t seq = 0;
    PlayerId who = PlayerId::P0;
    ClientIntent intent{};
};

struct WireAim {
    uint8_t who = 0;
    uint8_t angle = kDefaultAngle;
    uint8_t power = kDefaultPower;
};

struct WireDelta {
    uint16_t x = 0;
    uint16_t height = 0;
};

struct WireState {
    uint32_t seq = 0;
    uint8_t phase = 0;
    uint8_t mode = 0;
    uint32_t seed = 1;
    uint8_t turn = 0;
    uint8_t active = 0;
    uint8_t winner = 0;
    uint8_t flags = 0;
    int16_t angle = kDefaultAngle;
    int16_t power = kDefaultPower;
    int16_t wind = 0;
    int16_t hp0 = kTankHp;
    int16_t hp1 = kTankHp;
    int16_t tank_x0 = 0;
    int16_t tank_y0 = 0;
    int16_t tank_x1 = 0;
    int16_t tank_y1 = 0;
    uint8_t angle0 = kDefaultAngle;
    uint8_t angle1 = kDefaultAngle;
    uint8_t players = 0;
    uint8_t title_sel = 0;
    float proj_x = 0;
    float proj_y = 0;
    std::array<uint16_t, kWidth> heights{};
    bool have_heights = false;
    WireDelta deltas[64]{};
    uint8_t delta_count = 0;
};

struct WireSync {
    uint32_t seq = 0;
};

uint8_t want_to_u8(const char* want);
void want_from_u8(uint8_t v, char* out, size_t cap);

int encode_hello(uint8_t* buf, size_t cap, const WireHello& m);
int encode_welcome(uint8_t* buf, size_t cap, const WireWelcome& m);
int encode_error(uint8_t* buf, size_t cap, const WireError& m);
int encode_intent(uint8_t* buf, size_t cap, const WireIntent& m);
int encode_cmd(uint8_t* buf, size_t cap, const WireCmd& m);
int encode_aim(uint8_t* buf, size_t cap, const WireAim& m);
int encode_state(uint8_t* buf, size_t cap, const WireState& m);
int encode_sync(uint8_t* buf, size_t cap, const WireSync& m);

bool decode_hello(const uint8_t* buf, size_t len, WireHello* out);
bool decode_welcome(const uint8_t* buf, size_t len, WireWelcome* out);
bool decode_error(const uint8_t* buf, size_t len, WireError* out);
bool decode_intent(const uint8_t* buf, size_t len, WireIntent* out);
bool decode_cmd(const uint8_t* buf, size_t len, WireCmd* out);
bool decode_aim(const uint8_t* buf, size_t len, WireAim* out);
bool decode_state(const uint8_t* buf, size_t len, WireState* out);
bool decode_sync(const uint8_t* buf, size_t len, WireSync* out);

WireState state_from_view(const ViewModel& view, bool full_heights);
void apply_state_to_view(const WireState& st, ViewModel* view);
void collect_height_deltas(const std::array<uint16_t, kWidth>& before,
                           const std::array<uint16_t, kWidth>& after, WireState* st);

/** Encode float as i32 milli-units (proj coords). */
inline int32_t float_to_milli(float v)
{
    return static_cast<int32_t>(v * 1000.0f);
}
inline float milli_to_float(int32_t v)
{
    return static_cast<float>(v) / 1000.0f;
}

}  // namespace artillery
