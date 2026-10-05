#pragma once

#include "artillery/physics.hpp"
#include "artillery/types.hpp"
#include "artillery/command.hpp"
#include "artillery/world.hpp"

#include <array>
#include <cstdint>

namespace artillery {

struct MatchSnapshot {
    uint32_t seq = 0;
    Phase phase = Phase::Title;
    Mode mode = Mode::VsBot;
    uint32_t seed = 1;
    int turn = 0;
    PlayerId active = PlayerId::P0;
    PlayerId winner = PlayerId::P0;
    bool have_winner = false;
    int angle = kDefaultAngle;
    int power = kDefaultPower;
    int wind = 0;
    int hp[2] = {kTankHp, kTankHp};
    int tank_x[2] = {0, 0};
    int tank_y[2] = {0, 0};
    bool firing = false;
    float proj_x = 0;
    float proj_y = 0;
};

struct ViewModel {
    MatchSnapshot snap{};
    int title_sel = 0;
    int players = 0;
    TitleUi title_ui = TitleUi::LocalSelect;
    std::array<uint16_t, kWidth> heights{};
    bool have_heights = false;
    Tank tanks[2]{};
    int tank_angle[2] = {kDefaultAngle, kDefaultAngle};
    Projectile proj{};
};

inline void stamp_title_ui(ViewModel& v, bool remote, bool connected)
{
    if (!remote || v.snap.phase != Phase::Title) {
        v.title_ui = TitleUi::LocalSelect;
        return;
    }
    if (!connected) {
        v.title_ui = TitleUi::Connecting;
        return;
    }
    v.title_ui = v.players >= 2 ? TitleUi::Starting : TitleUi::Waiting;
}

class Match {
public:
    void reset_title();
    void start(uint32_t seed, Mode mode);
    void handle_buttons(const ButtonEdges& edges);
    void tick(uint32_t dt_ms);

    bool fire_current();
    bool fire_shot(Shot shot);

    ApplyResult apply(PlayerId who, ClientIntent intent);
    bool can_act(PlayerId who) const;
    void restore(const ViewModel& view);
    void step_fx(uint32_t dt_ms);
    bool consume_bot_fire(Shot* out);

    const World& world() const { return world_; }
    World& world() { return world_; }
    const Projectile& projectile() const { return proj_; }
    MatchSnapshot snapshot() const;
    ViewModel view() const;

    Phase phase() const { return phase_; }
    Mode mode() const { return mode_; }
    PlayerId active() const { return active_; }
    int angle() const { return angle_; }
    int power() const { return power_; }
    int wind() const { return wind_; }
    uint32_t seed() const { return seed_; }
    int title_sel() const { return title_sel_; }
    void set_seed(uint32_t seed) { seed_ = seed == 0 ? 1 : seed; }
    bool scene_dirty() const { return scene_dirty_; }
    void clear_scene_dirty() { scene_dirty_ = false; }
    bool hud_dirty() const { return hud_dirty_; }
    void clear_hud_dirty() { hud_dirty_ = false; }
    int resolve_frames_left() const { return resolve_frames_; }

    void set_angle(int deg);
    void set_power(int pwr);
    void set_mode(Mode mode) { mode_ = mode; }

private:
    void begin_turn();
    void finish_shot();
    void roll_wind();
    bool is_human_turn() const;

    World world_{};
    Projectile proj_{};
    ShotOutcome pending_{};
    Phase phase_ = Phase::Title;
    Mode mode_ = Mode::VsBot;
    PlayerId active_ = PlayerId::P0;
    uint32_t seed_ = 1;
    int turn_ = 0;
    int angle_ = kDefaultAngle;
    int power_ = kDefaultPower;
    int wind_ = 0;
    int title_sel_ = 0;
    int players_ = 0;
    int bot_think_ms_ = 0;
    int resolve_frames_ = 0;
    int aim_repeat_ms_ = 0;
    int fire_step_carry_ = 0;
    int saved_power_[2] = {kDefaultPower, kDefaultPower};
    int saved_angle_[2] = {kDefaultAngle, kDefaultAngle};
    bool scene_dirty_ = true;
    bool hud_dirty_ = true;
    bool have_winner_ = false;
    bool bot_fired_ = false;
    PlayerId winner_ = PlayerId::P0;
    Shot bot_shot_{};
};

}  // namespace artillery
