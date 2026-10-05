#pragma once

#include "bench_common.h"
#include "bench_fb.h"

/* Deterministic PRNG */
uint32_t bench_rand(uint32_t *state);
void bench_seed(uint32_t *state, uint32_t seed);

void gfx_blit_opaque(framebuffer_t *fb, int x, int y, int w, int h, const uint16_t *src,
                     int src_stride);
void gfx_blit_colorkey(framebuffer_t *fb, int x, int y, int w, int h, const uint16_t *src,
                       int src_stride, uint16_t key);
void gfx_blit_alpha(framebuffer_t *fb, int x, int y, int w, int h, const uint16_t *src,
                    int src_stride, uint8_t alpha);
void gfx_blit_scaled(framebuffer_t *fb, int x, int y, int dw, int dh, const uint16_t *src,
                     int sw, int sh, int src_stride);
void gfx_blit_rotated90(framebuffer_t *fb, int x, int y, int w, int h, const uint16_t *src,
                        int src_stride);

void gfx_draw_char(framebuffer_t *fb, int x, int y, char c, uint16_t fg, uint16_t bg, int scale);
void gfx_draw_string(framebuffer_t *fb, int x, int y, const char *s, uint16_t fg, uint16_t bg,
                     int scale);

void gfx_make_sprite(uint16_t *dst, int w, int h, uint16_t color, uint16_t key, int frame);
