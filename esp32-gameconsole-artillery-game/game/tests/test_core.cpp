#include "artillery/bot.hpp"
#include "artillery/match.hpp"
#include "artillery/physics.hpp"
#include "artillery/render.hpp"
#include "artillery/world.hpp"
#include "authority.hpp"
#include "artillery_protocol.h"
#include "codec.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace artillery;

static int g_fails = 0;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
            ++g_fails;                                                              \
        }                                                                           \
    } while (0)

static void test_world_should_be_deterministic_for_a_seed()
{
    World a;
    World b;
    a.generate(42);
    b.generate(42);
    for (int x = 0; x < kWidth; ++x) {
        CHECK(a.height_at(x) == b.height_at(x));
    }
    CHECK(static_cast<int>(a.tank(PlayerId::P0).x) == static_cast<int>(b.tank(PlayerId::P0).x));
}

static void test_world_should_place_tanks_on_opposite_slopes()
{
    World w;
    w.generate(7);
    CHECK(w.tank(PlayerId::P0).x < w.tank(PlayerId::P1).x);
    CHECK(w.tank(PlayerId::P0).alive);
    CHECK(w.tank(PlayerId::P1).alive);
}

static void test_crater_should_lower_the_heightmap()
{
    World w;
    w.generate(3);
    const int x = static_cast<int>(w.tank(PlayerId::P0).x);
    const int before = w.height_at(x);
    w.apply_crater(x, before, 16);
    CHECK(w.height_at(x) >= before);
}

static void test_flat_shot_should_hit_terrain_or_leave_the_map()
{
    World w;
    w.generate(11);
    const ShotOutcome out = simulate_shot(w, PlayerId::P0, Shot{55, 80}, 0);
    CHECK(out.steps > 5);
    CHECK(out.hit_terrain || out.left_map || out.hit_tank[0] || out.hit_tank[1]);
}

static void test_wind_should_push_the_shell_along_x()
{
    World w;
    w.generate(11);
    const Shot s{70, 85};
    const ShotOutcome left = simulate_shot(w, PlayerId::P0, s, -12);
    const ShotOutcome right = simulate_shot(w, PlayerId::P0, s, 12);
    CHECK(right.impact.x > left.impact.x);
}

static void test_sim_dt_should_be_seventy_percent_of_wall_time()
{
    CHECK(sim_dt_ms(16) == 11);
    CHECK(sim_dt_ms(10) == 7);
    CHECK(sim_dt_ms(0) == 0);
}

static void test_match_should_enter_aiming_after_start()
{
    Match m;
    m.start(99, Mode::Hotseat);
    CHECK(m.phase() == Phase::Aiming);
    CHECK(m.active() == PlayerId::P0);
}

static void test_fire_should_move_match_into_firing_then_resolve()
{
    Match m;
    m.start(5, Mode::Hotseat);
    CHECK(m.fire_current());
    CHECK(m.phase() == Phase::Firing);
    for (int i = 0; i < 4000 && m.phase() == Phase::Firing; ++i) {
        m.tick(16);
    }
    CHECK(m.phase() == Phase::Resolving || m.phase() == Phase::GameOver);
}

static void play_until_aiming(Match& m)
{
    for (int i = 0; i < 8000 && m.phase() == Phase::Firing; ++i) {
        m.tick(16);
    }
    for (int i = 0; i < 64 && m.phase() == Phase::Resolving; ++i) {
        m.tick(16);
    }
}

static void test_match_should_restore_each_players_last_aim()
{
    Match m;
    m.start(99, Mode::Hotseat);
    m.set_angle(62);
    m.set_power(72);
    CHECK(m.fire_current());
    play_until_aiming(m);
    CHECK(m.phase() == Phase::Aiming);
    CHECK(m.active() == PlayerId::P1);
    CHECK(m.power() == kDefaultPower);
    CHECK(m.angle() == kDefaultAngle);
    m.set_angle(110);
    m.set_power(36);
    CHECK(m.fire_current());
    play_until_aiming(m);
    CHECK(m.phase() == Phase::Aiming);
    CHECK(m.active() == PlayerId::P0);
    CHECK(m.power() == 72);
    CHECK(m.angle() == 62);
}

static void test_bot_should_return_a_legal_shot()
{
    World w;
    w.generate(21);
    const Shot s = choose_bot_shot(w, PlayerId::P1, 0);
    CHECK(s.angle_deg >= kMinAngle);
    CHECK(s.angle_deg <= kMaxAngle);
    CHECK(s.power >= kMinPower);
    CHECK(s.power <= kMaxPower);
}

