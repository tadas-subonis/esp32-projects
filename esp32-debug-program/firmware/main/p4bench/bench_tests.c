#include "bench_tests.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bench_fb.h"
#include "bench_gfx.h"
#include "bench_overlay.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ---------- helpers ---------- */

static int64_t now_us(void)
{
    return esp_timer_get_time();
}

static void frame_transfer(bench_ctx_t *ctx, frame_sample_t *out, int dirty_px_hint)
{
    int64_t t0 = now_us();
    int rects = 0;
    size_t bytes = fb_flush(&ctx->lcd, &ctx->fb, ctx->cfg.render_mode, ctx->cfg.chunk_rows, &rects);
    out->lcd_us = now_us() - t0;
    out->lcd_bytes = bytes;
    out->dirty_rects_merged = rects;
    if (dirty_px_hint >= 0) {
        out->dirty_px = dirty_px_hint;
    } else if (ctx->cfg.render_mode == RENDER_MODE_FULL) {
        out->dirty_px = LCD_H_RES * LCD_V_RES;
    }
}

/* ==================== TEST PATTERN ==================== */

static void pattern_init(bench_ctx_t *ctx)
{
    fb_clear(&ctx->fb, COL_BLACK);
    /* checker + coordinate grid */
    for (int y = 0; y < LCD_V_RES; ++y) {
        for (int x = 0; x < LCD_H_RES; ++x) {
            uint16_t c = (((x / 16) ^ (y / 16)) & 1) ? COL_DKGRAY : COL_BLACK;
            if (x % 32 == 0 || y % 32 == 0) {
                c = COL_GRAY;
            }
            ctx->fb.pixels[y * LCD_H_RES + x] = c;
        }
    }
    fb_fill_rect(&ctx->fb, 0, 0, 40, 40, COL_RED);
    fb_fill_rect(&ctx->fb, LCD_H_RES - 40, 0, 40, 40, COL_GREEN);
    fb_fill_rect(&ctx->fb, 0, LCD_V_RES - 40, 40, 40, COL_BLUE);
    fb_fill_rect(&ctx->fb, LCD_H_RES - 40, LCD_V_RES - 40, 40, 40, COL_YELLOW);
    gfx_draw_string(&ctx->fb, 80, 140, "CHECKER OK", COL_WHITE, COL_BLACK, 2);
    gfx_draw_string(&ctx->fb, 80, 180, "0,0=RED", COL_WHITE, COL_BLACK, 2);
    ctx->fb.dirty_n = 1;
    ctx->fb.dirty[0] = (rect_t){0, 0, LCD_H_RES, LCD_V_RES};
}

static void pattern_frame(bench_ctx_t *ctx, frame_sample_t *out)
{
    int64_t t0 = now_us();
    frame_transfer(ctx, out, LCD_H_RES * LCD_V_RES);
    out->total_us = now_us() - t0;
    /* re-mark full dirty so it keeps refreshing */
    ctx->fb.dirty_n = 1;
    ctx->fb.dirty[0] = (rect_t){0, 0, LCD_H_RES, LCD_V_RES};
}

const bench_test_t TEST_PATTERN = {
    .name = "pattern",
    .help = "checker/grid correctness pattern",
    .init = pattern_init,
    .frame = pattern_frame,
    .deinit = NULL,
};

/* ==================== LCD THROUGHPUT ==================== */

typedef struct {
    int phase;
    int frames_in_phase;
} lcd_state_t;

static void lcd_init(bench_ctx_t *ctx)
{
    lcd_state_t *st = calloc(1, sizeof(*st));
    ctx->test_state = st;
    fb_clear(&ctx->fb, COL_BLACK);
}

static void lcd_deinit(bench_ctx_t *ctx)
{
    free(ctx->test_state);
    ctx->test_state = NULL;
}

static void lcd_frame(bench_ctx_t *ctx, frame_sample_t *out)
{
    lcd_state_t *st = ctx->test_state;
    int64_t t_sim0 = now_us();
    /* no simulation */
    out->sim_us = now_us() - t_sim0;

    int64_t t_r0 = now_us();
    int phase = (st->phase) % 6;
    switch (phase) {
    case 0:
        fb_clear(&ctx->fb, (st->frames_in_phase & 1) ? COL_RED : COL_BLUE);
        break;
    case 1:
        fb_clear(&ctx->fb, COL_GREEN);
        break;
    case 2: /* h stripes */
        for (int y = 0; y < LCD_V_RES; ++y) {
            uint16_t c = (y & 8) ? COL_WHITE : COL_BLACK;
            for (int x = 0; x < LCD_H_RES; ++x) {
                ctx->fb.pixels[y * LCD_H_RES + x] = c;
            }
        }
        break;
    case 3: /* v stripes */
        for (int y = 0; y < LCD_V_RES; ++y) {
            for (int x = 0; x < LCD_H_RES; ++x) {
                ctx->fb.pixels[y * LCD_H_RES + x] = (x & 8) ? COL_YELLOW : COL_BLUE;
            }
        }
        break;
    case 4: { /* pseudo-random */
        uint32_t rng = 0x12345678u + (uint32_t)st->frames_in_phase;
        for (int i = 0; i < LCD_H_RES * LCD_V_RES; ++i) {
            ctx->fb.pixels[i] = (uint16_t)(bench_rand(&rng) & 0xFFFF);
        }
        break;
    }
    default: /* rect regions */
        fb_clear(&ctx->fb, COL_DKGRAY);
        fb_fill_rect(&ctx->fb, 40, 40, 200, 120, COL_RED);
        fb_fill_rect(&ctx->fb, 240, 100, 180, 160, COL_GREEN);
        fb_fill_rect(&ctx->fb, 100, 200, 280, 80, COL_BLUE);
        break;
    }
    ctx->fb.dirty_n = 1;
    ctx->fb.dirty[0] = (rect_t){0, 0, LCD_H_RES, LCD_V_RES};
    out->render_us = now_us() - t_r0;

    overlay_update(ctx);
    overlay_blit(ctx);
    frame_transfer(ctx, out, LCD_H_RES * LCD_V_RES);

    st->frames_in_phase++;
    if (st->frames_in_phase >= 30) {
        st->frames_in_phase = 0;
        st->phase++;
    }
}

const bench_test_t TEST_LCD = {
    .name = "lcd",
    .help = "raw LCD throughput patterns + chunk transfer",
    .init = lcd_init,
    .frame = lcd_frame,
    .deinit = lcd_deinit,
};

/* ==================== MEMORY ==================== */

typedef struct {
    uint16_t *tmp;
    size_t tmp_n;
    int op;
} mem_state_t;

