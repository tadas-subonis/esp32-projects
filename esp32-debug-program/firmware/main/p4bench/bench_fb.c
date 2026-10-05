#include "bench_common.h"

#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "bench_fb";

bool fb_init(framebuffer_t *fb, bool prefer_psram)
{
    memset(fb, 0, sizeof(*fb));
    fb->w = LCD_H_RES;
    fb->h = LCD_V_RES;
    size_t bytes = (size_t)fb->w * (size_t)fb->h * sizeof(uint16_t);
    if (prefer_psram) {
        fb->pixels = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        fb->in_psram = fb->pixels != NULL;
    }
    if (!fb->pixels) {
        fb->pixels = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        fb->in_psram = false;
    }
    if (!fb->pixels) {
        ESP_LOGE(TAG, "framebuffer alloc failed (%u bytes)", (unsigned)bytes);
        return false;
    }
    memset(fb->pixels, 0, bytes);
    ESP_LOGI(TAG, "FB %dx%d RGB565 in %s (%u bytes)", fb->w, fb->h,
             fb->in_psram ? "PSRAM" : "INTERNAL", (unsigned)bytes);
    return true;
}

void fb_deinit(framebuffer_t *fb)
{
    if (fb->pixels) {
        free(fb->pixels);
        fb->pixels = NULL;
    }
    fb->dirty_n = 0;
    fb->dirty_px = 0;
}

void fb_clear(framebuffer_t *fb, uint16_t color)
{
    size_t n = (size_t)fb->w * (size_t)fb->h;
    for (size_t i = 0; i < n; ++i) {
        fb->pixels[i] = color;
    }
    fb->dirty_n = 1;
    fb->dirty[0] = (rect_t){0, 0, fb->w, fb->h};
    fb->dirty_px = fb->w * fb->h;
}

void fb_fill_rect(framebuffer_t *fb, int x, int y, int w, int h, uint16_t color)
{
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > fb->w) {
        w = fb->w - x;
    }
    if (y + h > fb->h) {
        h = fb->h - y;
    }
    if (w <= 0 || h <= 0) {
        return;
    }
    for (int row = 0; row < h; ++row) {
        uint16_t *p = fb->pixels + (y + row) * fb->w + x;
        for (int col = 0; col < w; ++col) {
            p[col] = color;
        }
    }
}

void fb_dirty_reset(framebuffer_t *fb)
{
    fb->dirty_n = 0;
    fb->dirty_px = 0;
}

static int rect_area(rect_t r)
{
    return r.w * r.h;
}

static bool rect_overlap(rect_t a, rect_t b)
{
    return !(a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y);
}

static rect_t rect_union(rect_t a, rect_t b)
{
    int x0 = a.x < b.x ? a.x : b.x;
    int y0 = a.y < b.y ? a.y : b.y;
    int x1 = (a.x + a.w) > (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    int y1 = (a.y + a.h) > (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    return (rect_t){x0, y0, x1 - x0, y1 - y0};
}

void fb_dirty_add(framebuffer_t *fb, int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > fb->w) {
        w = fb->w - x;
    }
    if (y + h > fb->h) {
        h = fb->h - y;
    }
    if (w <= 0 || h <= 0) {
        return;
    }
    rect_t r = {x, y, w, h};

    for (int i = 0; i < fb->dirty_n; ++i) {
        if (rect_overlap(fb->dirty[i], r)) {
            fb->dirty[i] = rect_union(fb->dirty[i], r);
            return;
        }
    }
    if (fb->dirty_n < BENCH_MAX_DIRTY) {
        fb->dirty[fb->dirty_n++] = r;
        return;
    }
    /* merge into largest existing */
    int best = 0;
    for (int i = 1; i < fb->dirty_n; ++i) {
        if (rect_area(fb->dirty[i]) > rect_area(fb->dirty[best])) {
            best = i;
        }
    }
    fb->dirty[best] = rect_union(fb->dirty[best], r);
}

void fb_dirty_finalize(framebuffer_t *fb)
{
    /* simple pairwise merge pass */
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < fb->dirty_n; ++i) {
            for (int j = i + 1; j < fb->dirty_n; ++j) {
                if (rect_overlap(fb->dirty[i], fb->dirty[j])) {
                    fb->dirty[i] = rect_union(fb->dirty[i], fb->dirty[j]);
                    fb->dirty[j] = fb->dirty[fb->dirty_n - 1];
                    fb->dirty_n--;
                    changed = true;
                    break;
                }
            }
            if (changed) {
                break;
            }
        }
    }
    fb->dirty_px = 0;
    for (int i = 0; i < fb->dirty_n; ++i) {
        fb->dirty_px += rect_area(fb->dirty[i]);
    }
}

size_t fb_flush(lcd_hw_t *lcd, framebuffer_t *fb, render_mode_t mode, int chunk_rows,
                int *out_rects)
{
    size_t bytes = 0;
    int rects = 0;
    if (mode == RENDER_MODE_FULL || fb->dirty_n <= 0) {
        bytes = lcd_hw_blit_rgb565(lcd, 0, 0, fb->w, fb->h, fb->pixels, fb->w, chunk_rows);
        rects = 1;
    } else {
        fb_dirty_finalize(fb);
        for (int i = 0; i < fb->dirty_n; ++i) {
            rect_t r = fb->dirty[i];
            const uint16_t *src = fb->pixels + r.y * fb->w + r.x;
            bytes += lcd_hw_blit_rgb565(lcd, r.x, r.y, r.w, r.h, src, fb->w, chunk_rows);
            rects++;
        }
    }
    if (out_rects) {
        *out_rects = rects;
    }
    fb_dirty_reset(fb);
    return bytes;
}
