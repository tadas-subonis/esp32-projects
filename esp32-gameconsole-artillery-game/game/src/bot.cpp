#include "artillery/bot.hpp"

#include "artillery/physics.hpp"
#include "artillery/rng.hpp"

namespace artillery {

Shot choose_bot_shot(const World& world, PlayerId bot, int wind, int last_power)
{
    const Tank& me = world.tank(bot);
    const Tank& foe = world.tank(other_player(bot));
    Rng rng(mix_seed(static_cast<uint32_t>(me.x * 17 + foe.x * 31 + wind * 13 + last_power * 7), 0xB07u));

    // Practice dummy: aim well wide of the tank so the human can take turns.
    const float miss = 56.0f + rng.next_float() * 72.0f;
    const float dir = rng.next_int(0, 1) == 0 ? -1.0f : 1.0f;
    const float target_x = foe.x + dir * miss;
    const int power_lo = clamp_int(last_power - 8, kMinPower, kMaxPower);
    const int power_hi = clamp_int(last_power + 8, kMinPower, kMaxPower);

    Shot best{45, last_power};
    float best_score = 1.0e9f;
    for (int i = 0; i < 16; ++i) {
        Shot s;
        s.angle_deg = rng.next_int(20, 75);
        s.power = rng.next_int(power_lo, power_hi);
        const ShotOutcome out = simulate_shot(world, bot, s, wind);
        const float dx = out.impact.x - target_x;
        float score = dx * dx;
        if (out.left_map) {
            score += 40000.0f;
        }
        if (score < best_score) {
            best_score = score;
            best = s;
        }
    }

    best.angle_deg = clamp_int(best.angle_deg + rng.next_int(-10, 10), kMinAngle, 85);
    best.power = clamp_int(best.power + rng.next_int(-4, 4), kMinPower, kMaxPower);
    return best;
}

}  // namespace artillery
