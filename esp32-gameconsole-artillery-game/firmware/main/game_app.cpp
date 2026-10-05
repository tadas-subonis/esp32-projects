#include "game_app.hpp"

#include "artillery/command.hpp"
#include "artillery/input.hpp"
#include "artillery/log.hpp"
#include "artillery/render.hpp"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <cstring>

namespace tc {
namespace {

const char* TAG = "game";

uint16_t* alloc_fb()
{
    uint16_t* p = static_cast<uint16_t*>(
        heap_caps_malloc(artillery::kPixelCount * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (p == nullptr) {
        p = static_cast<uint16_t*>(
            heap_caps_malloc(artillery::kPixelCount * sizeof(uint16_t), MALLOC_CAP_8BIT));
    }
    return p;
}

void match_log_sink(const char* line) { ESP_LOGI("match", "%s", line); }

bool any_edge(const artillery::ButtonEdges& e)
{
    const artillery::Buttons& p = e.pressed;
    const artillery::Buttons& r = e.released;
    return p.up || p.down || p.left || p.right || p.a || p.b || r.up || r.down || r.left || r.right ||
           r.a || r.b;
}

}  // namespace

void GameApp::init(DeviceHal* hal)
{
    hal_ = hal;
    mu_ = xSemaphoreCreateRecursiveMutex();
    frame_ = alloc_fb();
    scene_ = alloc_fb();
    if (frame_ == nullptr || scene_ == nullptr) {
        ESP_LOGE(TAG, "framebuffer alloc failed");
        return;
    }

    const uint32_t seed = static_cast<uint32_t>(esp_timer_get_time() & 0xFFFFFFFFu);
    match_.set_seed(seed == 0 ? 1 : seed);
    artillery::set_log_sink(&match_log_sink);
    match_.reset_title();
    net_.start();
    present();
    ESP_LOGI(TAG, "init seed=%u display=%d fb=%p", static_cast<unsigned>(match_.seed()),
             hal_->display_ready() ? 1 : 0, static_cast<void*>(frame_));
}

void GameApp::lock()
{
    if (mu_) {
        xSemaphoreTakeRecursive(mu_, portMAX_DELAY);
    }
}

void GameApp::unlock()
{
    if (mu_) {
        xSemaphoreGiveRecursive(mu_);
    }
}

void GameApp::apply_net()
{
    if (net_.take_snapshot()) {
        const artillery::ViewModel v = net_.view();
        match_.restore(v);
        replica_seq_ = v.snap.seq;
        have_replica_ = true;
    }

    struct Pending {
        uint32_t seq = 0;
        artillery::PlayerId who = artillery::PlayerId::P0;
        artillery::ClientIntent intent{};
    };
    Pending pending[16];
    int n = 0;
    while (n < 16) {
        uint32_t seq = 0;
        artillery::PlayerId who = artillery::PlayerId::P0;
        artillery::ClientIntent intent{};
        if (!net_.pop_cmd(&seq, &who, &intent)) {
            break;
        }
        pending[n++] = Pending{seq, who, intent};
    }

    // Fold consecutive same-axis nudges so a wire backlog does not crawl the aim after release.
    for (int i = 0; i < n; ++i) {
        auto& e = pending[i];
        if (e.seq <= replica_seq_) {
            continue;
        }
        if (!have_replica_) {
            continue;
        }
        if (e.seq != replica_seq_ + 1) {
            net_.request_sync(replica_seq_);
            break;
        }
        if (e.intent.kind == artillery::ClientIntent::NudgeAngle ||
            e.intent.kind == artillery::ClientIntent::NudgePower) {
            while (i + 1 < n) {
                const auto& next = pending[i + 1];
                if (next.seq != e.seq + 1 || next.who != e.who || next.intent.kind != e.intent.kind) {
                    break;
                }
                e.intent.value += next.intent.value;
                e.seq = next.seq;
                ++i;
            }
        }
        match_.apply(e.who, e.intent);
        replica_seq_ = e.seq;
    }
}

void GameApp::tick(uint32_t dt_ms)
{
    lock();
    if (frame_ != nullptr && scene_ != nullptr && hal_ != nullptr) {
        const artillery::Buttons now = hal_->read_buttons();
        const artillery::ButtonEdges edges = artillery::make_edges(prev_, now);
        debug_.on_frame(dt_ms, now, edges.pressed);
        if (any_edge(edges)) {
            ESP_LOGI(TAG, "btn U%d D%d L%d R%d A%d B%d click=%s", now.up ? 1 : 0, now.down ? 1 : 0,
                     now.left ? 1 : 0, now.right ? 1 : 0, now.a ? 1 : 0, now.b ? 1 : 0, debug_.btn);
        }
        net_.pump();

        if (net_.enabled()) {
            apply_net();
            if (match_.phase() == artillery::Phase::Firing) {
                match_.step_fx(dt_ms);
            }
            artillery::ViewModel v = match_.view();
            stamp_lobby(&v);
            if (v.snap.phase == artillery::Phase::Title && edges.pressed.b) {
                ESP_LOGI(TAG, "cancel online — back to local title");
                net_.disable();
                have_replica_ = false;
                replica_seq_ = 0;
                match_.reset_title();
            } else {
                submit_buttons(edges, v, net_.enabled(), dt_ms);
                maybe_auto_start(dt_ms, v);
            }
            present_match();
        } else if (match_.phase() == artillery::Phase::Title && edges.pressed.a &&
                   match_.title_sel() == 1) {
            // HOTSEAT menu item = join online PvP (Wi-Fi stays off until this).
            if (net_.configured()) {
                ESP_LOGI(TAG, "HOTSEAT selected — enabling online");
                net_.enable();
            } else {
                ESP_LOGW(TAG, "HOTSEAT needs WIFI_SSID in .local.env");
            }
            present_match();
        } else {
            match_.handle_buttons(edges);
            match_.tick(dt_ms);
            present_match();
        }
        prev_ = now;
        hb_ms_ += dt_ms;
        if (hb_ms_ >= 5000) {
            hb_ms_ = 0;
            const artillery::ViewModel hv = match_.view();
            ESP_LOGI(TAG,
                     "hb net=%s seat=%d ip=%s seq=%lu phase=%s players=%d turn=%d active=%s fps=%d frame=%ums heap=%u",
                     net_.status(), net_.seat(), net_.ip(), static_cast<unsigned long>(replica_seq_),
                     artillery::phase_name(hv.snap.phase), hv.players, hv.snap.turn,
                     artillery::player_name(hv.snap.active), debug_.fps, frame_ms(),
                     static_cast<unsigned>(esp_get_free_heap_size()));
        }
    }
    unlock();
}

void GameApp::present()
{
    present_match();
}

artillery::ViewModel GameApp::view() const
{
    artillery::ViewModel v = match_.view();
    stamp_lobby(&v);
    return v;
}

void GameApp::stamp_lobby(artillery::ViewModel* view) const
{
    if (view == nullptr) {
        return;
    }
    if (net_.enabled() && net_.rejected()) {
        view->title_ui = artillery::TitleUi::Rejected;
        return;
    }
    artillery::stamp_title_ui(*view, net_.enabled(), net_.alive());
}

void GameApp::maybe_auto_start(uint32_t dt_ms, const artillery::ViewModel& view)
{
    using artillery::ClientIntent;
    using artillery::Mode;
    using artillery::TitleUi;

    if (view.title_ui != TitleUi::Starting || !net_.alive()) {
        start_hold_ms_ = 0;
        return;
    }
    start_hold_ms_ += dt_ms;
    if (start_hold_ms_ < 700) {
        return;
    }
    start_hold_ms_ = 0;
    ClientIntent start;
    start.kind = ClientIntent::Start;
    start.value = static_cast<int>(Mode::Pvp);
    if (submit(start)) {
        ESP_LOGI(TAG, "pvp auto-start players=%d", view.players);
    }
}

bool GameApp::scene_changed(const artillery::ViewModel& a, const artillery::ViewModel& b)
{
    if (a.snap.seed != b.snap.seed || a.title_sel != b.title_sel || a.title_ui != b.title_ui ||
        a.players != b.players) {
        return true;
    }
    const bool a_title = a.snap.phase == artillery::Phase::Title;
    const bool b_title = b.snap.phase == artillery::Phase::Title;
    if (a_title != b_title) {
        return true;
    }
    if (a.snap.phase == artillery::Phase::GameOver || b.snap.phase == artillery::Phase::GameOver) {
        if (a.snap.phase != b.snap.phase) {
            return true;
        }
    }
    return std::memcmp(a.heights.data(), b.heights.data(), sizeof(a.heights)) != 0;
}

bool GameApp::hud_changed(const artillery::ViewModel& a, const artillery::ViewModel& b)
{
    return a.snap.angle != b.snap.angle || a.snap.power != b.snap.power || a.snap.wind != b.snap.wind ||
           a.snap.active != b.snap.active || a.snap.hp[0] != b.snap.hp[0] || a.snap.hp[1] != b.snap.hp[1] ||
           a.tank_angle[0] != b.tank_angle[0] || a.tank_angle[1] != b.tank_angle[1] ||
           a.snap.tank_x[0] != b.snap.tank_x[0] || a.snap.tank_x[1] != b.snap.tank_x[1];
}

void GameApp::submit_buttons(const artillery::ButtonEdges& edges, const artillery::ViewModel& view,
                             bool remote_pvp, uint32_t dt_ms)
{
    using artillery::ClientIntent;
    using artillery::Mode;
    using artillery::Phase;
    using artillery::PlayerId;

    if (view.snap.phase == Phase::Title) {
        aim_send_.reset();
        if (remote_pvp) {
            if (edges.pressed.a && view.players >= 2) {
                ClientIntent start;
                start.kind = ClientIntent::Start;
                start.value = static_cast<int>(Mode::Pvp);
                submit(start);
            }
            return;
        }
        if (edges.pressed.left || edges.pressed.right || edges.pressed.up || edges.pressed.down ||
            edges.pressed.b) {
            submit(ClientIntent{ClientIntent::ToggleSelect});
        }
        if (edges.pressed.a) {
            // title_sel 1 (HOTSEAT) is handled in tick() as online opt-in.
            if (view.title_sel != 0) {
                return;
            }
            ClientIntent start;
            start.kind = ClientIntent::Start;
            start.value = static_cast<int>(Mode::VsBot);
            submit(start);
        }
        return;
    }

    if (view.snap.phase == Phase::GameOver) {
        aim_send_.reset();
        if (edges.pressed.a) {
            submit(ClientIntent{ClientIntent::Rematch});
        }
        if (edges.pressed.b) {
            if (net_.enabled()) {
                net_.disable();
                have_replica_ = false;
                replica_seq_ = 0;
            }
            submit(ClientIntent{ClientIntent::ToTitle});
        }
        return;
    }

    if (view.snap.phase != Phase::Aiming) {
        aim_send_.reset();
        return;
    }

    // PvP: only the active seat may aim/fire — never queue inputs for a later turn.
    if (remote_pvp) {
        const int seat = net_.seat();
        if (seat < 0 || static_cast<int>(view.snap.active) != seat) {
            aim_send_.reset();
            return;
        }
    }

    const bool holding =
        edges.down.left || edges.down.right || edges.down.up || edges.down.down;
    const bool edge =
        edges.pressed.left || edges.pressed.right || edges.pressed.up || edges.pressed.down;
    const int step = aim_send_.poll(holding, edge, dt_ms);
    if (step > 0) {
        const int dir_left = view.snap.active == PlayerId::P0 ? step : -step;
        if (edges.down.left) {
            submit(ClientIntent{ClientIntent::NudgeAngle, dir_left});
        }
        if (edges.down.right) {
            submit(ClientIntent{ClientIntent::NudgeAngle, -dir_left});
        }
        if (edges.down.up) {
            submit(ClientIntent{ClientIntent::NudgePower, step});
        }
        if (edges.down.down) {
            submit(ClientIntent{ClientIntent::NudgePower, -step});
        }
    }
    if (edges.pressed.a) {
        submit(ClientIntent{ClientIntent::Fire});
    }
}

bool GameApp::submit(const artillery::ClientIntent& intent)
{
    if (net_.enabled()) {
        if (!net_.alive() || !net_.submit(intent)) {
            return false;
        }
        // Predict own aim locally; Fire/Start wait for authoritative Cmd/State.
        if (intent.kind == artillery::ClientIntent::NudgeAngle ||
            intent.kind == artillery::ClientIntent::NudgePower ||
            intent.kind == artillery::ClientIntent::SetAngle ||
            intent.kind == artillery::ClientIntent::SetPower) {
            const int seat = net_.seat();
            const artillery::PlayerId who =
                seat < 0 ? match_.active() : static_cast<artillery::PlayerId>(seat);
            match_.apply(who, intent);
        }
        return true;
    }
    const artillery::PlayerId who =
        intent.kind == artillery::ClientIntent::Start || intent.kind == artillery::ClientIntent::ToggleSelect ||
                intent.kind == artillery::ClientIntent::Rematch ||
                intent.kind == artillery::ClientIntent::ToTitle
            ? artillery::PlayerId::P0
            : match_.active();
    return match_.apply(who, intent).accepted;
}

void GameApp::present_match()
{
    using namespace artillery;
    if (frame_ == nullptr || scene_ == nullptr || hal_ == nullptr) {
        return;
    }
    ViewModel v = match_.view();
    stamp_lobby(&v);
    bool scene = match_.scene_dirty();
    bool hud = match_.hud_dirty();
    if (!have_prev_view_ || scene_changed(prev_view_, v)) {
        scene = true;
    } else if (hud_changed(prev_view_, v)) {
        hud = true;
    }
    present_view(v, scene, hud);
    if (scene) {
        match_.clear_scene_dirty();
        match_.clear_hud_dirty();
    } else if (hud) {
        match_.clear_hud_dirty();
    }
    prev_view_ = v;
    have_prev_view_ = true;
}

void GameApp::present_view(const artillery::ViewModel& view, bool scene_dirty, bool hud_dirty)
{
    using namespace artillery;
    if (frame_ == nullptr || scene_ == nullptr || hal_ == nullptr) {
        return;
    }
    const int64_t t0 = esp_timer_get_time();
    DirtyList dirty;
    present_frame(frame_, scene_, view, scene_dirty, hud_dirty, dirty, prev_proj_, debug_);
    const int64_t t1 = esp_timer_get_time();
    dirty_px_ = dirty.pixels();
    hal_->flush(frame_, dirty);
    const int64_t t2 = esp_timer_get_time();
    compose_us_ = static_cast<uint32_t>(t1 - t0);
    flush_us_ = static_cast<uint32_t>(t2 - t1);
}

}  // namespace tc