static void test_bot_should_usually_miss_the_player()
{
    int hits = 0;
    for (uint32_t seed = 1; seed <= 24; ++seed) {
        World w;
        w.generate(seed);
        const Shot s = choose_bot_shot(w, PlayerId::P1, 0);
        const ShotOutcome out = simulate_shot(w, PlayerId::P1, s, 0);
        if (out.hit_tank[static_cast<int>(PlayerId::P0)]) {
            ++hits;
        }
    }
    CHECK(hits <= 6);
}

static void test_server_should_reject_fire_when_it_is_not_your_turn()
{
    Match m;
    m.start(9, Mode::Pvp);
    const ApplyResult bad = m.apply(PlayerId::P1, ClientIntent{ClientIntent::Fire});
    CHECK(!bad.accepted);
    CHECK(std::strcmp(bad.error, "not_your_turn") == 0);
    const ApplyResult good = m.apply(PlayerId::P0, ClientIntent{ClientIntent::Fire});
    CHECK(good.accepted);
    CHECK(m.phase() == Phase::Firing);
}

static void test_server_should_reject_aiming_from_the_waiting_player()
{
    Match m;
    m.start(9, Mode::VsBot);
    const ApplyResult bad = m.apply(PlayerId::P1, ClientIntent{ClientIntent::SetAngle, 30});
    CHECK(!bad.accepted);
    const ApplyResult good = m.apply(PlayerId::P0, ClientIntent{ClientIntent::SetAngle, 30});
    CHECK(good.accepted);
    CHECK(m.angle() == 30);
}

static void test_local_authority_should_reject_fire_before_start()
{
    Authority auth;
    ClientSeat seat;
    CHECK(auth.join(&seat, 0, "bot").accepted);
    const ApplyResult bad = auth.submit(seat, ClientIntent{ClientIntent::Fire});
    CHECK(!bad.accepted);
    CHECK(std::strcmp(bad.error, "not_aiming") == 0);
}

static void test_local_authority_should_simulate_a_legal_shot()
{
    Authority auth;
    ClientSeat seat;
    CHECK(auth.join(&seat, 0, "bot").accepted);
    ClientIntent start;
    start.kind = ClientIntent::Start;
    start.value = static_cast<int>(Mode::VsBot);
    start.seed = 11;
    CHECK(auth.submit(seat, start).accepted);
    CHECK(auth.phase() == Phase::Aiming);
    CHECK(auth.submit(seat, ClientIntent{ClientIntent::SetAngle, 55}).accepted);
    CHECK(auth.submit(seat, ClientIntent{ClientIntent::SetPower, 80}).accepted);
    CHECK(auth.submit(seat, ClientIntent{ClientIntent::Fire}).accepted);
    CHECK(auth.phase() == Phase::Firing);
    auth.tick(16);
    CHECK(auth.view().snap.firing || auth.phase() != Phase::Aiming);
}

static void test_pvp_authority_should_need_two_players_to_start()
{
    Authority auth;
    ClientSeat a;
    CHECK(auth.join(&a, ARTILLERY_PROTOCOL_VERSION, "pvp").accepted);
    CHECK(a.seat == 0);
    CHECK(auth.view().players == 1);
    ClientIntent start;
    start.kind = ClientIntent::Start;
    start.value = static_cast<int>(Mode::Pvp);
    start.seed = 7;
    const ApplyResult early = auth.submit(a, start);
    CHECK(!early.accepted);
    CHECK(std::strcmp(early.error, "need_two_players") == 0);

    ClientSeat b;
    CHECK(auth.join(&b, ARTILLERY_PROTOCOL_VERSION, "pvp").accepted);
    CHECK(b.seat == 1);
    CHECK(auth.view().players == 2);
    CHECK(auth.submit(a, start).accepted);
    CHECK(auth.phase() == Phase::Aiming);
    CHECK(auth.submit(b, ClientIntent{ClientIntent::Fire}).accepted == false);
    CHECK(auth.submit(a, ClientIntent{ClientIntent::Fire}).accepted);
}

static void test_restore_should_copy_match_so_reduce_can_continue()
{
    Match a;
    a.start(9, Mode::Hotseat);
    a.set_angle(50);
    a.set_power(70);
    Match b;
    b.restore(a.view());
    CHECK(b.phase() == Phase::Aiming);
    CHECK(b.angle() == 50);
    CHECK(b.power() == 70);
    CHECK(b.world().height_at(40) == a.world().height_at(40));
    CHECK(b.apply(PlayerId::P0, ClientIntent{ClientIntent::Fire}).accepted);
    CHECK(b.phase() == Phase::Firing);
}