static void memory_init(bench_ctx_t *ctx)
{
    mem_state_t *st = calloc(1, sizeof(*st));
    st->tmp_n = (size_t)LCD_H_RES * LCD_V_RES;
    st->tmp = heap_caps_malloc(st->tmp_n * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (!st->tmp) {
        st->tmp = malloc(st->tmp_n * sizeof(uint16_t));
    }
    ctx->test_state = st;
}

static void memory_deinit(bench_ctx_t *ctx)
{
    mem_state_t *st = ctx->test_state;
    if (st) {
        free(st->tmp);
        free(st);
    }
    ctx->test_state = NULL;
}

static void memory_frame(bench_ctx_t *ctx, frame_sample_t *out)
{
    mem_state_t *st = ctx->test_state;
    int64_t t0 = now_us();
    size_t bytes = st->tmp_n * sizeof(uint16_t);
    switch (st->op % 5) {
    case 0:
        memset(ctx->fb.pixels, 0, bytes);
        break;
    case 1:
        if (st->tmp) {
            memcpy(st->tmp, ctx->fb.pixels, bytes);
        }
        break;
    case 2:
        fb_fill_rect(&ctx->fb, 0, 0, LCD_H_RES, LCD_V_RES, COL_BLUE);
        break;
    case 3:
        fb_fill_rect(&ctx->fb, 100, 80, 280, 160, COL_RED);
        break;
    default:
        if (st->tmp) {
            memcpy(ctx->fb.pixels, st->tmp, bytes);
        }
        break;
    }
    out->sim_us = 0;
    out->render_us = now_us() - t0;
    /* report as LCD-less memory bench: still push one frame so screen shows activity */
    ctx->fb.dirty_n = 1;
    ctx->fb.dirty[0] = (rect_t){0, 0, LCD_H_RES, LCD_V_RES};
    overlay_update(ctx);
    overlay_blit(ctx);
    frame_transfer(ctx, out, LCD_H_RES * LCD_V_RES);
    st->op++;
}

const bench_test_t TEST_MEMORY = {
    .name = "memory",
    .help = "FB clear/memcpy/fill vs PSRAM",
    .init = memory_init,
    .frame = memory_frame,
    .deinit = memory_deinit,
};

/* ==================== SPRITES ==================== */

typedef struct {
    float x, y, vx, vy;
} sprite_t;

typedef struct {
    sprite_t *spr;
    uint16_t *atlas;
    int frame;
    uint32_t rng;
} spr_state_t;

static void sprites_init(bench_ctx_t *ctx)
{
    spr_state_t *st = calloc(1, sizeof(*st));
    int n = ctx->cfg.sprites > 0 ? ctx->cfg.sprites : 100;
    int w = ctx->cfg.sprite_w > 0 ? ctx->cfg.sprite_w : 32;
    int h = ctx->cfg.sprite_h > 0 ? ctx->cfg.sprite_h : 32;
    st->spr = calloc((size_t)n, sizeof(sprite_t));
    st->atlas = malloc((size_t)w * (size_t)h * sizeof(uint16_t));
    bench_seed(&st->rng, 0xC0FFEEu);
    gfx_make_sprite(st->atlas, w, h, COL_CYAN, COL_MAGENTA, 0);
    for (int i = 0; i < n; ++i) {
        st->spr[i].x = (float)(bench_rand(&st->rng) % (LCD_H_RES - w));
        st->spr[i].y = (float)(bench_rand(&st->rng) % (LCD_V_RES - h));
        st->spr[i].vx = 1.0f + (bench_rand(&st->rng) % 5);
        st->spr[i].vy = 1.0f + (bench_rand(&st->rng) % 5);
        if (bench_rand(&st->rng) & 1) {
            st->spr[i].vx = -st->spr[i].vx;
        }
        if (bench_rand(&st->rng) & 1) {
            st->spr[i].vy = -st->spr[i].vy;
        }
    }
    ctx->cfg.sprites = n;
    ctx->test_state = st;
    fb_clear(&ctx->fb, COL_BLACK);
}

static void sprites_deinit(bench_ctx_t *ctx)
{
    spr_state_t *st = ctx->test_state;
    if (st) {
        free(st->spr);
        free(st->atlas);
        free(st);
    }
    ctx->test_state = NULL;
}

static void sprites_frame(bench_ctx_t *ctx, frame_sample_t *out)
{
    spr_state_t *st = ctx->test_state;
    int n = ctx->cfg.sprites;
    int w = ctx->cfg.sprite_w;
    int h = ctx->cfg.sprite_h;

    int64_t t0 = now_us();
    for (int i = 0; i < n; ++i) {
        sprite_t *s = &st->spr[i];
        if (ctx->cfg.render_mode == RENDER_MODE_DIRTY) {
            fb_dirty_add(&ctx->fb, (int)s->x, (int)s->y, w, h);
        }
        s->x += s->vx;
        s->y += s->vy;
        if (s->x < 0) {
            s->x = 0;
            s->vx = -s->vx;
        }
        if (s->y < 0) {
            s->y = 0;
            s->vy = -s->vy;
        }
        if (s->x > LCD_H_RES - w) {
            s->x = (float)(LCD_H_RES - w);
            s->vx = -s->vx;
        }
        if (s->y > LCD_V_RES - h) {
            s->y = (float)(LCD_V_RES - h);
            s->vy = -s->vy;
        }
    }
    out->sim_us = now_us() - t0;

    int64_t tr = now_us();
    if (ctx->cfg.render_mode == RENDER_MODE_FULL) {
        fb_clear(&ctx->fb, COL_BLACK);
    } else {
        /* erase previous by clearing dirty regions already marked; then clear those rects */
        for (int i = 0; i < ctx->fb.dirty_n; ++i) {
            rect_t r = ctx->fb.dirty[i];
            fb_fill_rect(&ctx->fb, r.x, r.y, r.w, r.h, COL_BLACK);
        }
    }
    gfx_make_sprite(st->atlas, w, h, COL_CYAN, COL_MAGENTA, st->frame++);
    for (int i = 0; i < n; ++i) {
        int x = (int)st->spr[i].x;
        int y = (int)st->spr[i].y;
        switch (ctx->cfg.sprite_mode) {
        case 1:
            gfx_blit_colorkey(&ctx->fb, x, y, w, h, st->atlas, w, COL_MAGENTA);
            break;
        case 2:
            gfx_blit_alpha(&ctx->fb, x, y, w, h, st->atlas, w, 160);
            break;
        case 3:
            gfx_blit_scaled(&ctx->fb, x, y, w + 8, h + 8, st->atlas, w, h, w);
            break;
        case 4:
            gfx_blit_rotated90(&ctx->fb, x, y, w, h, st->atlas, w);
            break;
        default:
            gfx_blit_opaque(&ctx->fb, x, y, w, h, st->atlas, w);
            break;
        }
    }
    overlay_update(ctx);
    overlay_blit(ctx);
    out->render_us = now_us() - tr;

    int dirty_before = ctx->fb.dirty_px;
    if (ctx->cfg.render_mode == RENDER_MODE_DIRTY) {
        fb_dirty_finalize(&ctx->fb);
        dirty_before = ctx->fb.dirty_px;
        out->dirty_rects_raw = ctx->fb.dirty_n;
    }
    frame_transfer(ctx, out, dirty_before >= 0 ? dirty_before : LCD_H_RES * LCD_V_RES);
}

const bench_test_t TEST_SPRITES = {
    .name = "sprites",
    .help = "bouncing sprites opaque/colorkey/alpha/scale/rotate",
    .init = sprites_init,
    .frame = sprites_frame,
    .deinit = sprites_deinit,
};

/* ==================== DIRTY CROSSOVER ==================== */

typedef struct {
    int block_w, block_h;
    int count;
    float *x, *y, *vx, *vy;
} dirty_state_t;

static void dirty_init(bench_ctx_t *ctx)
{
    dirty_state_t *st = calloc(1, sizeof(*st));
    /* choose block size so count*area ~= dirty_pct */
    int pct = ctx->cfg.dirty_pct > 0 ? ctx->cfg.dirty_pct : 20;
    int target_px = (LCD_H_RES * LCD_V_RES * pct) / 100;
    st->block_w = 32;
    st->block_h = 32;
    st->count = target_px / (st->block_w * st->block_h);
    if (st->count < 1) {
        st->count = 1;
    }
    if (st->count > 400) {
        st->count = 400;
    }
    st->x = calloc((size_t)st->count, sizeof(float));
    st->y = calloc((size_t)st->count, sizeof(float));
    st->vx = calloc((size_t)st->count, sizeof(float));
    st->vy = calloc((size_t)st->count, sizeof(float));
    uint32_t rng = 99;
    for (int i = 0; i < st->count; ++i) {
        st->x[i] = (float)(bench_rand(&rng) % (LCD_H_RES - st->block_w));
        st->y[i] = (float)(bench_rand(&rng) % (LCD_V_RES - st->block_h));
        st->vx[i] = 2.0f;
        st->vy[i] = 1.5f;
    }
    ctx->test_state = st;
    fb_clear(&ctx->fb, COL_BLACK);
}

static void dirty_deinit(bench_ctx_t *ctx)
{
    dirty_state_t *st = ctx->test_state;
    if (st) {
        free(st->x);
        free(st->y);
        free(st->vx);
        free(st->vy);
        free(st);
    }
    ctx->test_state = NULL;
}

static void dirty_frame(bench_ctx_t *ctx, frame_sample_t *out)
{
    dirty_state_t *st = ctx->test_state;
    int64_t t0 = now_us();
    for (int i = 0; i < st->count; ++i) {
        if (ctx->cfg.render_mode == RENDER_MODE_DIRTY) {
            fb_dirty_add(&ctx->fb, (int)st->x[i], (int)st->y[i], st->block_w, st->block_h);
        }
        st->x[i] += st->vx[i];
        st->y[i] += st->vy[i];
        if (st->x[i] < 0 || st->x[i] > LCD_H_RES - st->block_w) {
            st->vx[i] = -st->vx[i];
        }
        if (st->y[i] < 0 || st->y[i] > LCD_V_RES - st->block_h) {
            st->vy[i] = -st->vy[i];
        }
    }
    out->sim_us = now_us() - t0;

    int64_t tr = now_us();
    if (ctx->cfg.render_mode == RENDER_MODE_FULL) {
        fb_clear(&ctx->fb, COL_BLACK);
    } else {
        for (int i = 0; i < ctx->fb.dirty_n; ++i) {
            rect_t r = ctx->fb.dirty[i];
            fb_fill_rect(&ctx->fb, r.x, r.y, r.w, r.h, COL_BLACK);
        }
    }
    for (int i = 0; i < st->count; ++i) {
        fb_fill_rect(&ctx->fb, (int)st->x[i], (int)st->y[i], st->block_w, st->block_h, COL_ORANGE);
        fb_dirty_add(&ctx->fb, (int)st->x[i], (int)st->y[i], st->block_w, st->block_h);
    }
    overlay_update(ctx);
    overlay_blit(ctx);
    out->render_us = now_us() - tr;

    fb_dirty_finalize(&ctx->fb);
    out->dirty_rects_raw = st->count;
    int dpx = ctx->fb.dirty_px;
    frame_transfer(ctx, out, dpx);
}

const bench_test_t TEST_DIRTY = {
    .name = "dirty",
    .help = "full vs dirty-region crossover (set dirty_pct)",
    .init = dirty_init,
    .frame = dirty_frame,
    .deinit = dirty_deinit,
};

/* ==================== PARTICLES ==================== */

typedef struct {
    float x, y, vx, vy;
    int life;
} particle_t;

typedef struct {
    particle_t *p;
    uint32_t rng;
} part_state_t;

static void particles_init(bench_ctx_t *ctx)
{
    part_state_t *st = calloc(1, sizeof(*st));
    int n = ctx->cfg.particles > 0 ? ctx->cfg.particles : 500;
    st->p = calloc((size_t)n, sizeof(particle_t));
    bench_seed(&st->rng, 0xDEADBEEFu);
    for (int i = 0; i < n; ++i) {
        st->p[i].x = LCD_H_RES * 0.5f;
        st->p[i].y = LCD_V_RES * 0.5f;
        st->p[i].vx = ((int)(bench_rand(&st->rng) % 11) - 5) * 0.5f;
        st->p[i].vy = ((int)(bench_rand(&st->rng) % 11) - 8) * 0.5f;
        st->p[i].life = 30 + (int)(bench_rand(&st->rng) % 90);
    }
    ctx->cfg.particles = n;
    ctx->test_state = st;
    fb_clear(&ctx->fb, COL_BLACK);
}

static void particles_deinit(bench_ctx_t *ctx)
{
    part_state_t *st = ctx->test_state;
    if (st) {
        free(st->p);
        free(st);
    }
    ctx->test_state = NULL;
}

static void particles_frame(bench_ctx_t *ctx, frame_sample_t *out)
{
    part_state_t *st = ctx->test_state;
    int n = ctx->cfg.particles;
    int sz = ctx->cfg.particle_size > 0 ? ctx->cfg.particle_size : 4;

    int64_t t0 = now_us();
    for (int i = 0; i < n; ++i) {
        particle_t *p = &st->p[i];
        p->x += p->vx;
        p->y += p->vy;
        p->vy += 0.08f; /* gravity */
        p->life--;
        if (p->life <= 0 || p->x < 0 || p->y < 0 || p->x >= LCD_H_RES || p->y >= LCD_V_RES) {
            p->x = LCD_H_RES * 0.5f;
            p->y = LCD_V_RES * 0.7f;
            p->vx = ((int)(bench_rand(&st->rng) % 11) - 5) * 0.6f;
            p->vy = -((int)(bench_rand(&st->rng) % 8) + 2) * 0.6f;
            p->life = 40 + (int)(bench_rand(&st->rng) % 80);
        }
    }
    out->sim_us = now_us() - t0;

    int64_t tr = now_us();
    fb_clear(&ctx->fb, COL_BLACK);
    for (int i = 0; i < n; ++i) {
        fb_fill_rect(&ctx->fb, (int)st->p[i].x, (int)st->p[i].y, sz, sz,
                     (st->p[i].life > 40) ? COL_YELLOW : COL_ORANGE);
    }
    overlay_update(ctx);
    overlay_blit(ctx);
    out->render_us = now_us() - tr;
    frame_transfer(ctx, out, LCD_H_RES * LCD_V_RES);
}

const bench_test_t TEST_PARTICLES = {
    .name = "particles",
    .help = "particle sim + draw",
    .init = particles_init,
    .frame = particles_frame,
    .deinit = particles_deinit,
};

/* ==================== TILES ==================== */

#define TILE_MAP_W 256
#define TILE_MAP_H 256
#define TILE_PX    16

typedef struct {
    uint8_t *map; /* TILE_MAP_W * TILE_MAP_H */
    uint16_t tiles[8][TILE_PX * TILE_PX];
    float cam_x, cam_y;
    float cvx, cvy;
    int anim;
} tile_state_t;

static void tiles_init(bench_ctx_t *ctx)
{
    tile_state_t *st = calloc(1, sizeof(*st));
    st->map = malloc((size_t)TILE_MAP_W * TILE_MAP_H);
    uint32_t rng = 42;
    for (int i = 0; i < TILE_MAP_W * TILE_MAP_H; ++i) {
        st->map[i] = (uint8_t)(bench_rand(&rng) % 8);
    }
    for (int t = 0; t < 8; ++t) {
        uint16_t c = (uint16_t[]){COL_GREEN, COL_DKGRAY, COL_BLUE, COL_GRAY,
                                  COL_ORANGE, COL_CYAN, COL_MAGENTA, COL_YELLOW}[t];
        for (int i = 0; i < TILE_PX * TILE_PX; ++i) {
            st->tiles[t][i] = ((i / TILE_PX + i) & 3) ? c : COL_BLACK;
        }
    }
    st->cvx = 1.2f;
    st->cvy = 0.7f;
    ctx->test_state = st;
    fb_clear(&ctx->fb, COL_BLACK);
}

static void tiles_deinit(bench_ctx_t *ctx)
{
    tile_state_t *st = ctx->test_state;
    if (st) {
        free(st->map);
        free(st);
    }
    ctx->test_state = NULL;
}

static void tiles_frame(bench_ctx_t *ctx, frame_sample_t *out)
{
    tile_state_t *st = ctx->test_state;
    int layers = ctx->cfg.tile_layers > 0 ? ctx->cfg.tile_layers : 1;

    int64_t t0 = now_us();
    st->cam_x += st->cvx;
    st->cam_y += st->cvy;
    if (st->cam_x < 0 || st->cam_x > (TILE_MAP_W * TILE_PX - LCD_H_RES)) {
        st->cvx = -st->cvx;
    }
    if (st->cam_y < 0 || st->cam_y > (TILE_MAP_H * TILE_PX - LCD_V_RES)) {
        st->cvy = -st->cvy;
    }
    st->anim++;
    out->sim_us = now_us() - t0;

    int64_t tr = now_us();
    int cam_x = (int)st->cam_x;
    int cam_y = (int)st->cam_y;
    int tw = LCD_H_RES / TILE_PX + 2;
    int th = LCD_V_RES / TILE_PX + 2;
    int ox = cam_x / TILE_PX;
    int oy = cam_y / TILE_PX;
    int px = -(cam_x % TILE_PX);
    int py = -(cam_y % TILE_PX);

    for (int layer = 0; layer < layers; ++layer) {
        int parallax = layer * 3;
        for (int ty = 0; ty < th; ++ty) {
            for (int tx = 0; tx < tw; ++tx) {
                int mx = (ox + tx + parallax) & (TILE_MAP_W - 1);
                int my = (oy + ty + parallax) & (TILE_MAP_H - 1);
                int tid = st->map[my * TILE_MAP_W + mx];
                if (layer > 0 && (tid & 1) == 0) {
                    continue; /* sparse upper layers */
                }
                if ((st->anim & 8) && tid == 7) {
                    tid = 6; /* animated */
                }
                gfx_blit_opaque(&ctx->fb, px + tx * TILE_PX, py + ty * TILE_PX, TILE_PX, TILE_PX,
                                st->tiles[tid], TILE_PX);
            }
        }
    }
    overlay_update(ctx);
    overlay_blit(ctx);
    out->render_us = now_us() - tr;
    frame_transfer(ctx, out, LCD_H_RES * LCD_V_RES);
}

const bench_test_t TEST_TILES = {
    .name = "tiles",
    .help = "scrolling tile map layers",
    .init = tiles_init,
    .frame = tiles_frame,
    .deinit = tiles_deinit,
};

/* ==================== COMPOSE (expensive ops) ==================== */

static void compose_init(bench_ctx_t *ctx)
{
    ctx->test_state = calloc(1, sizeof(spr_state_t));
    spr_state_t *st = ctx->test_state;
    int n = ctx->cfg.sprites > 0 ? ctx->cfg.sprites : 50;
    ctx->cfg.sprites = n;
    st->spr = calloc((size_t)n, sizeof(sprite_t));
    st->atlas = malloc(32 * 32 * sizeof(uint16_t));
    gfx_make_sprite(st->atlas, 32, 32, COL_RED, COL_MAGENTA, 0);
    uint32_t rng = 7;
    for (int i = 0; i < n; ++i) {
        st->spr[i].x = (float)(bench_rand(&rng) % 400);
        st->spr[i].y = (float)(bench_rand(&rng) % 260);
        st->spr[i].vx = 1;
        st->spr[i].vy = 1;
    }
}

static void compose_deinit(bench_ctx_t *ctx)
{
    sprites_deinit(ctx);
}

static void compose_frame(bench_ctx_t *ctx, frame_sample_t *out)
{
    /* cycle sprite_mode each ~60 frames via metrics frames */
    ctx->cfg.sprite_mode = (int)((ctx->metrics.frames / 60) % 5);
    sprites_frame(ctx, out);
}

const bench_test_t TEST_COMPOSE = {
    .name = "compose",
    .help = "opaque vs colorkey vs alpha vs scale vs rotate",
    .init = compose_init,
    .frame = compose_frame,
    .deinit = compose_deinit,
};

/* ==================== HEADLESS TD ==================== */

typedef struct {
    float progress;
    float speed;
    int hp;
    int alive;
} enemy_t;

typedef struct {
    float x, y;
    float range;
    int cooldown;
    int cd_left;
    int target;
} tower_t;

typedef struct {
    float x, y, tx, ty;
    float speed;
    int dmg;
    int alive;
} proj_t;

typedef struct {
    enemy_t *enemies;
    tower_t *towers;
    proj_t *projs;
    int n_e, n_t, n_p;
    int use_grid;
    uint32_t rng;
    /* path: horizontal then vertical */
} td_state_t;

static void td_spawn(td_state_t *st)
{
    for (int i = 0; i < st->n_e; ++i) {
        if (!st->enemies[i].alive) {
            st->enemies[i].alive = 1;
            st->enemies[i].progress = 0;
            st->enemies[i].speed = 0.4f + (bench_rand(&st->rng) % 10) * 0.05f;
            st->enemies[i].hp = 30 + (int)(bench_rand(&st->rng) % 40);
            return;
        }
    }
}

static void td_enemy_pos(const enemy_t *e, float *x, float *y)
{
    float p = e->progress;
    if (p < 200.0f) {
        *x = p;
        *y = 40.0f;
    } else if (p < 400.0f) {
        *x = 200.0f;
        *y = 40.0f + (p - 200.0f);
    } else if (p < 680.0f) {
        *x = 200.0f + (p - 400.0f);
        *y = 240.0f;
    } else {
        *x = 480.0f;
        *y = 240.0f;
    }
}

static void td_sim_init_common(bench_ctx_t *ctx, bool visual)
{
    (void)visual;
    td_state_t *st = calloc(1, sizeof(*st));
    st->n_e = ctx->cfg.enemies > 0 ? ctx->cfg.enemies : 100;
    st->n_t = ctx->cfg.towers > 0 ? ctx->cfg.towers : 20;
    st->n_p = ctx->cfg.projectiles > 0 ? ctx->cfg.projectiles : 100;
    st->enemies = calloc((size_t)st->n_e, sizeof(enemy_t));
    st->towers = calloc((size_t)st->n_t, sizeof(tower_t));
    st->projs = calloc((size_t)st->n_p, sizeof(proj_t));
    bench_seed(&st->rng, 0x7D51EDu);
    for (int i = 0; i < st->n_t; ++i) {
        st->towers[i].x = 40.0f + (i % 10) * 40.0f;
        st->towers[i].y = 60.0f + (i / 10) * 50.0f;
        st->towers[i].range = 80.0f;
        st->towers[i].cooldown = 15;
        st->towers[i].cd_left = i % 15;
        st->towers[i].target = -1;
    }
    for (int i = 0; i < st->n_e / 2; ++i) {
        td_spawn(st);
        st->enemies[i].progress = (float)(bench_rand(&st->rng) % 500);
    }
    ctx->cfg.enemies = st->n_e;
    ctx->cfg.towers = st->n_t;
    ctx->cfg.projectiles = st->n_p;
    ctx->test_state = st;
}

static void td_sim_step(td_state_t *st)
{
    /* enemies */
    for (int i = 0; i < st->n_e; ++i) {
        if (!st->enemies[i].alive) {
            continue;
        }
        st->enemies[i].progress += st->enemies[i].speed;
        if (st->enemies[i].progress > 700.0f || st->enemies[i].hp <= 0) {
            st->enemies[i].alive = 0;
            td_spawn(st);
        }
    }
    /* towers acquire + fire */
    for (int t = 0; t < st->n_t; ++t) {
        tower_t *tw = &st->towers[t];
        if (tw->cd_left > 0) {
            tw->cd_left--;
        }
        tw->target = -1;
        float best = tw->range * tw->range;
        for (int e = 0; e < st->n_e; ++e) {
            if (!st->enemies[e].alive) {
                continue;
            }
            float ex, ey;
            td_enemy_pos(&st->enemies[e], &ex, &ey);
            float dx = ex - tw->x;
            float dy = ey - tw->y;
            float d2 = dx * dx + dy * dy;
            if (d2 < best) {
                best = d2;
                tw->target = e;
            }
        }
        if (tw->target >= 0 && tw->cd_left == 0) {
            for (int p = 0; p < st->n_p; ++p) {
                if (!st->projs[p].alive) {
                    float ex, ey;
                    td_enemy_pos(&st->enemies[tw->target], &ex, &ey);
                    st->projs[p].alive = 1;
                    st->projs[p].x = tw->x;
                    st->projs[p].y = tw->y;
                    st->projs[p].tx = ex;
                    st->projs[p].ty = ey;
                    st->projs[p].speed = 6.0f;
                    st->projs[p].dmg = 8;
                    tw->cd_left = tw->cooldown;
                    break;
                }
            }
        }
    }
    /* projectiles */
    for (int p = 0; p < st->n_p; ++p) {
        if (!st->projs[p].alive) {
            continue;
        }
        float dx = st->projs[p].tx - st->projs[p].x;
        float dy = st->projs[p].ty - st->projs[p].y;
        float d2 = dx * dx + dy * dy;
        if (d2 < 16.0f) {
            /* hit nearest enemy */
            for (int e = 0; e < st->n_e; ++e) {
                if (!st->enemies[e].alive) {
                    continue;
                }
                float ex, ey;
                td_enemy_pos(&st->enemies[e], &ex, &ey);
                float hx = ex - st->projs[p].x;
                float hy = ey - st->projs[p].y;
                if (hx * hx + hy * hy < 100.0f) {
                    st->enemies[e].hp -= st->projs[p].dmg;
                    break;
                }
            }
            st->projs[p].alive = 0;
            continue;
        }
        float inv = st->projs[p].speed / (sqrtf(d2) + 0.001f);
        st->projs[p].x += dx * inv;
        st->projs[p].y += dy * inv;
    }
}

static void td_sim_init(bench_ctx_t *ctx)
{
    td_sim_init_common(ctx, false);
}

static void td_sim_deinit(bench_ctx_t *ctx)
{
    td_state_t *st = ctx->test_state;
    if (st) {
        free(st->enemies);
        free(st->towers);
        free(st->projs);
        free(st);
    }
    ctx->test_state = NULL;
}

static void td_sim_frame(bench_ctx_t *ctx, frame_sample_t *out)
{
    td_state_t *st = ctx->test_state;
    int64_t t0 = now_us();
    td_sim_step(st);
    out->sim_us = now_us() - t0;
    out->render_us = 0;
    out->lcd_us = 0;
    out->lcd_bytes = 0;
    out->total_us = out->sim_us;
    /* occasional yield so watchdog is happy under huge loads */
    if ((ctx->metrics.frames & 15) == 0) {
        vTaskDelay(1);
    }
}

const bench_test_t TEST_TD_SIM = {
    .name = "simulation",
    .help = "headless tower-defense simulation (no draw)",
    .init = td_sim_init,
    .frame = td_sim_frame,
    .deinit = td_sim_deinit,
};

/* ==================== VISUAL TD ==================== */

static void td_vis_init(bench_ctx_t *ctx)
{
    td_sim_init_common(ctx, true);
    fb_clear(&ctx->fb, COL_BLACK);
}

static void td_vis_frame(bench_ctx_t *ctx, frame_sample_t *out)
{
    td_state_t *st = ctx->test_state;
    int64_t t0 = now_us();
    td_sim_step(st);
    out->sim_us = now_us() - t0;

    int64_t tr = now_us();
    fb_clear(&ctx->fb, 0x01CF); /* dark blue-ish ground */
    /* path */
    fb_fill_rect(&ctx->fb, 0, 32, 208, 24, COL_DKGRAY);
    fb_fill_rect(&ctx->fb, 192, 32, 24, 220, COL_DKGRAY);
    fb_fill_rect(&ctx->fb, 192, 228, 288, 24, COL_DKGRAY);
    for (int t = 0; t < st->n_t; ++t) {
        int x = (int)st->towers[t].x - 6;
        int y = (int)st->towers[t].y - 6;
        fb_fill_rect(&ctx->fb, x, y, 12, 12, COL_BLUE);
    }
    for (int e = 0; e < st->n_e; ++e) {
        if (!st->enemies[e].alive) {
            continue;
        }
        float ex, ey;
        td_enemy_pos(&st->enemies[e], &ex, &ey);
        fb_fill_rect(&ctx->fb, (int)ex - 4, (int)ey - 4, 8, 8, COL_RED);
        int hp = st->enemies[e].hp;
        fb_fill_rect(&ctx->fb, (int)ex - 6, (int)ey - 8, 12, 2, COL_BLACK);
        fb_fill_rect(&ctx->fb, (int)ex - 6, (int)ey - 8, hp / 6, 2, COL_GREEN);
    }
    for (int p = 0; p < st->n_p; ++p) {
        if (!st->projs[p].alive) {
            continue;
        }
        fb_fill_rect(&ctx->fb, (int)st->projs[p].x - 1, (int)st->projs[p].y - 1, 3, 3, COL_YELLOW);
    }
    overlay_update(ctx);
    overlay_blit(ctx);
    out->render_us = now_us() - tr;
    frame_transfer(ctx, out, LCD_H_RES * LCD_V_RES);
}

const bench_test_t TEST_TD = {
    .name = "td",
    .help = "visual tower-defense combined workload",
    .init = td_vis_init,
    .frame = td_vis_frame,
    .deinit = td_sim_deinit,
};

/* ==================== CHAOS ==================== */

typedef struct {
    td_state_t *td;
    spr_state_t *spr;
    part_state_t *part;
    tile_state_t *tiles;
} chaos_state_t;

static void chaos_init(bench_ctx_t *ctx)
{
    chaos_state_t *st = calloc(1, sizeof(*st));
    /* borrow inits by temporarily swapping test_state */
    td_sim_init_common(ctx, true);
    st->td = ctx->test_state;

    ctx->cfg.sprites = ctx->cfg.sprites > 0 ? ctx->cfg.sprites : 80;
    ctx->cfg.sprite_w = 16;
    ctx->cfg.sprite_h = 16;
    sprites_init(ctx);
    st->spr = ctx->test_state;

    ctx->cfg.particles = ctx->cfg.particles > 0 ? ctx->cfg.particles : 400;
    particles_init(ctx);
    st->part = ctx->test_state;

    tiles_init(ctx);
    st->tiles = ctx->test_state;

    ctx->test_state = st;
    fb_clear(&ctx->fb, COL_BLACK);
}

static void chaos_deinit(bench_ctx_t *ctx)
{
    chaos_state_t *st = ctx->test_state;
    if (!st) {
        return;
    }
    ctx->test_state = st->td;
    td_sim_deinit(ctx);
    ctx->test_state = st->spr;
    sprites_deinit(ctx);
    ctx->test_state = st->part;
    particles_deinit(ctx);
    ctx->test_state = st->tiles;
    tiles_deinit(ctx);
    free(st);
    ctx->test_state = NULL;
}

static void chaos_frame(bench_ctx_t *ctx, frame_sample_t *out)
{
    chaos_state_t *st = ctx->test_state;

    int64_t t0 = now_us();
    td_sim_step(st->td);
    /* sprite motion */
    int n = ctx->cfg.sprites;
    int w = ctx->cfg.sprite_w > 0 ? ctx->cfg.sprite_w : 16;
    int h = ctx->cfg.sprite_h > 0 ? ctx->cfg.sprite_h : 16;
    for (int i = 0; i < n && st->spr; ++i) {
        sprite_t *s = &st->spr->spr[i];
        s->x += s->vx;
        s->y += s->vy;
        if (s->x < 0 || s->x > LCD_H_RES - w) {
            s->vx = -s->vx;
        }
        if (s->y < 0 || s->y > LCD_V_RES - h) {
            s->vy = -s->vy;
        }
    }
    /* particles */
    int pn = ctx->cfg.particles;
    for (int i = 0; i < pn && st->part; ++i) {
        particle_t *p = &st->part->p[i];
        p->x += p->vx;
        p->y += p->vy;
        p->vy += 0.05f;
        p->life--;
        if (p->life <= 0) {
            p->x = (float)(bench_rand(&st->part->rng) % LCD_H_RES);
            p->y = (float)(bench_rand(&st->part->rng) % LCD_V_RES);
            p->vx = ((int)(bench_rand(&st->part->rng) % 7) - 3);
            p->vy = -((int)(bench_rand(&st->part->rng) % 5));
            p->life = 50;
        }
    }
    st->tiles->cam_x += st->tiles->cvx;
    st->tiles->cam_y += st->tiles->cvy;
    out->sim_us = now_us() - t0;

    int64_t tr = now_us();
    /* background tiles (cheap sample) */
    ctx->test_state = st->tiles;
    /* inline mini tile draw */
    {
        tile_state_t *ts = st->tiles;
        int cam_x = (int)ts->cam_x;
        int cam_y = (int)ts->cam_y;
        for (int y = 0; y < LCD_V_RES; y += TILE_PX) {
            for (int x = 0; x < LCD_H_RES; x += TILE_PX) {
                int mx = ((cam_x + x) / TILE_PX) & (TILE_MAP_W - 1);
                int my = ((cam_y + y) / TILE_PX) & (TILE_MAP_H - 1);
                int tid = ts->map[my * TILE_MAP_W + mx] & 7;
                gfx_blit_opaque(&ctx->fb, x, y, TILE_PX, TILE_PX, ts->tiles[tid], TILE_PX);
            }
        }
    }
    /* TD entities */
    td_state_t *td = st->td;
    for (int t = 0; t < td->n_t; ++t) {
        fb_fill_rect(&ctx->fb, (int)td->towers[t].x - 5, (int)td->towers[t].y - 5, 10, 10, COL_BLUE);
    }
    for (int e = 0; e < td->n_e; ++e) {
        if (!td->enemies[e].alive) {
            continue;
        }
        float ex, ey;
        td_enemy_pos(&td->enemies[e], &ex, &ey);
        fb_fill_rect(&ctx->fb, (int)ex - 3, (int)ey - 3, 6, 6, COL_RED);
    }
    for (int p = 0; p < td->n_p; ++p) {
        if (td->projs[p].alive) {
            fb_fill_rect(&ctx->fb, (int)td->projs[p].x, (int)td->projs[p].y, 2, 2, COL_WHITE);
        }
    }
    if (st->spr && st->spr->atlas) {
        gfx_make_sprite(st->spr->atlas, w, h, COL_CYAN, COL_MAGENTA, st->spr->frame++);
        for (int i = 0; i < n; ++i) {
            gfx_blit_colorkey(&ctx->fb, (int)st->spr->spr[i].x, (int)st->spr->spr[i].y, w, h,
                              st->spr->atlas, w, COL_MAGENTA);
        }
    }
    int psz = ctx->cfg.particle_size > 0 ? ctx->cfg.particle_size : 2;
    for (int i = 0; i < pn && st->part; ++i) {
        fb_fill_rect(&ctx->fb, (int)st->part->p[i].x, (int)st->part->p[i].y, psz, psz, COL_YELLOW);
    }
    overlay_update(ctx);
    overlay_blit(ctx);
    out->render_us = now_us() - tr;
    ctx->test_state = st;
    frame_transfer(ctx, out, LCD_H_RES * LCD_V_RES);
}

const bench_test_t TEST_CHAOS = {
    .name = "chaos",
    .help = "everything at once; raise load with set commands",
    .init = chaos_init,
    .frame = chaos_frame,
    .deinit = chaos_deinit,
};

/* ==================== registry + auto ==================== */

static const bench_test_t *ALL_TESTS[] = {
    &TEST_PATTERN, &TEST_LCD,     &TEST_MEMORY, &TEST_SPRITES, &TEST_DIRTY, &TEST_PARTICLES,
    &TEST_TILES,   &TEST_COMPOSE, &TEST_TD_SIM, &TEST_TD,      &TEST_CHAOS,
};

const bench_test_t *bench_find_test(const char *name)
{
    for (size_t i = 0; i < sizeof(ALL_TESTS) / sizeof(ALL_TESTS[0]); ++i) {
        if (strcmp(ALL_TESTS[i]->name, name) == 0) {
            return ALL_TESTS[i];
        }
    }
    /* aliases */
    if (strcmp(name, "sim") == 0) {
        return &TEST_TD_SIM;
    }
    return NULL;
}

void bench_list_tests(void)
{
    printf("Available tests:\n");
    for (size_t i = 0; i < sizeof(ALL_TESTS) / sizeof(ALL_TESTS[0]); ++i) {
        printf("  %-12s %s\n", ALL_TESTS[i]->name, ALL_TESTS[i]->help);
    }
}

void bench_run_auto(bench_ctx_t *ctx, const char *which, int target_fps, int seconds_per_step)
{
    printf("\nAUTO %s BENCHMARK  Target FPS: %d  mode=%d\n\n", which, target_fps,
           (int)ctx->cfg.render_mode);
    ctx->cfg.target_fps = target_fps;
    ctx->cfg.overlay_on = false;
    int best = 0;

    int counts[16];
    if (strcmp(which, "td") == 0 || strcmp(which, "simulation") == 0) {
        int td_counts[] = {50, 100, 200, 400, 800, 1200, 2000, 4000, 8000, 16000, 0};
        memcpy(counts, td_counts, sizeof(td_counts));
    } else if (strcmp(which, "particles") == 0) {
        int p_counts[] = {100, 250, 500, 1000, 2000, 5000, 10000, 0};
        memcpy(counts, p_counts, sizeof(p_counts));
    } else {
        int s_counts[] = {10, 25, 50, 100, 200, 400, 800, 1600, 0};
        memcpy(counts, s_counts, sizeof(s_counts));
    }

    for (int ci = 0; counts[ci] > 0; ++ci) {
        int count = counts[ci];
        if (strcmp(which, "sprites") == 0) {
            ctx->cfg.sprites = count;
        } else if (strcmp(which, "particles") == 0) {
            ctx->cfg.particles = count;
        } else if (strcmp(which, "simulation") == 0 || strcmp(which, "td") == 0) {
            ctx->cfg.enemies = count;
            ctx->cfg.towers = count / 5;
            if (ctx->cfg.towers < 10) {
                ctx->cfg.towers = 10;
            }
            ctx->cfg.projectiles = count;
        } else {
            printf("auto: unknown which=%s\n", which);
            return;
        }

        const bench_test_t *t = bench_find_test(strcmp(which, "simulation") == 0 ? "simulation"
                                                : strcmp(which, "td") == 0         ? "td"
                                                                                   : which);
        if (!t) {
            return;
        }
        if (ctx->test && ctx->test->deinit) {
            ctx->test->deinit(ctx);
        }
        ctx->test = t;
        strncpy(ctx->cfg.active_test, t->name, sizeof(ctx->cfg.active_test) - 1);
        metrics_reset(&ctx->metrics);
        t->init(ctx);

        /* ~1 s warm-up (not fixed frame count — full-frame SPI is ~370 ms/frame) */
        int64_t warm_end = now_us() + 1000000;
        while (now_us() < warm_end) {
            frame_sample_t f;
            metrics_begin_frame(&ctx->metrics, &f);
            int64_t tf0 = now_us();
            t->frame(ctx, &f);
            if (f.total_us <= 0) {
                f.total_us = now_us() - tf0;
            }
            metrics_end_frame(&ctx->metrics, &f);
        }
        metrics_reset(&ctx->metrics);

        int64_t end = now_us() + (int64_t)seconds_per_step * 1000000;
        while (now_us() < end) {
            frame_sample_t f;
            metrics_begin_frame(&ctx->metrics, &f);
            int64_t tf0 = now_us();
            t->frame(ctx, &f);
            if (f.total_us <= 0) {
                f.total_us = now_us() - tf0;
            }
            metrics_end_frame(&ctx->metrics, &f);
        }

        float fps = metrics_avg_fps(&ctx->metrics);
        float sim_ms = ctx->metrics.frames
                           ? (float)(ctx->metrics.sum_sim_us / (double)ctx->metrics.frames / 1000.0)
                           : 0.0f;
        float lcd_ms = ctx->metrics.frames
                           ? (float)(ctx->metrics.sum_lcd_us / (double)ctx->metrics.frames / 1000.0)
                           : 0.0f;
        printf("%d -> %.1f fps  (sim %.2f ms  lcd %.1f ms)\n", count, fps, sim_ms, lcd_ms);
        if (fps >= (float)target_fps) {
            best = count;
        } else {
            break;
        }
    }

    printf("\nRESULT:\n%d %s @ >=%d FPS\n\n", best, which, target_fps);
    char extra[96];
    snprintf(extra, sizeof(extra), "\"auto_best\":%d,\"auto_target\":%d,\"render_mode\":%d", best,
             target_fps, (int)ctx->cfg.render_mode);
    metrics_print_human(which, &ctx->cfg, &ctx->metrics, extra);
    metrics_print_jsonl(which, &ctx->cfg, &ctx->metrics, extra);

    if (ctx->test && ctx->test->deinit) {
        ctx->test->deinit(ctx);
        ctx->test = NULL;
    }
    ctx->cfg.running = false;
    ctx->cfg.overlay_on = true;
}

static void measure_dirty_mode(bench_ctx_t *ctx, int pct, render_mode_t mode, int seconds,
                               float *out_fps, float *out_lcd_ms, float *out_bytes)
{
    ctx->cfg.dirty_pct = pct;
    ctx->cfg.render_mode = mode;
    ctx->cfg.overlay_on = false;
    if (ctx->test && ctx->test->deinit) {
        ctx->test->deinit(ctx);
    }
    ctx->test = &TEST_DIRTY;
    strncpy(ctx->cfg.active_test, "dirty", sizeof(ctx->cfg.active_test) - 1);
    metrics_reset(&ctx->metrics);
    TEST_DIRTY.init(ctx);

    int64_t warm_end = now_us() + 800000;
    while (now_us() < warm_end) {
        frame_sample_t f;
        metrics_begin_frame(&ctx->metrics, &f);
        int64_t tf0 = now_us();
        TEST_DIRTY.frame(ctx, &f);
        if (f.total_us <= 0) {
            f.total_us = now_us() - tf0;
        }
        metrics_end_frame(&ctx->metrics, &f);
    }
    metrics_reset(&ctx->metrics);

    int64_t end = now_us() + (int64_t)seconds * 1000000;
    while (now_us() < end) {
        frame_sample_t f;
        metrics_begin_frame(&ctx->metrics, &f);
        int64_t tf0 = now_us();
        TEST_DIRTY.frame(ctx, &f);
        if (f.total_us <= 0) {
            f.total_us = now_us() - tf0;
        }
        metrics_end_frame(&ctx->metrics, &f);
    }

    *out_fps = metrics_avg_fps(&ctx->metrics);
    *out_lcd_ms =
        ctx->metrics.frames ? (float)(ctx->metrics.sum_lcd_us / ctx->metrics.frames / 1000.0) : 0;
    *out_bytes =
        ctx->metrics.frames ? (float)(ctx->metrics.sum_lcd_bytes / (double)ctx->metrics.frames) : 0;

    if (ctx->test && ctx->test->deinit) {
        ctx->test->deinit(ctx);
        ctx->test = NULL;
    }
}

void bench_run_crossover(bench_ctx_t *ctx, int seconds_per_step)
{
    printf("\n=== DIRTY vs FULL CROSSOVER ===\n");
    printf("seconds/step=%d\n\n", seconds_per_step);
    int pcts[] = {1, 5, 10, 20, 40, 60, 80, 100, 0};
    float full_fps_at_100 = 0;
    int crossover_pct = -1;

    for (int i = 0; pcts[i] > 0; ++i) {
        int pct = pcts[i];
        float dfps, dlcd, dbytes, ffps, flcd, fbytes;
        measure_dirty_mode(ctx, pct, RENDER_MODE_DIRTY, seconds_per_step, &dfps, &dlcd, &dbytes);
        measure_dirty_mode(ctx, pct, RENDER_MODE_FULL, seconds_per_step, &ffps, &flcd, &fbytes);
        if (pct == 100) {
            full_fps_at_100 = ffps;
        }
        printf("pct=%3d  dirty: fps=%.1f lcd=%.1fms bytes=%.0f  |  full: fps=%.1f lcd=%.1fms "
               "bytes=%.0f\n",
               pct, dfps, dlcd, dbytes, ffps, flcd, fbytes);
        printf("JSONL {\"test\":\"crossover\",\"dirty_pct\":%d,\"dirty_fps\":%.2f,\"dirty_lcd_ms\":"
               "%.2f,\"dirty_bytes\":%.0f,\"full_fps\":%.2f,\"full_lcd_ms\":%.2f,\"full_bytes\":%.0f}"
               "\n",
               pct, dfps, dlcd, dbytes, ffps, flcd, fbytes);
        if (crossover_pct < 0 && dfps < ffps) {
            crossover_pct = pct;
        }
    }

    printf("\nCROSSOVER: dirty slower than full starting around %d%% changed "
           "(full@100%%=%.1f FPS)\n",
           crossover_pct >= 0 ? crossover_pct : 100, full_fps_at_100);
    printf("=== END CROSSOVER ===\n\n");
    ctx->cfg.running = false;
    ctx->cfg.overlay_on = true;
}

void bench_run_memory_bench(bench_ctx_t *ctx)
{
    printf("\n=== MEMORY BENCH ===\n");
    size_t n = (size_t)LCD_H_RES * LCD_V_RES;
    size_t bytes = n * sizeof(uint16_t);
    uint16_t *tmp = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (!tmp) {
        tmp = malloc(bytes);
    }
    const int iters = 40;
    int64_t t0, t1;
    double ms, mbps;

    t0 = now_us();
    for (int i = 0; i < iters; ++i) {
        memset(ctx->fb.pixels, 0, bytes);
    }
    t1 = now_us();
    ms = (double)(t1 - t0) / iters / 1000.0;
    mbps = (bytes / (ms / 1000.0)) / (1024.0 * 1024.0);
    printf("fb_clear(memset)     %.3f ms  %.1f MB/s  (%s)\n", ms, mbps,
           ctx->fb.in_psram ? "PSRAM" : "INT");
    printf("JSONL {\"test\":\"memory\",\"op\":\"fb_clear\",\"ms\":%.3f,\"mbps\":%.1f,\"bytes\":%u}"
           "\n",
           ms, mbps, (unsigned)bytes);

    if (tmp) {
        t0 = now_us();
        for (int i = 0; i < iters; ++i) {
            memcpy(tmp, ctx->fb.pixels, bytes);
        }
        t1 = now_us();
        ms = (double)(t1 - t0) / iters / 1000.0;
        mbps = (bytes / (ms / 1000.0)) / (1024.0 * 1024.0);
        printf("memcpy FB->PSRAM     %.3f ms  %.1f MB/s\n", ms, mbps);
        printf("JSONL {\"test\":\"memory\",\"op\":\"memcpy_out\",\"ms\":%.3f,\"mbps\":%.1f,"
               "\"bytes\":%u}\n",
               ms, mbps, (unsigned)bytes);

        t0 = now_us();
        for (int i = 0; i < iters; ++i) {
            memcpy(ctx->fb.pixels, tmp, bytes);
        }
        t1 = now_us();
        ms = (double)(t1 - t0) / iters / 1000.0;
        mbps = (bytes / (ms / 1000.0)) / (1024.0 * 1024.0);
        printf("memcpy PSRAM->FB     %.3f ms  %.1f MB/s\n", ms, mbps);
        printf("JSONL {\"test\":\"memory\",\"op\":\"memcpy_in\",\"ms\":%.3f,\"mbps\":%.1f,"
               "\"bytes\":%u}\n",
               ms, mbps, (unsigned)bytes);
        free(tmp);
    }

    t0 = now_us();
    for (int i = 0; i < iters; ++i) {
        fb_fill_rect(&ctx->fb, 0, 0, LCD_H_RES, LCD_V_RES, COL_BLUE);
    }
    t1 = now_us();
    ms = (double)(t1 - t0) / iters / 1000.0;
    mbps = (bytes / (ms / 1000.0)) / (1024.0 * 1024.0);
    printf("fb_fill_rect full    %.3f ms  %.1f MB/s\n", ms, mbps);
    printf("JSONL {\"test\":\"memory\",\"op\":\"fill_rect_full\",\"ms\":%.3f,\"mbps\":%.1f,"
           "\"bytes\":%u}\n",
           ms, mbps, (unsigned)bytes);

    t0 = now_us();
    for (int i = 0; i < iters; ++i) {
        fb_fill_rect(&ctx->fb, 100, 80, 280, 160, COL_RED);
    }
    t1 = now_us();
    ms = (double)(t1 - t0) / iters / 1000.0;
    size_t rbytes = 280 * 160 * 2;
    mbps = (rbytes / (ms / 1000.0)) / (1024.0 * 1024.0);
    printf("fb_fill_rect 280x160 %.3f ms  %.1f MB/s\n", ms, mbps);
    printf("JSONL {\"test\":\"memory\",\"op\":\"fill_rect_partial\",\"ms\":%.3f,\"mbps\":%.1f,"
           "\"bytes\":%u}\n",
           ms, mbps, (unsigned)rbytes);

    printf("=== END MEMORY BENCH ===\n\n");
}
