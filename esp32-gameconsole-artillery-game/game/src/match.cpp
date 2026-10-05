#include "artillery/match.hpp"

#include "artillery/bot.hpp"
#include "artillery/log.hpp"
#include "artillery/rng.hpp"

namespace artillery {

void Match::reset_title()
{
    phase_ = Phase::Title;
    title_sel_ = 0;
    players_ = 0;
    have_winner_ = false;
    proj_.alive = false;
    scene_dirty_ = true;
    log_msg("title screen");
}

void Match::start(uint32_t seed, Mode mode)
{
    seed_ = seed == 0 ? 1 : seed;
    mode_ = mode;
    world_.generate(seed_);
    turn_ = 0;
    active_ = PlayerId::P0;
    have_winner_ = false;
    proj_.alive = false;
    saved_power_[0] = kDefaultPower;
    saved_power_[1] = kDefaultPower;
    saved_angle_[0] = kDefaultAngle;
    saved_angle_[1] = kDefaultAngle;
    phase_ = Phase::Aiming;
    log_msg("match start seed=%u mode=%s", static_cast<unsigned>(seed_), mode_name(mode_));
    begin_turn();
    scene_dirty_ = true;
}

void Match::set_angle(int deg)
{
    const int next = clamp_int(deg, kMinAngle, kMaxAngle);
    if (next == angle_) {
        return;
    }
    angle_ = next;
    hud_dirty_ = true;
}

void Match::set_power(int pwr)
{
    const int next = clamp_int(pwr, kMinPower, kMaxPower);
    if (next == power_) {
        return;
    }
    power_ = next;
    hud_dirty_ = true;
}

void Match::roll_wind()
{
    Rng rng(mix_seed(seed_, static_cast<uint32_t>(turn_ + 1) * 0x9E3779B9u));
    wind_ = rng.next_int(-12, 12);
}

void Match::begin_turn()
{
    roll_wind();
    angle_ = saved_angle_[static_cast<int>(active_)];
    power_ = saved_power_[static_cast<int>(active_)];
    bot_think_ms_ = mode_ == Mode::VsBot && active_ == PlayerId::P1 ? 550 : 0;
    phase_ = Phase::Aiming;
    scene_dirty_ = true;
    log_msg("turn %d active=%s wind=%d angle=%d power=%d hp=%d/%d%s", turn_, player_name(active_),
            wind_, angle_, power_, world_.tank(PlayerId::P0).hp, world_.tank(PlayerId::P1).hp,
            bot_think_ms_ > 0 ? " (bot thinking)" : "");
}

bool Match::is_human_turn() const
{
    if (phase_ != Phase::Aiming) {
        return false;
    }
    if (mode_ == Mode::VsBot) {
        return active_ == PlayerId::P0;
    }
    return true;
}

bool Match::can_act(PlayerId who) const
{
    if (phase_ != Phase::Aiming) {
        return false;
    }
    return who == active_;
}

ApplyResult Match::apply(PlayerId who, ClientIntent intent)
{
    auto done = [&](ApplyResult r) {
        const bool noisy =
            intent.kind == ClientIntent::NudgeAngle || intent.kind == ClientIntent::NudgePower;
        if (!noisy) {
            if (r.accepted) {
                log_msg("cmd %s who=%s value=%d", intent_name(intent.kind), player_name(who),
                        intent.value);
            } else {
                log_msg("cmd %s who=%s REJECT %s", intent_name(intent.kind), player_name(who),
                        r.error);
            }
        }
        return r;
    };

    switch (intent.kind) {
        case ClientIntent::ToggleSelect:
            if (phase_ != Phase::Title) {
                return done(apply_reject("not_title"));
            }
            title_sel_ = 1 - title_sel_;
            scene_dirty_ = true;
            log_msg("title select %s", title_sel_ == 0 ? "vs-bot" : "hotseat");
            return done(apply_ok());
        case ClientIntent::Start: {
            if (phase_ != Phase::Title && phase_ != Phase::GameOver) {
                return done(apply_reject("already_playing"));
            }
            Mode mode = mode_;
            if (intent.value >= 0 && intent.value <= 2) {
                mode = static_cast<Mode>(intent.value);
            } else {
                mode = title_sel_ == 0 ? Mode::VsBot : Mode::Hotseat;
            }
            const uint32_t seed = intent.seed == 0 ? seed_ : intent.seed;
            start(seed, mode);
            return done(apply_ok());
        }
        case ClientIntent::Rematch:
            if (phase_ != Phase::GameOver) {
                return done(apply_reject("not_gameover"));
            }
            start(mix_seed(seed_, 0xC0FFEEu), mode_);
            return done(apply_ok());
        case ClientIntent::ToTitle:
            if (phase_ != Phase::GameOver) {
                return done(apply_reject("not_gameover"));
            }
            reset_title();
            return done(apply_ok());
        case ClientIntent::SetAngle:
            if (phase_ != Phase::Aiming) {
                return done(apply_reject("not_aiming"));
            }
            if (!can_act(who)) {
                return done(apply_reject("not_your_turn"));
            }
            set_angle(intent.value);
            return done(apply_ok());
        case ClientIntent::SetPower:
            if (phase_ != Phase::Aiming) {
                return done(apply_reject("not_aiming"));
            }
            if (!can_act(who)) {
                return done(apply_reject("not_your_turn"));
            }
            set_power(intent.value);
            return done(apply_ok());
        case ClientIntent::NudgeAngle:
            if (phase_ != Phase::Aiming) {
                return done(apply_reject("not_aiming"));
            }
            if (!can_act(who)) {
                return done(apply_reject("not_your_turn"));
            }
            set_angle(angle_ + intent.value);
            return done(apply_ok());
        case ClientIntent::NudgePower:
            if (phase_ != Phase::Aiming) {
                return done(apply_reject("not_aiming"));
            }
            if (!can_act(who)) {
                return done(apply_reject("not_your_turn"));
            }
            set_power(power_ + intent.value);
            return done(apply_ok());
        case ClientIntent::Fire:
            if (phase_ != Phase::Aiming) {
                return done(apply_reject("not_aiming"));
            }
            if (!can_act(who)) {
                return done(apply_reject("not_your_turn"));
            }
            if (!fire_current()) {
                return done(apply_reject("not_aiming"));
            }
            return done(apply_ok());
        default:
            return done(apply_reject("unknown_intent"));
    }
}

ViewModel Match::view() const
{
    ViewModel v;
    v.snap = snapshot();
    v.title_sel = title_sel_;
    v.players = players_;
    v.heights = world_.heights();
    v.have_heights = true;
    v.tanks[0] = world_.tank(PlayerId::P0);
    v.tanks[1] = world_.tank(PlayerId::P1);
    v.tank_angle[0] = active_ == PlayerId::P0 ? angle_ : saved_angle_[0];
    v.tank_angle[1] = active_ == PlayerId::P1 ? angle_ : saved_angle_[1];
    v.proj = proj_;
    return v;
}

bool Match::fire_shot(Shot shot)
{
    if (phase_ != Phase::Aiming) {
        return false;
    }
    shot.angle_deg = clamp_int(shot.angle_deg, kMinAngle, kMaxAngle);
    shot.power = clamp_int(shot.power, kMinPower, kMaxPower);
    angle_ = shot.angle_deg;
    power_ = shot.power;
    saved_power_[static_cast<int>(active_)] = power_;
    saved_angle_[static_cast<int>(active_)] = angle_;
    proj_ = launch_shot(world_, active_, shot, wind_);
    pending_ = ShotOutcome{};
    fire_step_carry_ = 0;
    phase_ = Phase::Firing;
    log_msg("FIRE %s angle=%d power=%d wind=%d", player_name(active_), angle_, power_, wind_);
    return true;
}

bool Match::fire_current()
{
    return fire_shot(Shot{angle_, power_});
}

void Match::finish_shot()
{
    pending_ = outcome_at(world_, proj_.pos);
    apply_explosion(world_, pending_, active_);
    scene_dirty_ = true;
    resolve_frames_ = 18;
    log_msg("impact x=%d y=%d terrain=%d left=%d hit0=%d hit1=%d dmg=%d/%d hp=%d/%d",
            static_cast<int>(pending_.impact.x + 0.5f), static_cast<int>(pending_.impact.y + 0.5f),
            pending_.hit_terrain ? 1 : 0, pending_.left_map ? 1 : 0, pending_.hit_tank[0] ? 1 : 0,
            pending_.hit_tank[1] ? 1 : 0, pending_.damage[0], pending_.damage[1],
            world_.tank(PlayerId::P0).hp, world_.tank(PlayerId::P1).hp);

    const bool p0 = world_.tank(PlayerId::P0).alive;
    const bool p1 = world_.tank(PlayerId::P1).alive;
    if (!p0 || !p1) {
        have_winner_ = true;
        if (p0 && !p1) {
            winner_ = PlayerId::P0;
        } else if (p1 && !p0) {
            winner_ = PlayerId::P1;
        } else {
            winner_ = active_;
        }
        phase_ = Phase::GameOver;
        log_msg("game over winner=%s", player_name(winner_));
        return;
    }

    phase_ = Phase::Resolving;
}

void Match::handle_buttons(const ButtonEdges& edges)
{
    if (phase_ == Phase::Title) {
        if (edges.pressed.left || edges.pressed.right || edges.pressed.up || edges.pressed.down ||
            edges.pressed.b) {
            apply(PlayerId::P0, ClientIntent{ClientIntent::ToggleSelect});
        }
        if (edges.pressed.a) {
            ClientIntent in;
            in.kind = ClientIntent::Start;
            in.value = title_sel_ == 0 ? static_cast<int>(Mode::VsBot) : static_cast<int>(Mode::Hotseat);
            apply(PlayerId::P0, in);
        }
        return;
    }

    if (phase_ == Phase::GameOver) {
        if (edges.pressed.a) {
            apply(PlayerId::P0, ClientIntent{ClientIntent::Rematch});
        }
        if (edges.pressed.b) {
            apply(PlayerId::P0, ClientIntent{ClientIntent::ToTitle});
        }
        return;
    }

    if (!is_human_turn()) {
        return;
    }

    const int step_a = edges.down.a ? 0 : (aim_repeat_ms_ > 280 ? 2 : 1);
    if (edges.down.left) {
        apply(active_, ClientIntent{ClientIntent::NudgeAngle,
                                    active_ == PlayerId::P0 ? step_a : -step_a});
    }
    if (edges.down.right) {
        apply(active_, ClientIntent{ClientIntent::NudgeAngle,
                                    active_ == PlayerId::P0 ? -step_a : step_a});
    }
    if (edges.down.up) {
        apply(active_, ClientIntent{ClientIntent::NudgePower, step_a});
    }
    if (edges.down.down) {
        apply(active_, ClientIntent{ClientIntent::NudgePower, -step_a});
    }
    if (edges.pressed.a) {
        apply(active_, ClientIntent{ClientIntent::Fire});
    }
}

void Match::tick(uint32_t dt_ms)
{
    dt_ms = sim_dt_ms(dt_ms);
    if (phase_ == Phase::Aiming && is_human_turn()) {
        aim_repeat_ms_ += static_cast<int>(dt_ms);
    } else {
        aim_repeat_ms_ = 0;
    }

    if (phase_ == Phase::Aiming && mode_ == Mode::VsBot && active_ == PlayerId::P1) {
        bot_think_ms_ -= static_cast<int>(dt_ms);
        if (bot_think_ms_ <= 0) {
            const Shot s = choose_bot_shot(world_, PlayerId::P1, wind_,
                                          saved_power_[static_cast<int>(PlayerId::P1)]);
            log_msg("bot chose angle=%d power=%d", s.angle_deg, s.power);
            bot_shot_ = s;
            bot_fired_ = true;
            fire_shot(s);
        }
        return;
    }

    if (phase_ == Phase::Firing) {
        fire_step_carry_ += static_cast<int>(dt_ms);
        const int steps = fire_step_carry_ / kShotStepSimMs;
        fire_step_carry_ %= kShotStepSimMs;
        for (int i = 0; i < steps; ++i) {
            if (!step_projectile(world_, proj_, wind_)) {
                finish_shot();
                break;
            }
        }
        return;
    }

    if (phase_ == Phase::Resolving) {
        resolve_frames_ -= 1;
        if (resolve_frames_ <= 0) {
            active_ = other_player(active_);
            turn_ += 1;
            begin_turn();
        }
    }
}

MatchSnapshot Match::snapshot() const
{
    MatchSnapshot s;
    s.phase = phase_;
    s.mode = mode_;
    s.seed = seed_;
    s.turn = turn_;
    s.active = active_;
    s.winner = winner_;
    s.have_winner = have_winner_;
    s.angle = angle_;
    s.power = power_;
    s.wind = wind_;
    s.hp[0] = world_.tank(PlayerId::P0).hp;
    s.hp[1] = world_.tank(PlayerId::P1).hp;
    s.tank_x[0] = static_cast<int>(world_.tank(PlayerId::P0).x);
    s.tank_x[1] = static_cast<int>(world_.tank(PlayerId::P1).x);
    s.tank_y[0] = static_cast<int>(world_.tank(PlayerId::P0).y);
    s.tank_y[1] = static_cast<int>(world_.tank(PlayerId::P1).y);
    s.firing = proj_.alive;
    s.proj_x = proj_.pos.x;
    s.proj_y = proj_.pos.y;
    return s;
}

void Match::restore(const ViewModel& v)
{
    seed_ = v.snap.seed == 0 ? 1 : v.snap.seed;
    mode_ = v.snap.mode;
    phase_ = v.snap.phase;
    turn_ = v.snap.turn;
    active_ = v.snap.active;
    winner_ = v.snap.winner;
    have_winner_ = v.snap.have_winner;
    angle_ = v.snap.angle;
    power_ = v.snap.power;
    wind_ = v.snap.wind;
    title_sel_ = v.title_sel;
    players_ = v.players;
    saved_angle_[0] = v.tank_angle[0];
    saved_angle_[1] = v.tank_angle[1];
    saved_power_[static_cast<int>(active_)] = v.snap.power;
    if (v.have_heights) {
        world_.replace_heights(v.heights);
    }
    world_.tank(PlayerId::P0) = v.tanks[0];
    world_.tank(PlayerId::P1) = v.tanks[1];
    world_.tank(PlayerId::P0).x = static_cast<float>(v.snap.tank_x[0]);
    world_.tank(PlayerId::P1).x = static_cast<float>(v.snap.tank_x[1]);
    world_.tank(PlayerId::P0).y = static_cast<float>(v.snap.tank_y[0]);
    world_.tank(PlayerId::P1).y = static_cast<float>(v.snap.tank_y[1]);
    world_.tank(PlayerId::P0).hp = v.snap.hp[0];
    world_.tank(PlayerId::P1).hp = v.snap.hp[1];
    world_.tank(PlayerId::P0).alive = v.snap.hp[0] > 0;
    world_.tank(PlayerId::P1).alive = v.snap.hp[1] > 0;
    proj_ = v.proj;
    proj_.alive = v.snap.firing;
    proj_.pos.x = v.snap.proj_x;
    proj_.pos.y = v.snap.proj_y;
    pending_ = ShotOutcome{};
    fire_step_carry_ = 0;
    bot_think_ms_ = 0;
    bot_fired_ = false;
    resolve_frames_ = phase_ == Phase::Resolving ? 18 : 0;
    scene_dirty_ = true;
    hud_dirty_ = true;
}

void Match::step_fx(uint32_t dt_ms)
{
    if (phase_ != Phase::Firing) {
        return;
    }
    dt_ms = sim_dt_ms(dt_ms);
    fire_step_carry_ += static_cast<int>(dt_ms);
    const int steps = fire_step_carry_ / kShotStepSimMs;
    fire_step_carry_ %= kShotStepSimMs;
    for (int i = 0; i < steps; ++i) {
        if (!step_projectile(world_, proj_, wind_)) {
            break;
        }
    }
}

bool Match::consume_bot_fire(Shot* out)
{
    if (!bot_fired_) {
        return false;
    }
    if (out != nullptr) {
        *out = bot_shot_;
    }
    bot_fired_ = false;
    return true;
}

}  // namespace artillery