static void test_authority_should_replay_commands_after_a_snapshot_seq()
{
    Authority auth;
    ClientSeat seat;
    CHECK(auth.join(&seat, 0, "bot").accepted);
    ClientIntent start;
    start.kind = ClientIntent::Start;
    start.value = static_cast<int>(Mode::VsBot);
    start.seed = 11;
    CHECK(auth.submit(seat, start).accepted);
    const uint32_t snap_seq = auth.seq();
    const ViewModel snap = auth.view();
    CHECK(auth.submit(seat, ClientIntent{ClientIntent::SetAngle, 55}).accepted);
    CHECK(auth.submit(seat, ClientIntent{ClientIntent::SetPower, 80}).accepted);
    CHECK(auth.submit(seat, ClientIntent{ClientIntent::Fire}).accepted);

    LoggedCommand cmds[Authority::kLogCap];
    const int n = auth.commands_after(snap_seq, cmds, Authority::kLogCap);
    CHECK(n == 3);

    Match replica;
    replica.restore(snap);
    for (int i = 0; i < n; ++i) {
        CHECK(replica.apply(cmds[i].who, cmds[i].intent).accepted);
    }
    CHECK(replica.phase() == Phase::Firing);
    CHECK(replica.angle() == 55);
    CHECK(replica.power() == 80);
}

static void test_pvp_disconnect_should_keep_the_seat_for_the_same_token()
{
    Authority auth;
    ClientSeat a;
    ClientSeat b;
    CHECK(auth.join(&a, ARTILLERY_PROTOCOL_VERSION, "pvp").accepted);
    CHECK(auth.join(&b, ARTILLERY_PROTOCOL_VERSION, "pvp").accepted);
    CHECK(a.seat == 0);
    CHECK(b.seat == 1);
    CHECK(a.token[0] != 0);
    char token[9];
    std::snprintf(token, sizeof(token), "%s", a.token);
    auth.disconnect(&a);
    CHECK(auth.joined_count() == 1);

    ClientSeat again;
    std::snprintf(again.token, sizeof(again.token), "%s", token);
    CHECK(auth.join(&again, ARTILLERY_PROTOCOL_VERSION, "pvp").accepted);
    CHECK(again.seat == 0);
    CHECK(auth.joined_count() == 2);
}

static void test_pvp_stale_token_should_take_a_free_seat_not_server_full()
{
    Authority auth;
    ClientSeat a;
    ClientSeat b;
    CHECK(auth.join(&a, ARTILLERY_PROTOCOL_VERSION, "pvp").accepted);
    CHECK(auth.join(&b, ARTILLERY_PROTOCOL_VERSION, "pvp").accepted);
    auth.disconnect(&a);
    CHECK(auth.joined_count() == 1);

    // Stale token for a held seat reclaims that seat (TCP layer kicks the old socket).
    ClientSeat reclaim;
    std::snprintf(reclaim.token, sizeof(reclaim.token), "%s", b.token);
    CHECK(auth.join(&reclaim, ARTILLERY_PROTOCOL_VERSION, "pvp").accepted);
    CHECK(reclaim.seat == 1);
    CHECK(auth.joined_count() == 1);

    ClientSeat free_seat;
    CHECK(auth.join(&free_seat, ARTILLERY_PROTOCOL_VERSION, "pvp").accepted);
    CHECK(free_seat.seat == 0);
    CHECK(auth.joined_count() == 2);
}

static void test_pvp_token_reclaim_should_work_while_still_present()
{
    Authority auth;
    ClientSeat a;
    ClientSeat b;
    CHECK(auth.join(&a, ARTILLERY_PROTOCOL_VERSION, "pvp").accepted);
    CHECK(auth.join(&b, ARTILLERY_PROTOCOL_VERSION, "pvp").accepted);
    char token[9];
    std::snprintf(token, sizeof(token), "%s", a.token);

    ClientSeat again;
    std::snprintf(again.token, sizeof(again.token), "%s", token);
    CHECK(auth.join(&again, ARTILLERY_PROTOCOL_VERSION, "pvp").accepted);
    CHECK(again.seat == 0);
    CHECK(auth.joined_count() == 2);
}

static void test_title_ui_should_show_lobby_states_for_remote_pvp()
{
    ViewModel v;
    v.snap.phase = Phase::Title;
    stamp_title_ui(v, false, false);
    CHECK(v.title_ui == TitleUi::LocalSelect);

    stamp_title_ui(v, true, false);
    CHECK(v.title_ui == TitleUi::Connecting);

    v.players = 1;
    stamp_title_ui(v, true, true);
    CHECK(v.title_ui == TitleUi::Waiting);

    v.players = 2;
    stamp_title_ui(v, true, true);
    CHECK(v.title_ui == TitleUi::Starting);

    v.snap.phase = Phase::Aiming;
    stamp_title_ui(v, true, true);
    CHECK(v.title_ui == TitleUi::LocalSelect);
}

