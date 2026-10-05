#include "bench_common.h"
#include "bench_fb.h"
#include "bench_gfx.h"

#include <stdio.h>

#include "esp_heap_caps.h"
#include "esp_timer.h"

void overlay_update(bench_ctx_t *ctx)
{
    if (!ctx->cfg.overlay_on) {
        return;
    }
    int64_t now = esp_timer_get_time();
    if (now - ctx->overlay_last_us < 250000) {
        return; /* ~4 Hz */
    }
    ctx->overlay_last_us = now;

    const metrics_t *m = &ctx->metrics;
    float fps = m->last.fps;
    float frame_ms = m->last.total_us / 1000.0f;
    float sim = m->last.sim_us / 1000.0f;
    float ren = m->last.render_us / 1000.0f;
    float lcd = m->last.lcd_us / 1000.0f;
    int dirty_pct = 0;
    if (m->last.dirty_px > 0) {
        dirty_pct = (m->last.dirty_px * 100) / (LCD_H_RES * LCD_V_RES);
    }

    ctx->overlay_n = 0;
    snprintf(ctx->overlay_lines[ctx->overlay_n++], 40, "P4 BENCH - %.8s", ctx->cfg.active_test);
    snprintf(ctx->overlay_lines[ctx->overlay_n++], 40, "FPS %.1f", fps);
    snprintf(ctx->overlay_lines[ctx->overlay_n++], 40, "FRM %.1fms", frame_ms);
    snprintf(ctx->overlay_lines[ctx->overlay_n++], 40, "SIM %.1f REN %.1f", sim, ren);
    snprintf(ctx->overlay_lines[ctx->overlay_n++], 40, "LCD %.1fms", lcd);
    snprintf(ctx->overlay_lines[ctx->overlay_n++], 40, "DRT %d%%", dirty_pct);
    snprintf(ctx->overlay_lines[ctx->overlay_n++], 40, "H %uK",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));
    snprintf(ctx->overlay_lines[ctx->overlay_n++], 40, "P %uK",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
}

void overlay_blit(bench_ctx_t *ctx)
{
    if (!ctx->cfg.overlay_on || ctx->overlay_n <= 0 || !ctx->fb.pixels) {
        return;
    }
    const int scale = 1;
    const int line_h = 8 * scale + 2;
    int box_h = ctx->overlay_n * line_h + 4;
    int box_w = 18 * 8 * scale + 8;
    fb_fill_rect(&ctx->fb, 2, 2, box_w, box_h, COL_DKGRAY);
    for (int i = 0; i < ctx->overlay_n; ++i) {
        gfx_draw_string(&ctx->fb, 6, 4 + i * line_h, ctx->overlay_lines[i], COL_YELLOW, COL_DKGRAY,
                        scale);
    }
    fb_dirty_add(&ctx->fb, 2, 2, box_w, box_h);
}
