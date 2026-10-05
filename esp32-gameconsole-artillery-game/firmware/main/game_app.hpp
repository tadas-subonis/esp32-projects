#pragma once

#include "artillery/match.hpp"
#include "artillery/net_input.hpp"
#include "device_hal.hpp"
#include "net_client.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace tc {

class GameApp {
public:
    void init(DeviceHal* hal);
    void tick(uint32_t dt_ms);
    void lock();
    void unlock();
    void present();
    bool submit(const artillery::ClientIntent& intent);
    bool online() const { return net_.alive(); }
    int seat() const { return net_.seat(); }
    const char* net_status() const { return net_.status(); }
    const char* ip() const { return net_.ip(); }
    artillery::ViewModel view() const;
    artillery::Match& match() { return match_; }
    const artillery::Match& match() const { return match_; }
    const uint16_t* frame() const { return frame_; }
    int fps() const { return debug_.fps; }
    unsigned compose_ms() const { return compose_us_ / 1000; }
    unsigned flush_ms() const { return flush_us_ / 1000; }
    unsigned frame_ms() const { return (compose_us_ + flush_us_) / 1000; }
    unsigned dirty_px() const { return dirty_px_; }

private:
    void present_match();
    void present_view(const artillery::ViewModel& view, bool scene_dirty, bool hud_dirty);
    void submit_buttons(const artillery::ButtonEdges& edges, const artillery::ViewModel& view,
                        bool remote_pvp, uint32_t dt_ms);
    void stamp_lobby(artillery::ViewModel* view) const;
    void maybe_auto_start(uint32_t dt_ms, const artillery::ViewModel& view);
    static bool scene_changed(const artillery::ViewModel& a, const artillery::ViewModel& b);
    static bool hud_changed(const artillery::ViewModel& a, const artillery::ViewModel& b);

    void apply_net();

    DeviceHal* hal_ = nullptr;
    artillery::Match match_{};
    NetClient net_{};
    uint32_t replica_seq_ = 0;
    bool have_replica_ = false;
    artillery::ViewModel prev_view_{};
    bool have_prev_view_ = false;
    artillery::Buttons prev_{};
    artillery::Rect prev_proj_{};
    artillery::DebugOverlay debug_{};
    uint32_t start_hold_ms_ = 0;
    artillery::AimSendClock aim_send_{};
    uint32_t hb_ms_ = 0;
    uint32_t compose_us_ = 0;
    uint32_t flush_us_ = 0;
    unsigned dirty_px_ = 0;
    uint16_t* frame_ = nullptr;
    uint16_t* scene_ = nullptr;
    SemaphoreHandle_t mu_ = nullptr;
};

extern GameApp g_app;

}  // namespace tc
