#include "artillery/render.hpp"
#include "artillery/sprites_data.hpp"

#include <cmath>
#include <cstring>

namespace artillery {
namespace {

constexpr float kPi = 3.14159265358979323846f;

void fill_rect(uint16_t* dst, int x, int y, int w, int h, uint16_t c)
{
    const int x0 = clamp_int(x, 0, kWidth);
    const int y0 = clamp_int(y, 0, kHeight);
    const int x1 = clamp_int(x + w, 0, kWidth);
    const int y1 = clamp_int(y + h, 0, kHeight);
    for (int py = y0; py < y1; ++py) {
        uint16_t* row = dst + py * kWidth + x0;
        for (int px = x0; px < x1; ++px) {
            *row++ = c;
        }
    }
}

void format_int(char* buf, int n);

// 3x5 glyphs: 0-9 then A-Z. Bit2 is the leftmost pixel.
const uint8_t kGlyph3x5[][5] = {
    {0x7, 0x5, 0x5, 0x5, 0x7},  // 0
    {0x2, 0x6, 0x2, 0x2, 0x7},  // 1
    {0x7, 0x1, 0x7, 0x4, 0x7},  // 2
    {0x7, 0x1, 0x7, 0x1, 0x7},  // 3
    {0x5, 0x5, 0x7, 0x1, 0x1},  // 4
    {0x7, 0x4, 0x7, 0x1, 0x7},  // 5
    {0x7, 0x4, 0x7, 0x5, 0x7},  // 6
    {0x7, 0x1, 0x1, 0x1, 0x1},  // 7
    {0x7, 0x5, 0x7, 0x5, 0x7},  // 8
    {0x7, 0x5, 0x7, 0x1, 0x7},  // 9
    {0x2, 0x5, 0x7, 0x5, 0x5},  // A
    {0x6, 0x5, 0x6, 0x5, 0x6},  // B
    {0x7, 0x4, 0x4, 0x4, 0x7},  // C
    {0x6, 0x5, 0x5, 0x5, 0x6},  // D
    {0x7, 0x4, 0x6, 0x4, 0x7},  // E
    {0x7, 0x4, 0x6, 0x4, 0x4},  // F
    {0x7, 0x4, 0x5, 0x5, 0x7},  // G
    {0x5, 0x5, 0x7, 0x5, 0x5},  // H
    {0x7, 0x2, 0x2, 0x2, 0x7},  // I
    {0x7, 0x2, 0x2, 0x2, 0x6},  // J
    {0x5, 0x5, 0x6, 0x5, 0x5},  // K
    {0x4, 0x4, 0x4, 0x4, 0x7},  // L
    {0x5, 0x7, 0x5, 0x5, 0x5},  // M
    {0x6, 0x5, 0x5, 0x5, 0x5},  // N
    {0x7, 0x5, 0x5, 0x5, 0x7},  // O
    {0x7, 0x5, 0x7, 0x4, 0x4},  // P
    {0x7, 0x5, 0x5, 0x7, 0x1},  // Q
    {0x7, 0x5, 0x6, 0x5, 0x5},  // R
    {0x7, 0x4, 0x7, 0x1, 0x7},  // S
    {0x7, 0x2, 0x2, 0x2, 0x2},  // T
    {0x5, 0x5, 0x5, 0x5, 0x7},  // U
    {0x5, 0x5, 0x5, 0x5, 0x2},  // V
    {0x5, 0x5, 0x5, 0x7, 0x5},  // W
    {0x5, 0x5, 0x2, 0x5, 0x5},  // X
    {0x5, 0x5, 0x2, 0x2, 0x2},  // Y
    {0x7, 0x1, 0x2, 0x4, 0x7},  // Z
};

int glyph_index(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'Z') {
        return 10 + (c - 'A');
    }
    if (c >= 'a' && c <= 'z') {
        return 10 + (c - 'a');
    }
    return -1;
}

int text_width(const char* s, int scale)
{
    int n = 0;
    for (const char* p = s; *p; ++p) {
        ++n;
    }
    if (n == 0 || scale < 1) {
        return 0;
    }
    return n * 4 * scale - scale;
}

void draw_glyph(uint16_t* dst, int x, int y, char c, uint16_t color, int scale)
{
    const int gi = glyph_index(c);
    if (gi < 0 || scale < 1) {
        return;
    }
    for (int row = 0; row < 5; ++row) {
        const uint8_t bits = kGlyph3x5[gi][row];
        for (int col = 0; col < 3; ++col) {
            if (bits & (1u << (2 - col))) {
                fill_rect(dst, x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

void draw_text(uint16_t* dst, int x, int y, const char* s, uint16_t color, int scale = 1)
{
    if (scale < 1) {
        scale = 1;
    }
    int cx = x;
    for (const char* p = s; *p; ++p) {
        if (*p == ' ') {
            cx += 4 * scale;
            continue;
        }
        if (*p == '-') {
            fill_rect(dst, cx, y + 2 * scale, 3 * scale, scale, color);
            cx += 4 * scale;
            continue;
        }
        if (*p == '>') {
            fill_rect(dst, cx, y + 2 * scale, scale, scale, color);
            fill_rect(dst, cx + scale, y + scale, scale, 3 * scale, color);
            fill_rect(dst, cx + 2 * scale, y + 2 * scale, scale, scale, color);
            cx += 4 * scale;
            continue;
        }
        if (*p == '<') {
            fill_rect(dst, cx + 2 * scale, y + 2 * scale, scale, scale, color);
            fill_rect(dst, cx + scale, y + scale, scale, 3 * scale, color);
            fill_rect(dst, cx, y + 2 * scale, scale, scale, color);
            cx += 4 * scale;
            continue;
        }
        draw_glyph(dst, cx, y, *p, color, scale);
        cx += 4 * scale;
    }
}

void draw_text_centered(uint16_t* dst, int y, const char* s, uint16_t color, int scale)
{
    draw_text(dst, (kWidth - text_width(s, scale)) / 2, y, s, color, scale);
}

void draw_label(uint16_t* dst, int x, int y, int w, int h, const char* s, uint16_t color, int scale)
{
    const int tw = text_width(s, scale);
    const int th = 5 * scale;
    draw_text(dst, x + (w - tw) / 2, y + (h - th) / 2, s, color, scale);
}

int unpack_r(uint16_t c) { return (c >> 11) * 8; }
int unpack_g(uint16_t c) { return ((c >> 5) & 0x3F) * 4; }
int unpack_b(uint16_t c) { return (c & 0x1F) * 8; }

uint16_t lerp_565(uint16_t a, uint16_t b, int t /*0..256*/)
{
    const int r = unpack_r(a) + (unpack_r(b) - unpack_r(a)) * t / 256;
    const int g = unpack_g(a) + (unpack_g(b) - unpack_g(a)) * t / 256;
    const int bl = unpack_b(a) + (unpack_b(b) - unpack_b(a)) * t / 256;
    return rgb565(r, g, bl);
}

void blit_pixel_sprite(uint16_t* dst, int x, int y, const PixelSprite& spr, bool flip)
{
    for (int row = 0; row < spr.h; ++row) {
        for (int col = 0; col < spr.w; ++col) {
            const int i = row * spr.w + col;
            if (!sprite_opaque(spr.mask, i)) {
                continue;
            }
            const int dx = flip ? (spr.w - 1 - col) : col;
            fb_set(dst, x + dx, y + row, spr.rgb[i]);
        }
    }
}

void draw_chevron(uint16_t* dst, int x, int y, int dir, uint16_t c, int scale)
{
    // dir +1 = right, -1 = left. 7x11 glyph at scale 1.
    for (int row = 0; row < 11; ++row) {
        const int dist = row < 6 ? row : 10 - row;
        const int ox = dir > 0 ? dist : (4 - dist);
        fill_rect(dst, x + ox * scale, y + row * scale, 3 * scale, scale, c);
    }
}

void draw_wind_meter(uint16_t* dst, int cx, int cy, int wind, int chev_scale, int num_scale)
{
    const int mag = wind < 0 ? -wind : wind;
    int n = 0;
    if (mag > 0) {
        n = 1;
    }
    if (mag >= 4) {
        n = 2;
    }
    if (mag >= 7) {
        n = 3;
    }
    if (mag >= 10) {
        n = 4;
    }
    const int chev_w = 8 * chev_scale;
    const int chev_h = 11 * chev_scale;
    if (n == 0) {
        draw_text(dst, cx - text_width("WIND 0", num_scale) / 2, cy + (chev_h - 5 * num_scale) / 2,
                  "WIND 0", kColorMuted, num_scale);
        return;
    }
    char num[8];
    format_int(num, mag);
    const int num_w = text_width(num, num_scale);
    const int gap = 6;
    const int total = n * chev_w + gap + num_w;
    int x = cx - total / 2;
    const uint16_t col = kColorWind;
    const int num_y = cy + (chev_h - 5 * num_scale) / 2;
    if (wind < 0) {
        for (int i = 0; i < n; ++i) {
            draw_chevron(dst, x, cy, -1, col, chev_scale);
            x += chev_w;
        }
        x += gap - 2;
        draw_text(dst, x, num_y, num, kColorText, num_scale);
    } else {
        draw_text(dst, x, num_y, num, kColorText, num_scale);
        x += num_w + gap;
        for (int i = 0; i < n; ++i) {
            draw_chevron(dst, x, cy, 1, col, chev_scale);
            x += chev_w;
        }
    }
}

void draw_windsock(uint16_t* dst, const ViewModel& view)
{
    const int x = kWidth / 2;
    const int ground = view.heights[static_cast<size_t>(x)];
    const int base = ground - 1;
    const int top = base - 28;
    fill_rect(dst, x, top, 2, base - top, rgb565(90, 72, 48));
    fill_rect(dst, x - 3, top, 8, 2, kColorMuted);
    const int wind = view.snap.wind;
    const int dir = wind < 0 ? -1 : 1;
    const int mag = wind < 0 ? -wind : wind;
    const int segs = mag == 0 ? 1 : (1 + mag / 4);
    for (int i = 0; i < segs && i < 4; ++i) {
        const int yy = top + 4 + i * 6;
        const int len = 10 + i * 3 + mag;
        const int x0 = dir > 0 ? x + 2 : x - len;
        const uint16_t c = (i & 1) ? kColorWind : kColorP0;
        fill_rect(dst, x0, yy, len, 4, c);
        if (dir > 0) {
            fb_set(dst, x0 + len, yy + 1, c);
            fb_set(dst, x0 + len + 1, yy + 2, c);
        } else {
            fb_set(dst, x0 - 1, yy + 1, c);
            fb_set(dst, x0 - 2, yy + 2, c);
        }
    }
}

void draw_sun(uint16_t* dst, int cx, int cy)
{
    blit_pixel_sprite(dst, cx - kSpr_sun.w / 2, cy - kSpr_sun.h / 2, kSpr_sun, false);
}

void draw_clouds(uint16_t* dst, uint32_t seed)
{
    // Keep silhouettes clear of the centered wind panel and the sun.
    const int xs[3] = {24 + static_cast<int>(seed % 44),
                       102 + static_cast<int>((seed / 7) % 28),
                       310 + static_cast<int>((seed / 13) % 28)};
    const int ys[3] = {kPlayTop + 18, kPlayTop + 72, kPlayTop + 82};
    for (int i = 0; i < 3; ++i) {
        blit_pixel_sprite(dst, xs[i], ys[i], kSpr_cloud, i == 1);
    }
}

void draw_sky(uint16_t* dst)
{
    for (int y = kPlayTop; y < kHeight; ++y) {
        const int t = (y - kPlayTop) * 256 / kPlayHeight;
        const uint16_t sky = lerp_565(kColorSkyHi, kColorSkyLo, t);
        uint16_t* row = dst + y * kWidth;
        for (int x = 0; x < kWidth; ++x) {
            row[x] = sky;
        }
    }
}

void draw_grass_tufts(uint16_t* dst, const ViewModel& view)
{
    const uint32_t seed = view.snap.seed;
    for (int i = 0; i < 12; ++i) {
        const int x = 20 + static_cast<int>((seed * 13u + static_cast<uint32_t>(i) * 89u) %
                                            static_cast<uint32_t>(kWidth - 40));
        if (x < kTankInset + 20 || x > kWidth - kTankInset - 20) {
            continue;
        }
        const int ground = view.heights[static_cast<size_t>(x)];
        blit_pixel_sprite(dst, x - kSpr_grass.w / 2, ground - kSpr_grass.h + 2, kSpr_grass, (i & 1) != 0);
    }
}

uint32_t terrain_noise(int x, int y, uint32_t seed)
{
    uint32_t n = static_cast<uint32_t>(x) * 0x1F123BB5u;
    n ^= static_cast<uint32_t>(y) * 0x5F356495u;
    n ^= seed * 0x9E3779B9u;
    n ^= n >> 15;
    n *= 0x85EBCA6Bu;
    return n ^ (n >> 13);
}

void draw_ground(uint16_t* dst, const ViewModel& view)
{
    const uint32_t seed = view.snap.seed;
    for (int x = 0; x < kWidth; ++x) {
        const int ground = view.heights[static_cast<size_t>(x)];
        // Paint solid layers first so texture can never expose stale sky pixels.
        fill_rect(dst, x, ground, 1, kHeight - ground, kColorDirtDark);
        fill_rect(dst, x, ground, 1, 1, kColorGrass);
        fill_rect(dst, x, ground + 1, 1, 4, kColorGround);
        fill_rect(dst, x, ground + 5, 1, 17, kColorDirt);
        for (int y = ground + 1; y < kHeight; ++y) {
            const int depth = y - ground;
            const uint32_t noise = terrain_noise(x, y, seed);
            if (depth < 5 && (noise & 7u) == 0) {
                fb_set(dst, x, y, kColorGroundDark);
            } else if (depth < 22 && (noise & 31u) == 0) {
                fb_set(dst, x, y, kColorStone);
            } else if (depth >= 22 && (noise & 15u) == 0) {
                fb_set(dst, x, y, kColorDirt);
            }
        }
        if ((terrain_noise(x, ground, seed) & 15u) == 0 && ground > kPlayTop + 2) {
            fb_set(dst, x, ground - 1, kColorGroundDark);
            if ((x & 1) == 0) {
                fb_set(dst, x, ground - 2, kColorGrass);
            }
        }
    }
}

void draw_tank(uint16_t* dst, const Tank& tank, PlayerId id, int angle_deg, bool active)
{
    if (!tank.alive && tank.hp <= 0) {
        return;
    }
    const int x = static_cast<int>(tank.x + 0.5f);
    const int y = static_cast<int>(tank.y + 0.5f);
    const PixelSprite& spr = id == PlayerId::P0 ? kSpr_tank_p0 : kSpr_tank_p1;
    const uint16_t body = id == PlayerId::P0 ? kColorP0 : kColorP1;
    const uint16_t outline = id == PlayerId::P0 ? kColorP0Dk : kColorP1Dk;
    const bool flip = id == PlayerId::P1;
    blit_pixel_sprite(dst, x - spr.w / 2, y - spr.h + 1, spr, flip);

    const float local = id == PlayerId::P0 ? static_cast<float>(angle_deg)
                                           : static_cast<float>(180 - angle_deg);
    const float rad = local * kPi / 180.0f;
    const int bx = x + (flip ? -4 : 4);
    const int by = y - spr.h + 8;
    const int x1 = bx + static_cast<int>(std::cos(rad) * kBarrelLen);
    const int y1 = by - static_cast<int>(std::sin(rad) * kBarrelLen);
    const int steps = kBarrelLen + 2;
    for (int i = 0; i <= steps; ++i) {
        const int px = bx + (x1 - bx) * i / steps;
        const int py = by + (y1 - by) * i / steps;
        fb_set(dst, px, py, outline);
        fb_set(dst, px, py - 1, active ? kColorText : body);
        fb_set(dst, px, py + 1, body);
        fb_set(dst, px - 1, py, body);
    }
}

void draw_title_hills(uint16_t* dst)
{
    for (int x = 0; x < kWidth; ++x) {
        const int h = 248 + static_cast<int>(12.0f * std::sin(x * 0.03f) + 8.0f * std::sin(x * 0.07f));
        for (int y = h; y < kHeight; ++y) {
            const int depth = y - h;
            const uint16_t c = depth < 4 ? kColorGrass : (depth < 20 ? kColorDirt : kColorDirtDark);
            fb_set(dst, x, y, c);
        }
    }
}

void draw_title(uint16_t* dst, const ViewModel& view)
{
    for (int y = 0; y < kHeight; ++y) {
        const uint16_t sky = lerp_565(kColorSkyHi, kColorSkyLo, y * 256 / kHeight);
        uint16_t* row = dst + y * kWidth;
        for (int x = 0; x < kWidth; ++x) {
            row[x] = sky;
        }
    }
    draw_sun(dst, 400, 48);
    draw_clouds(dst, 42);
    draw_title_hills(dst);
    blit_pixel_sprite(dst, 90, 236, kSpr_grass, false);
    blit_pixel_sprite(dst, 210, 240, kSpr_grass, true);
    blit_pixel_sprite(dst, 380, 234, kSpr_grass, false);

    Tank left{};
    left.x = 148;
    left.y = 248;
    left.alive = true;
    Tank right{};
    right.x = 332;
    right.y = 246;
    right.alive = true;
    draw_tank(dst, left, PlayerId::P0, 48, true);
    draw_tank(dst, right, PlayerId::P1, 52, false);

    fill_rect(dst, 62, 70, 356, 60, kColorHud);
    fill_rect(dst, 62, 70, 356, 3, kColorHudHi);
    // 3x5 glyphs at scale 5 → 25px tall; center in the 60px banner (y=70..130).
    constexpr int kTitleScale = 5;
    constexpr int kTitleBannerY = 70;
    constexpr int kTitleBannerH = 60;
    const int title_y = kTitleBannerY + (kTitleBannerH - 5 * kTitleScale) / 2;
    draw_text_centered(dst, title_y + 2, "TANK DUEL", kColorP0Dk, kTitleScale);
    draw_text_centered(dst, title_y, "TANK DUEL", kColorTitle, kTitleScale);

    if (view.title_ui != TitleUi::LocalSelect) {
        fill_rect(dst, 50, 148, 380, 80, kColorHud);
        fill_rect(dst, 50, 148, 380, 3, kColorTitle);
        const char* headline = "CONNECTING";
        const char* sub = "JOINING SERVER";
        if (view.title_ui == TitleUi::Rejected) {
            headline = "SERVER FULL";
            sub = "TRY AGAIN LATER";
        } else if (view.title_ui == TitleUi::Waiting) {
            headline = "WAITING FOR OPPONENT";
            sub = view.players <= 0 ? "HOLDING SEAT" : "1 OF 2";
        } else if (view.title_ui == TitleUi::Starting) {
            headline = "STARTING GAME";
            sub = "BOTH READY";
        }
        draw_text_centered(dst, 164, headline, kColorTitle, 2);
        draw_text_centered(dst, 192, sub, kColorMuted, 2);
        draw_text_centered(dst, 276, "B CANCEL", kColorMuted, 2);
        return;
    }

    const bool bot = view.title_sel == 0;
    fill_rect(dst, 70, 160, 160, 44, bot ? kColorP0 : kColorHud);
    fill_rect(dst, 250, 160, 160, 44, !bot ? kColorP1 : kColorHud);
    fill_rect(dst, 70, 160, 160, 3, bot ? kColorTitle : kColorHudHi);
    fill_rect(dst, 250, 160, 160, 3, !bot ? kColorTitle : kColorHudHi);
    draw_label(dst, 70, 160, 160, 44, "VS BOT", kColorText, 2);
    draw_label(dst, 250, 160, 160, 44, "HOTSEAT", kColorText, 2);

    draw_text_centered(dst, 276, "A START    ARROWS SELECT", kColorMuted, 2);
}

void draw_world_playfield(uint16_t* dst, const ViewModel& view)
{
    fill_rect(dst, 0, 0, kWidth, kHudHeight, kColorHud);
    draw_sky(dst);
    draw_sun(dst, 418, kPlayTop + 32);
    draw_clouds(dst, view.snap.seed);
    draw_ground(dst, view);
    draw_grass_tufts(dst, view);
    draw_windsock(dst, view);

    fill_rect(dst, 148, kPlayTop + 4, 184, 36, kColorHud);
    fill_rect(dst, 148, kPlayTop + 4, 184, 3, kColorHudHi);
    draw_wind_meter(dst, kWidth / 2, kPlayTop + 8, view.snap.wind, 2, 3);

    draw_tank(dst, view.tanks[0], PlayerId::P0, view.tank_angle[0], view.snap.active == PlayerId::P0);
    draw_tank(dst, view.tanks[1], PlayerId::P1, view.tank_angle[1], view.snap.active == PlayerId::P1);
}

void format_int(char* buf, int n)
{
    if (n < 0) {
        buf[0] = '-';
        format_int(buf + 1, -n);
        return;
    }
    if (n >= 100) {
        buf[0] = static_cast<char>('0' + n / 100);
        buf[1] = static_cast<char>('0' + (n / 10) % 10);
        buf[2] = static_cast<char>('0' + n % 10);
        buf[3] = 0;
        return;
    }
    if (n >= 10) {
        buf[0] = static_cast<char>('0' + n / 10);
        buf[1] = static_cast<char>('0' + n % 10);
        buf[2] = 0;
        return;
    }
    buf[0] = static_cast<char>('0' + n);
    buf[1] = 0;
}

}  // namespace

void DirtyList::add(Rect r)
{
    if (r.w <= 0 || r.h <= 0) {
        return;
    }
    if (r.x < 0) {
        r.w += r.x;
        r.x = 0;
    }
    if (r.y < 0) {
        r.h += r.y;
        r.y = 0;
    }
    if (r.x + r.w > kWidth) {
        r.w = kWidth - r.x;
    }
    if (r.y + r.h > kHeight) {
        r.h = kHeight - r.y;
    }
    if (r.w <= 0 || r.h <= 0) {
        return;
    }
    if (count >= 12) {
        count = 1;
        rects[0] = Rect{0, 0, kWidth, kHeight};
        return;
    }
    rects[count++] = r;
}

unsigned DirtyList::pixels() const
{
    unsigned n = 0;
    for (int i = 0; i < count; ++i) {
        n += static_cast<unsigned>(rects[i].w * rects[i].h);
    }
    return n;
}

void compose_scene(uint16_t* dst, const ViewModel& view)
{
    if (view.snap.phase == Phase::Title) {
        draw_title(dst, view);
        return;
    }
    draw_world_playfield(dst, view);
    draw_hud_overlay(dst, view);
    if (view.snap.phase == Phase::GameOver) {
        fill_rect(dst, 96, 118, 288, 56, kColorHud);
        draw_text_centered(dst, 136, view.snap.winner == PlayerId::P0 ? "P1 WINS" : "P2 WINS",
                           kColorTitle, 3);
    }
}

void compose_scene(uint16_t* dst, const Match& match)
{
    compose_scene(dst, match.view());
}

void blit_rect(uint16_t* dst, const uint16_t* src, Rect r)
{
    const int x0 = clamp_int(r.x, 0, kWidth);
    const int y0 = clamp_int(r.y, 0, kHeight);
    const int x1 = clamp_int(r.x + r.w, 0, kWidth);
    const int y1 = clamp_int(r.y + r.h, 0, kHeight);
    for (int y = y0; y < y1; ++y) {
        std::memcpy(dst + y * kWidth + x0, src + y * kWidth + x0,
                    static_cast<size_t>(x1 - x0) * sizeof(uint16_t));
    }
}

void draw_projectile(uint16_t* dst, const ViewModel& view)
{
    if (!view.proj.alive) {
        if (view.snap.phase == Phase::Resolving || view.snap.phase == Phase::GameOver) {
            const int x = static_cast<int>(view.proj.pos.x + 0.5f);
            const int y = static_cast<int>(view.proj.pos.y + 0.5f);
            blit_pixel_sprite(dst, x - kSpr_boom.w / 2, y - kSpr_boom.h / 2, kSpr_boom, false);
        }
        return;
    }
    const int x = static_cast<int>(view.proj.pos.x + 0.5f);
    const int y = static_cast<int>(view.proj.pos.y + 0.5f);
    const int tx = x - static_cast<int>(view.proj.vel.x * 3.0f);
    const int ty = y - static_cast<int>(view.proj.vel.y * 3.0f);
    fb_set(dst, tx, ty, kColorP0);
    fb_set(dst, (x + tx) / 2, (y + ty) / 2, kColorBoomHi);
    blit_pixel_sprite(dst, x - kSpr_shell.w / 2, y - kSpr_shell.h / 2, kSpr_shell, false);
}

void draw_projectile(uint16_t* dst, const Match& match)
{
    draw_projectile(dst, match.view());
}

void draw_hud_overlay(uint16_t* dst, const ViewModel& view)
{
    fill_rect(dst, 0, 0, kWidth, kHudHeight, kColorHud);
    fill_rect(dst, 0, kHudHeight - 2, kWidth, 2, kColorHudHi);
    const int hp0 = view.tanks[0].hp;
    const int hp1 = view.tanks[1].hp;
    fill_rect(dst, 6, 5, 104, 12, rgb565(40, 40, 48));
    fill_rect(dst, 8, 6, hp0, 10, kColorP0);
    fill_rect(dst, kWidth - 110, 5, 104, 12, rgb565(40, 40, 48));
    fill_rect(dst, kWidth - 108 + (100 - hp1), 6, hp1, 10, kColorP1);

    char num[8];
    format_int(num, view.snap.angle);
    draw_text(dst, 118, 6, num, kColorText, 2);
    fill_rect(dst, 158, 6, 104, 12, rgb565(40, 40, 48));
    fill_rect(dst, 160, 7, view.snap.power, 10, kColorTitle);

    draw_wind_meter(dst, 300, 5, view.snap.wind, 1, 2);
}

void draw_hud_overlay(uint16_t* dst, const Match& match)
{
    draw_hud_overlay(dst, match.view());
}

namespace {

void copy_z(char* dst, const char* src, int max)
{
    int i = 0;
    while (src[i] != 0 && i + 1 < max) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

bool any_down(const Buttons& d)
{
    return d.up || d.down || d.left || d.right || d.a || d.b;
}

void write_held(char* buf, int max, const Buttons& d)
{
    int i = 0;
    auto add = [&](const char* s) {
        if (i > 0 && i + 1 < max) {
            buf[i++] = ' ';
        }
        while (*s != 0 && i + 1 < max) {
            buf[i++] = *s++;
        }
    };
    if (d.up) {
        add("U");
    }
    if (d.down) {
        add("D");
    }
    if (d.left) {
        add("L");
    }
    if (d.right) {
        add("R");
    }
    if (d.a) {
        add("A");
    }
    if (d.b) {
        add("B");
    }
    if (i == 0) {
        copy_z(buf, "--", max);
        return;
    }
    buf[i] = 0;
}

const char* click_name(const Buttons& p)
{
    if (p.a) {
        return "A";
    }
    if (p.b) {
        return "B";
    }
    if (p.up) {
        return "UP";
    }
    if (p.down) {
        return "DOWN";
    }
    if (p.left) {
        return "LEFT";
    }
    if (p.right) {
        return "RIGHT";
    }
    return nullptr;
}

}  // namespace

void DebugOverlay::on_frame(uint32_t dt_ms, const Buttons& down, const Buttons& pressed)
{
    if (dt_ms > 0) {
        int inst = 1000 / static_cast<int>(dt_ms);
        if (inst > 999) {
            inst = 999;
        }
        fps = (fps == 0) ? inst : (fps * 3 + inst) / 4;
    }
    if (const char* name = click_name(pressed)) {
        copy_z(btn, name, static_cast<int>(sizeof(btn)));
        sticky_ms = 1000;
    } else if (any_down(down)) {
        write_held(btn, static_cast<int>(sizeof(btn)), down);
        sticky_ms = 0;
    } else if (sticky_ms > 0) {
        sticky_ms -= static_cast<int>(dt_ms);
        if (sticky_ms <= 0) {
            sticky_ms = 0;
            copy_z(btn, "--", static_cast<int>(sizeof(btn)));
        }
    } else {
        copy_z(btn, "--", static_cast<int>(sizeof(btn)));
    }
}

void draw_debug_overlay(uint16_t* dst, const DebugOverlay& overlay)
{
    const Rect r = kDebugOverlayRect;
    fill_rect(dst, r.x, r.y, r.w, r.h, kColorHud);
    char fps_line[12];
    fps_line[0] = 'F';
    fps_line[1] = 'P';
    fps_line[2] = 'S';
    fps_line[3] = ' ';
    format_int(fps_line + 4, overlay.fps);
    draw_text(dst, r.x + 4, r.y + 3, fps_line, kColorTitle, 2);
    draw_text(dst, r.x + 4, r.y + 16, overlay.btn, kColorText, 2);
}

namespace {

Rect tank_dirty_rect(const Tank& tank)
{
    const int x = static_cast<int>(tank.x + 0.5f);
    const int y = static_cast<int>(tank.y + 0.5f);
    const int pad = kBarrelLen + 10;
    const int w = kSpr_tank_p0.w > kSpr_tank_p1.w ? kSpr_tank_p0.w : kSpr_tank_p1.w;
    const int h = kSpr_tank_p0.h > kSpr_tank_p1.h ? kSpr_tank_p0.h : kSpr_tank_p1.h;
    return Rect{x - w / 2 - pad, y - h - pad, w + pad * 2, h + pad * 2};
}

Rect proj_dirty_rect(const ViewModel& view)
{
    const int x = static_cast<int>(view.proj.pos.x + 0.5f);
    const int y = static_cast<int>(view.proj.pos.y + 0.5f);
    const int s = kSpr_boom.w > kSpr_shell.w ? kSpr_boom.w : kSpr_shell.w;
    return Rect{x - s / 2 - 2, y - s / 2 - 2, s + 4, s + 4};
}

void paint_overlay(uint16_t* frame, const uint16_t* scene, const DebugOverlay& overlay, DirtyList& dirty)
{
    blit_rect(frame, scene, kDebugOverlayRect);
    draw_debug_overlay(frame, overlay);
    dirty.add(kDebugOverlayRect);
}

void paint_projectile(uint16_t* frame, const uint16_t* scene, const ViewModel& view, DirtyList& dirty,
                      Rect& prev_proj, bool firing)
{
    if (prev_proj.w > 0) {
        blit_rect(frame, scene, prev_proj);
        dirty.add(prev_proj);
        prev_proj = Rect{};
    }
    if (firing || view.snap.phase == Phase::Resolving || view.snap.phase == Phase::GameOver) {
        draw_projectile(frame, view);
        if (firing) {
            prev_proj = proj_dirty_rect(view);
            dirty.add(prev_proj);
        }
    }
}

}  // namespace

void present_frame(uint16_t* frame, uint16_t* scene, const ViewModel& view, bool scene_dirty,
                   bool hud_dirty, DirtyList& dirty, Rect& prev_proj, const DebugOverlay& overlay)
{
    dirty.clear();
    const bool firing = view.snap.phase == Phase::Firing && view.proj.alive;

    if (scene_dirty) {
        compose_scene(scene, view);
        std::memcpy(frame, scene, sizeof(uint16_t) * kPixelCount);
        prev_proj = Rect{};
        if (firing || view.snap.phase == Phase::Resolving || view.snap.phase == Phase::GameOver) {
            draw_projectile(frame, view);
            if (firing) {
                prev_proj = proj_dirty_rect(view);
            }
        }
        draw_debug_overlay(frame, overlay);
        dirty.mark_all();
        return;
    }

    if (hud_dirty) {
        compose_scene(scene, view);
        const Rect hud{0, 0, kWidth, kHudHeight};
        blit_rect(frame, scene, hud);
        dirty.add(hud);
        const Rect t0 = tank_dirty_rect(view.tanks[0]);
        const Rect t1 = tank_dirty_rect(view.tanks[1]);
        blit_rect(frame, scene, t0);
        blit_rect(frame, scene, t1);
        dirty.add(t0);
        dirty.add(t1);
        paint_projectile(frame, scene, view, dirty, prev_proj, firing);
        paint_overlay(frame, scene, overlay, dirty);
        return;
    }

    if (firing || prev_proj.w > 0) {
        paint_projectile(frame, scene, view, dirty, prev_proj, firing);
    }
    paint_overlay(frame, scene, overlay, dirty);
}

void present_frame(uint16_t* frame, uint16_t* scene, Match& match, DirtyList& dirty, Rect& prev_proj,
                   const DebugOverlay& overlay)
{
    const bool scene_dirty = match.scene_dirty();
    const bool hud_dirty = match.hud_dirty();
    present_frame(frame, scene, match.view(), scene_dirty, hud_dirty, dirty, prev_proj, overlay);
    if (scene_dirty) {
        match.clear_scene_dirty();
        match.clear_hud_dirty();
        return;
    }
    if (hud_dirty) {
        match.clear_hud_dirty();
    }
}

}  // namespace artillery