static void test_cmd_json_should_parse_even_when_type_value_is_cmd()
{
    ClientIntent intent;
    intent.kind = ClientIntent::SetAngle;
    intent.value = 60;
    char wire[256];
    write_cmd_json(wire, sizeof(wire), 2, PlayerId::P0, intent);
    CHECK(std::strstr(wire, "\"type\":\"cmd\"") != nullptr);

    uint32_t seq = 0;
    PlayerId who = PlayerId::P1;
    ClientIntent out{};
    CHECK(parse_cmd_json(wire, &seq, &who, &out));
    CHECK(seq == 2);
    CHECK(who == PlayerId::P0);
    CHECK(out.kind == ClientIntent::SetAngle);
    CHECK(out.value == 60);
}

static void test_state_json_should_roundtrip_have_winner()
{
    Match m;
    m.start(7, Mode::Hotseat);
    ViewModel v = m.view();
    v.snap.phase = Phase::GameOver;
    v.snap.winner = PlayerId::P1;
    v.snap.have_winner = true;
    v.snap.hp[0] = 40;
    v.snap.hp[1] = 0;
    char wire[16384];
    CHECK(write_state_json(wire, sizeof(wire), v, false) > 0);
    CHECK(std::strstr(wire, "\"have_winner\":true") != nullptr);

    ViewModel got{};
    CHECK(parse_state_json(wire, &got));
    CHECK(got.snap.phase == Phase::GameOver);
    CHECK(got.snap.have_winner);
    CHECK(got.snap.winner == PlayerId::P1);
    CHECK(got.snap.hp[1] == 0);
}

static void test_present_should_full_flush_once_then_overlay_when_idle()
{
    Match m;
    m.start(42, Mode::Hotseat);
    std::vector<uint16_t> frame(kPixelCount);
    std::vector<uint16_t> scene(kPixelCount);
    DirtyList dirty;
    Rect prev{};
    DebugOverlay ov{};
    present_frame(frame.data(), scene.data(), m, dirty, prev, ov);
    CHECK(dirty.count == 1);
    CHECK(dirty.pixels() == static_cast<unsigned>(kPixelCount));
    CHECK(!m.scene_dirty());

    present_frame(frame.data(), scene.data(), m, dirty, prev, ov);
    CHECK(dirty.pixels() < 8000);
    CHECK(dirty.pixels() == static_cast<unsigned>(kDebugOverlayRect.w * kDebugOverlayRect.h));
}

static void test_present_should_dirty_hud_and_tanks_when_aim_changes()
{
    Match m;
    m.start(42, Mode::Hotseat);
    std::vector<uint16_t> frame(kPixelCount);
    std::vector<uint16_t> scene(kPixelCount);
    DirtyList dirty;
    Rect prev{};
    DebugOverlay ov{};
    present_frame(frame.data(), scene.data(), m, dirty, prev, ov);
    m.set_angle(62);
    CHECK(m.hud_dirty());
    present_frame(frame.data(), scene.data(), m, dirty, prev, ov);
    CHECK(!m.hud_dirty());
    CHECK(dirty.pixels() > 8000);
    CHECK(dirty.pixels() < static_cast<unsigned>(kPixelCount));
}

int main()
{
    test_world_should_be_deterministic_for_a_seed();
    test_world_should_place_tanks_on_opposite_slopes();
    test_crater_should_lower_the_heightmap();
    test_flat_shot_should_hit_terrain_or_leave_the_map();
    test_wind_should_push_the_shell_along_x();
    test_sim_dt_should_be_seventy_percent_of_wall_time();
    test_match_should_enter_aiming_after_start();
    test_fire_should_move_match_into_firing_then_resolve();
    test_match_should_restore_each_players_last_aim();
    test_bot_should_return_a_legal_shot();
    test_bot_should_usually_miss_the_player();
    test_server_should_reject_fire_when_it_is_not_your_turn();
    test_server_should_reject_aiming_from_the_waiting_player();
    test_local_authority_should_reject_fire_before_start();
    test_local_authority_should_simulate_a_legal_shot();
    test_pvp_authority_should_need_two_players_to_start();
    test_restore_should_copy_match_so_reduce_can_continue();
    test_authority_should_replay_commands_after_a_snapshot_seq();
    test_pvp_disconnect_should_keep_the_seat_for_the_same_token();
    test_pvp_stale_token_should_take_a_free_seat_not_server_full();
    test_pvp_token_reclaim_should_work_while_still_present();
    test_title_ui_should_show_lobby_states_for_remote_pvp();
    test_cmd_json_should_parse_even_when_type_value_is_cmd();
    test_state_json_should_roundtrip_have_winner();
    test_present_should_full_flush_once_then_overlay_when_idle();
    test_present_should_dirty_hud_and_tanks_when_aim_changes();

    if (g_fails) {
        std::fprintf(stderr, "%d checks failed\n", g_fails);
        return 1;
    }
    std::puts("ok");
    return 0;
}
