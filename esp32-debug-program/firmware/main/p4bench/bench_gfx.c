#include "bench_gfx.h"

#include "font8.h"

uint32_t bench_rand(uint32_t *state)
{
    /* xorshift32 */
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x ? x : 0xA5A5A5A5u;
    return *state;
}

void bench_seed(uint32_t *state, uint32_t seed)
{
    *state = seed ? seed : 1u;
}

static uint16_t blend565(uint16_t a, uint16_t b, uint8_t alpha)
{
    /* alpha = 0..255 weight of b */
    uint32_t ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    uint32_t br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
    uint32_t inv = 255 - alpha;
    uint32_t r = (ar * inv + br * alpha) / 255;
    uint32_t g = (ag * inv + bg * alpha) / 255;
    uint32_t bl = (ab * inv + bb * alpha) / 255;
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

void gfx_blit_opaque(framebuffer_t *fb, int x, int y, int w, int h, const uint16_t *src,
                     int src_stride)
{
    for (int row = 0; row < h; ++row) {
        int dy = y + row;
        if (dy < 0 || dy >= fb->h) {
            continue;
        }
        for (int col = 0; col < w; ++col) {
            int dx = x + col;
            if (dx < 0 || dx >= fb->w) {
                continue;
            }
            fb->pixels[dy * fb->w + dx] = src[row * src_stride + col];
        }
    }
    fb_dirty_add(fb, x, y, w, h);
}

void gfx_blit_colorkey(framebuffer_t *fb, int x, int y, int w, int h, const uint16_t *src,
                       int src_stride, uint16_t key)
{
    for (int row = 0; row < h; ++row) {
        int dy = y + row;
        if (dy < 0 || dy >= fb->h) {
            continue;
        }
        for (int col = 0; col < w; ++col) {
            int dx = x + col;
            if (dx < 0 || dx >= fb->w) {
                continue;
            }
            uint16_t c = src[row * src_stride + col];
            if (c != key) {
                fb->pixels[dy * fb->w + dx] = c;
            }
        }
    }
    fb_dirty_add(fb, x, y, w, h);
}

void gfx_blit_alpha(framebuffer_t *fb, int x, int y, int w, int h, const uint16_t *src,
                    int src_stride, uint8_t alpha)
{
    for (int row = 0; row < h; ++row) {
        int dy = y + row;
        if (dy < 0 || dy >= fb->h) {
            continue;
        }
        for (int col = 0; col < w; ++col) {
            int dx = x + col;
            if (dx < 0 || dx >= fb->w) {
                continue;
            }
            uint16_t *dst = &fb->pixels[dy * fb->w + dx];
            *dst = blend565(*dst, src[row * src_stride + col], alpha);
        }
    }
    fb_dirty_add(fb, x, y, w, h);
}

void gfx_blit_scaled(framebuffer_t *fb, int x, int y, int dw, int dh, const uint16_t *src,
                     int sw, int sh, int src_stride)
{
    for (int row = 0; row < dh; ++row) {
        int sy = row * sh / dh;
        int dy = y + row;
        if (dy < 0 || dy >= fb->h) {
            continue;
        }
        for (int col = 0; col < dw; ++col) {
            int sx = col * sw / dw;
            int dx = x + col;
            if (dx < 0 || dx >= fb->w) {
                continue;
            }
            fb->pixels[dy * fb->w + dx] = src[sy * src_stride + sx];
        }
    }
    fb_dirty_add(fb, x, y, dw, dh);
}

void gfx_blit_rotated90(framebuffer_t *fb, int x, int y, int w, int h, const uint16_t *src,
                        int src_stride)
{
    /* 90° CW: out size h x w */
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            int dx = x + (h - 1 - row);
            int dy = y + col;
            if (dx < 0 || dy < 0 || dx >= fb->w || dy >= fb->h) {
                continue;
            }
            fb->pixels[dy * fb->w + dx] = src[row * src_stride + col];
        }
    }
    fb_dirty_add(fb, x, y, h, w);
}

void gfx_draw_char(framebuffer_t *fb, int x, int y, char c, uint16_t fg, uint16_t bg, int scale)
{
    const uint8_t *g = font8_glyph(c);
    for (int row = 0; row < 8; ++row) {
        uint8_t bits = g[row];
        for (int col = 0; col < 8; ++col) {
            uint16_t color = (bits & (uint8_t)(0x80 >> col)) ? fg : bg;
            fb_fill_rect(fb, x + col * scale, y + row * scale, scale, scale, color);
        }
    }
}

void gfx_draw_string(framebuffer_t *fb, int x, int y, const char *s, uint16_t fg, uint16_t bg,
                     int scale)
{
    while (*s) {
        gfx_draw_char(fb, x, y, *s, fg, bg, scale);
        x += 8 * scale;
        s++;
    }
}

void gfx_make_sprite(uint16_t *dst, int w, int h, uint16_t color, uint16_t key, int frame)
{
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int border = (x == 0 || y == 0 || x == w - 1 || y == h - 1);
            int diag = ((x + y + frame) & 7) == 0;
            if (border) {
                dst[y * w + x] = COL_WHITE;
            } else if (diag) {
                dst[y * w + x] = COL_YELLOW;
            } else if ((x + y) & 1) {
                dst[y * w + x] = color;
            } else {
                dst[y * w + x] = key;
            }
        }
    }
}
