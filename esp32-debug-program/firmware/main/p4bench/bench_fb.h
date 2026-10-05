#pragma once

#include "bench_common.h"

bool fb_init(framebuffer_t *fb, bool prefer_psram);
void fb_deinit(framebuffer_t *fb);
void fb_clear(framebuffer_t *fb, uint16_t color);
void fb_fill_rect(framebuffer_t *fb, int x, int y, int w, int h, uint16_t color);
void fb_dirty_reset(framebuffer_t *fb);
void fb_dirty_add(framebuffer_t *fb, int x, int y, int w, int h);
void fb_dirty_finalize(framebuffer_t *fb);
size_t fb_flush(lcd_hw_t *lcd, framebuffer_t *fb, render_mode_t mode, int chunk_rows,
                int *out_rects);
