#pragma once

#include "artillery/match.hpp"

#include <cstdint>

namespace artillery {

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct DirtyList {
    Rect rects[12]{};
    int count = 0;

    void clear() { count = 0; }
    void add(Rect r);
    unsigned pixels() const;
    void mark_all()
    {
        count = 1;
        rects[0] = Rect{0, 0, kWidth, kHeight};
    }
};

inline uint16_t fb_get(const uint16_t* fb, int x, int y)
{
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) {
        return 0;
    }
    return fb[y * kWidth + x];
}

inline void fb_set(uint16_t* fb, int x, int y, uint16_t c)
{
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) {
        return;
    }
    fb[y * kWidth + x] = c;
}

struct DebugOverlay {
    int fps = 0;
    char btn[16] = "--";
    int sticky_ms = 0;
    void on_frame(uint32_t dt_ms, const Buttons& down, const Buttons& pressed);
};

inline constexpr Rect kDebugOverlayRect{6, 288, 152, 30};

void compose_scene(uint16_t* dst, const ViewModel& view);
void compose_scene(uint16_t* dst, const Match& match);
void blit_rect(uint16_t* dst, const uint16_t* src, Rect r);
void draw_projectile(uint16_t* dst, const ViewModel& view);
void draw_projectile(uint16_t* dst, const Match& match);
void draw_hud_overlay(uint16_t* dst, const ViewModel& view);
void draw_hud_overlay(uint16_t* dst, const Match& match);
void draw_debug_overlay(uint16_t* dst, const DebugOverlay& overlay);
void present_frame(uint16_t* frame, uint16_t* scene, const ViewModel& view, bool scene_dirty,
                   bool hud_dirty, DirtyList& dirty, Rect& prev_proj, const DebugOverlay& overlay);
void present_frame(uint16_t* frame, uint16_t* scene, Match& match, DirtyList& dirty, Rect& prev_proj,
                   const DebugOverlay& overlay);

}  // namespace artillery
