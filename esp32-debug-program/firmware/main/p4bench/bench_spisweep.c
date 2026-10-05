#include "bench_tests.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bench_fb.h"
#include "bench_gfx.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef enum {
    VIS_STABLE = 0,
    VIS_SUSPECT,
    VIS_UNSTABLE,
} vis_status_t;

static const char *vis_name(vis_status_t s)
{
    switch (s) {
    case VIS_STABLE:
        return "STABLE";
    case VIS_SUSPECT:
        return "SUSPECT";
    default:
        return "UNSTABLE";
    }
}

static int64_t now_us(void)
{
    return esp_timer_get_time();
}

static void fill_checker1(framebuffer_t *fb)
{
    for (int y = 0; y < fb->h; ++y) {
        for (int x = 0; x < fb->w; ++x) {
            fb->pixels[y * fb->w + x] = ((x ^ y) & 1) ? COL_WHITE : COL_BLACK;
        }
    }
}

static void fill_vstripes1(framebuffer_t *fb)
{
    for (int y = 0; y < fb->h; ++y) {
        for (int x = 0; x < fb->w; ++x) {
            fb->pixels[y * fb->w + x] = (x & 1) ? COL_WHITE : COL_BLACK;
        }
    }
}

static void fill_hstripes1(framebuffer_t *fb)
{
    for (int y = 0; y < fb->h; ++y) {
        uint16_t c = (y & 1) ? COL_WHITE : COL_BLACK;
        for (int x = 0; x < fb->w; ++x) {
            fb->pixels[y * fb->w + x] = c;
        }
    }
}

static void fill_color_bars(framebuffer_t *fb)
{
    static const uint16_t bars[] = {COL_WHITE, COL_YELLOW, COL_CYAN, COL_GREEN,
                                    COL_MAGENTA, COL_RED, COL_BLUE, COL_BLACK};
    int bw = fb->w / 8;
    for (int y = 0; y < fb->h; ++y) {
        for (int x = 0; x < fb->w; ++x) {
            int bi = x / bw;
            if (bi > 7) {
                bi = 7;
            }
            fb->pixels[y * fb->w + x] = bars[bi];
        }
    }
}

static void fill_grid(framebuffer_t *fb)
{
    for (int y = 0; y < fb->h; ++y) {
        for (int x = 0; x < fb->w; ++x) {
            uint16_t c = COL_DKGRAY;
            if (x % 32 == 0 || y % 32 == 0) {
                c = COL_GRAY;
            }
            if (x == 0 || y == 0 || x == fb->w - 1 || y == fb->h - 1) {
                c = COL_YELLOW;
            }
            fb->pixels[y * fb->w + x] = c;
        }
    }
    fb_fill_rect(fb, 0, 0, 16, 16, COL_RED);
    fb_fill_rect(fb, fb->w - 16, 0, 16, 16, COL_GREEN);
    fb_fill_rect(fb, 0, fb->h - 16, 16, 16, COL_BLUE);
    fb_fill_rect(fb, fb->w - 16, fb->h - 16, 16, 16, COL_WHITE);
}

static void fill_prng(framebuffer_t *fb, uint32_t seed)
{
    uint32_t rng = seed ? seed : 1u;
    for (int i = 0; i < fb->w * fb->h; ++i) {
        fb->pixels[i] = (uint16_t)(bench_rand(&rng) & 0xFFFF);
    }
}

static bool blit_full(bench_ctx_t *ctx, int chunk_rows, size_t *out_bytes)
{
    size_t expect = lcd_hw_rect_wire_bytes(&ctx->lcd, LCD_H_RES, LCD_V_RES);
    size_t got = lcd_hw_blit_rgb565(&ctx->lcd, 0, 0, LCD_H_RES, LCD_V_RES, ctx->fb.pixels,
                                    LCD_H_RES, chunk_rows);
    if (out_bytes) {
        *out_bytes = got;
    }
    return got == expect;
}

/* Big HUD: SPI MHz + "LOOK" so you always know which clock you are judging. */
static void draw_mhz_hud(framebuffer_t *fb, int actual_hz, int frame)
{
    char line1[24];
    char line2[24];
    int mhz = (actual_hz + 500000) / 1000000;
    snprintf(line1, sizeof(line1), "SPI %d MHZ", mhz);
    snprintf(line2, sizeof(line2), (frame & 8) ? "LOOK OK?" : "WATCH ME");

    fb_fill_rect(fb, 8, 8, 280, 56, COL_BLACK);
    gfx_draw_string(fb, 16, 16, line1, COL_YELLOW, COL_BLACK, 2);
    gfx_draw_string(fb, 16, 40, line2, COL_WHITE, COL_BLACK, 2);
}

/*
 * One animated proof frame — easy to judge by eye:
 *  - scrolling rainbow background (tearing / missing lines show up)
 *  - bouncing yellow block (stutter / ghosting / stuck pixels)
 *  - corner traffic lights that flash
 *  - MHz label
 */
static void draw_proof_frame(framebuffer_t *fb, int actual_hz, int frame)
{
    static const uint16_t rainbow[] = {COL_RED,     COL_ORANGE, COL_YELLOW, COL_GREEN,
                                       COL_CYAN,    COL_BLUE,   COL_MAGENTA, COL_WHITE};
    int shift = frame * 3;
    for (int y = 0; y < fb->h; ++y) {
        for (int x = 0; x < fb->w; ++x) {
            int bi = ((x + shift) / 40) & 7;
            fb->pixels[y * fb->w + x] = rainbow[bi];
        }
    }

    /* 1-pixel black grid every 40 px — exposes missing/shifted lines */
    for (int x = 0; x < fb->w; x += 40) {
        for (int y = 0; y < fb->h; ++y) {
            fb->pixels[y * fb->w + x] = COL_BLACK;
        }
    }
    for (int y = 0; y < fb->h; y += 40) {
        for (int x = 0; x < fb->w; ++x) {
            fb->pixels[y * fb->w + x] = COL_BLACK;
        }
    }

    int bx = frame * 6;
    int by = frame * 4;
    int wmax = fb->w - 48;
    int hmax = fb->h - 48;
    if (wmax < 1) {
        wmax = 1;
    }
    if (hmax < 1) {
        hmax = 1;
    }
    int xcycle = bx % (2 * wmax);
    int ycycle = by % (2 * hmax);
    bx = (xcycle < wmax) ? xcycle : (2 * wmax - xcycle);
    by = (ycycle < hmax) ? ycycle : (2 * hmax - ycycle);
    fb_fill_rect(fb, bx, by, 48, 48, COL_YELLOW);
    fb_fill_rect(fb, bx + 8, by + 8, 32, 32, COL_BLACK);
    fb_fill_rect(fb, bx + 16, by + 16, 16, 16, COL_RED);

    /* flashing corner beacons */
    uint16_t flash = (frame & 4) ? COL_WHITE : COL_RED;
    fb_fill_rect(fb, 0, 0, 24, 24, flash);
    fb_fill_rect(fb, fb->w - 24, 0, 24, 24, (frame & 4) ? COL_GREEN : COL_BLACK);
    fb_fill_rect(fb, 0, fb->h - 24, 24, 24, (frame & 4) ? COL_BLUE : COL_BLACK);
    fb_fill_rect(fb, fb->w - 24, fb->h - 24, 24, 24, (frame & 4) ? COL_YELLOW : COL_BLACK);

    draw_mhz_hud(fb, actual_hz, frame);
}

static void measure_full_throughput(bench_ctx_t *ctx, int chunk_rows, int iters, float *frame_ms,
                                    float *fps, size_t *bytes, float *mbps)
{
    size_t b = 0;
    for (int w = 0; w < 2; ++w) {
        blit_full(ctx, chunk_rows, NULL);
    }
    int64_t sum = 0;
    for (int i = 0; i < iters; ++i) {
        int64_t t0 = now_us();
        blit_full(ctx, chunk_rows, &b);
        sum += now_us() - t0;
    }
    *bytes = b;
    *frame_ms = (float)((double)sum / iters / 1000.0);
    *fps = (*frame_ms > 0.0f) ? (1000.0f / *frame_ms) : 0.0f;
    *mbps = (*frame_ms > 0.0f)
                ? (float)((b / (*frame_ms / 1000.0)) / (1024.0 * 1024.0))
                : 0.0f;
}

static float measure_dirty_fps(bench_ctx_t *ctx, int pct, int seconds)
{
    ctx->cfg.dirty_pct = pct;
    ctx->cfg.render_mode = RENDER_MODE_DIRTY;
    ctx->cfg.overlay_on = false;
    if (ctx->test && ctx->test->deinit) {
        ctx->test->deinit(ctx);
    }
    ctx->test = &TEST_DIRTY;
    metrics_reset(&ctx->metrics);
    TEST_DIRTY.init(ctx);

    int64_t warm = now_us() + 500000;
    while (now_us() < warm) {
        frame_sample_t f;
        metrics_begin_frame(&ctx->metrics, &f);
        int64_t t0 = now_us();
        TEST_DIRTY.frame(ctx, &f);
        if (f.total_us <= 0) {
            f.total_us = now_us() - t0;
        }
        metrics_end_frame(&ctx->metrics, &f);
    }
    metrics_reset(&ctx->metrics);

    int64_t end = now_us() + (int64_t)seconds * 1000000;
    while (now_us() < end) {
        frame_sample_t f;
        metrics_begin_frame(&ctx->metrics, &f);
        int64_t t0 = now_us();
        TEST_DIRTY.frame(ctx, &f);
        if (f.total_us <= 0) {
            f.total_us = now_us() - t0;
        }
        metrics_end_frame(&ctx->metrics, &f);
    }
    float fps = metrics_avg_fps(&ctx->metrics);
    float lcd_ms =
        ctx->metrics.frames ? (float)(ctx->metrics.sum_lcd_us / ctx->metrics.frames / 1000.0) : 0;
    float bytes =
        ctx->metrics.frames ? (float)(ctx->metrics.sum_lcd_bytes / (double)ctx->metrics.frames) : 0;
    printf("  dirty %2d%%: fps=%.1f lcd=%.1fms bytes=%.0f\n", pct, fps, lcd_ms, bytes);
    printf("JSONL {\"test\":\"spi_dirty\",\"spi_hz\":%d,\"actual_hz\":%d,\"dirty_pct\":%d,"
           "\"fps\":%.2f,\"lcd_ms\":%.2f,\"bytes\":%.0f}\n",
           ctx->lcd.spi_hz, ctx->lcd.spi_hz_actual, pct, fps, lcd_ms, bytes);

    if (ctx->test && ctx->test->deinit) {
        ctx->test->deinit(ctx);
        ctx->test = NULL;
    }
    return fps;
}

/* Static patterns: one clear paint + hold. Re-blitting full frames @2.7 FPS looks like a
 * white blink and is useless for judging solid colors. */
static bool show_pattern_hold(bench_ctx_t *ctx, const char *name, int hold_ms, int chunk_rows)
{
    printf("    pattern %s (%d ms hold)\n", name, hold_ms);
    fflush(stdout);
    draw_mhz_hud(&ctx->fb, ctx->lcd.spi_hz_actual, 0);
    if (!blit_full(ctx, chunk_rows, NULL)) {
        printf("    !!! blit failed for %s\n", name);
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(hold_ms));
    return true;
}

static bool visual_static_torture(bench_ctx_t *ctx, int hold_ms, int chunk_rows)
{
    const uint16_t solids[] = {COL_RED, COL_GREEN, COL_BLUE, COL_WHITE, COL_BLACK};
    const char *solid_names[] = {"solid_red", "solid_green", "solid_blue", "solid_white",
                                 "solid_black"};

    /* Solids via direct SPI fill (same path as color proof) — unmistakable. */
    for (int i = 0; i < 5; ++i) {
        printf("    pattern %s (direct fill, %d ms)\n", solid_names[i], hold_ms);
        fflush(stdout);
        if (!lcd_hw_fill_screen(&ctx->lcd, solids[i])) {
            printf("    !!! fill_screen failed\n");
            return false;
        }
        /* Label on top via FB so you can read the name */
        fb_clear(&ctx->fb, solids[i]);
        uint16_t fg = (solids[i] == COL_BLACK || solids[i] == COL_BLUE) ? COL_WHITE : COL_BLACK;
        gfx_draw_string(&ctx->fb, 24, 120, solid_names[i], fg, solids[i], 3);
        draw_mhz_hud(&ctx->fb, ctx->lcd.spi_hz_actual, 0);
        if (!blit_full(ctx, chunk_rows, NULL)) {
            printf("    !!! label blit failed (solid may still be OK)\n");
        }
        vTaskDelay(pdMS_TO_TICKS(hold_ms));
    }

    fill_checker1(&ctx->fb);
    if (!show_pattern_hold(ctx, "checker1", hold_ms, chunk_rows)) {
        return false;
    }
    fill_vstripes1(&ctx->fb);
    if (!show_pattern_hold(ctx, "vstripe1", hold_ms, chunk_rows)) {
        return false;
    }
    fill_hstripes1(&ctx->fb);
    if (!show_pattern_hold(ctx, "hstripe1", hold_ms, chunk_rows)) {
        return false;
    }
    fill_color_bars(&ctx->fb);
    if (!show_pattern_hold(ctx, "color_bars", hold_ms, chunk_rows)) {
        return false;
    }
    fill_grid(&ctx->fb);
    if (!show_pattern_hold(ctx, "grid", hold_ms, chunk_rows)) {
        return false;
    }
    fill_prng(&ctx->fb, 0xA5A5A5A5u);
    if (!show_pattern_hold(ctx, "prng", hold_ms, chunk_rows)) {
        return false;
    }

    /* Explicit blink: black/white with holds — not a 2.7 FPS redraw race. */
    printf("    pattern blink_bw\n");
    fflush(stdout);
    for (int i = 0; i < 4; ++i) {
        if (!lcd_hw_fill_screen(&ctx->lcd, (i & 1) ? COL_WHITE : COL_BLACK)) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(hold_ms / 2));
    }
    return true;
}

static bool show_solid_direct(bench_ctx_t *ctx, uint16_t color, const char *name, int hold_ms)
{
    printf("  >>> LCD must show %s now (%d ms)\n", name, hold_ms);
    fflush(stdout);
    if (!lcd_hw_fill_screen(&ctx->lcd, color)) {
        printf("  !!! fill_screen(%s) FAILED\n", name);
        return false;
    }
    /* Big label via FB path after solid — if this fails, solid still visible */
    fb_clear(&ctx->fb, color);
    uint16_t fg = (color == COL_BLACK || color == COL_BLUE) ? COL_WHITE : COL_BLACK;
    gfx_draw_string(&ctx->fb, 40, 120, name, fg, color, 3);
    char mhz[20];
    snprintf(mhz, sizeof(mhz), "%d MHZ", (ctx->lcd.spi_hz_actual + 500000) / 1000000);
    gfx_draw_string(&ctx->fb, 40, 180, mhz, fg, color, 3);
    if (!blit_full(ctx, 16, NULL)) {
        printf("  !!! FB blit label failed (solid fill may still be OK)\n");
        /* keep going — solid already on glass */
    }
    vTaskDelay(pdMS_TO_TICKS(hold_ms));
    return true;
}

static bool visual_color_proof(bench_ctx_t *ctx)
{
    printf("  COLOR PROOF (direct SPI fills — must not stay white)\n");
    if (!show_solid_direct(ctx, COL_RED, "RED", 900)) {
        return false;
    }
    if (!show_solid_direct(ctx, COL_GREEN, "GREEN", 900)) {
        return false;
    }
    if (!show_solid_direct(ctx, COL_BLUE, "BLUE", 900)) {
        return false;
    }
    if (!show_solid_direct(ctx, COL_YELLOW, "YELLOW", 700)) {
        return false;
    }
    if (!show_solid_direct(ctx, COL_BLACK, "BLACK", 500)) {
        return false;
    }
    return true;
}

/*
 * Animated proof the operator watches. Runs for ~duration_ms of wall time
 * (many frames). Corruption → torn rainbow, frozen ball, wrong MHz text, etc.
 */
static bool visual_animated_proof(bench_ctx_t *ctx, int duration_ms, int chunk_rows)
{
    printf("  ANIMATION: rainbow scroll + bouncing block + MHz label (%d ms)\n", duration_ms);
    printf("  Watch the panel — motion should be smooth, colors clean, text readable.\n");
    fflush(stdout);

    int64_t end = now_us() + (int64_t)duration_ms * 1000;
    int frame = 0;
    while (now_us() < end) {
        draw_proof_frame(&ctx->fb, ctx->lcd.spi_hz_actual, frame++);
        if (!blit_full(ctx, chunk_rows, NULL)) {
            printf("  TX FAIL during animation frame %d\n", frame);
            return false;
        }
    }
    printf("  animation frames=%d\n", frame);
    return true;
}

static vis_status_t ask_visual_status(bench_ctx_t *ctx, bool auto_classify, bool tx_ok)
{
    if (auto_classify) {
        if (!tx_ok) {
            return VIS_UNSTABLE;
        }
        printf("  visual_status auto=TX_OK (you must still confirm by eye)\n");
        return VIS_STABLE;
    }

    printf("\n  === LOOK AT THE SCREEN ===\n");
    printf("  You should see: scrolling rainbow, bouncing yellow/red block,\n");
    printf("  flashing corner squares, and yellow 'SPI xx MHZ' text.\n");
    printf("  Bad clock → tear, sparkle, wrong colors, frozen image, garbled text.\n");
    printf("  Type:  s=STABLE  p=SUSPECT  u=UNSTABLE   (anim keeps playing)\n> ");
    fflush(stdout);

    int frame = 0;
    int64_t deadline = now_us() + 60000000;
    while (now_us() < deadline) {
        /* Keep animating while waiting so you can judge live. */
        draw_proof_frame(&ctx->fb, ctx->lcd.spi_hz_actual, frame++);
        blit_full(ctx, 16, NULL);

        /* Non-blocking: getchar() on UART console blocks and freezes animation. */
        int c = -1;
        {
            int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
            if (flags >= 0) {
                fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
            }
            unsigned char ch;
            if (read(STDIN_FILENO, &ch, 1) == 1) {
                c = (int)ch;
            }
        }
        if (c < 0) {
            continue;
        }
        c = tolower(c);
        if (c == 's') {
            printf("s\n");
            return VIS_STABLE;
        }
        if (c == 'p') {
            printf("p\n");
            return VIS_SUSPECT;
        }
        if (c == 'u') {
            printf("u\n");
            return VIS_UNSTABLE;
        }
    }
    printf("\n  (timeout → SUSPECT)\n");
    return VIS_SUSPECT;
}

static bool apply_clock(bench_ctx_t *ctx, int hz)
{
    printf("  switching SPI to %d Hz...\n", hz);
    fflush(stdout);
    if (lcd_hw_reinit_at_hz(&ctx->lcd, hz)) {
        ctx->cfg.spi_hz = hz;
        printf("  actual=%d Hz — screen should be solid GREEN\n", ctx->lcd.spi_hz_actual);
        return true;
    }
    printf("  reclock FAILED at %d Hz\n", hz);
    return false;
}

static bool verify_panel_alive(bench_ctx_t *ctx)
{
    return show_solid_direct(ctx, COL_GREEN, "PANEL OK", 400);
}

void bench_run_spisweep(bench_ctx_t *ctx, bool auto_classify)
{
    static const int freqs[] = {
        10000000, 16000000, 20000000, 26666667, 32000000, 40000000, 48000000, 60000000, 80000000, 0,
    };

    printf("\n=== SPI CLOCK SWEEP ===\n");
    printf("ILI9488 datasheet max write: 20 MHz. Higher = overclock.\n");
    printf("After each clock you will see an ANIMATION on the LCD.\n");
    printf("Judge that animation, then type s/p/u on serial.\n");
    printf("Mode: %s\n\n", auto_classify ? "auto (TX only — still watch the panel)"
                                           : "interactive");

    ctx->cfg.running = false;
    ctx->cfg.frames_paused = true; /* stop frame task from fighting SPI during sweep */
    ctx->cfg.overlay_on = false;
    if (ctx->test && ctx->test->deinit) {
        ctx->test->deinit(ctx);
        ctx->test = NULL;
    }
    vTaskDelay(pdMS_TO_TICKS(50)); /* let frame task notice pause */

    int known_good_hz = LCD_SPI_HZ_DEFAULT;
    int max_stable_hz = 0;
    char summary[16][96];
    int summary_n = 0;

    if (!apply_clock(ctx, LCD_SPI_HZ_DEFAULT) || !verify_panel_alive(ctx)) {
        printf("FATAL: cannot restore %d Hz known-good\n", LCD_SPI_HZ_DEFAULT);
        ctx->cfg.frames_paused = false;
        return;
    }

    const int pattern_hold_ms = auto_classify ? 500 : 900;
    const int thru_iters = 8;
    const int anim_ms = auto_classify ? 2000 : 3500;

    for (int fi = 0; freqs[fi] > 0; ++fi) {
        int req = freqs[fi];
        bool overclock = req > 20000000;
        printf("----------------------------------------------------------------\n");
        printf("FREQ requested=%d Hz%s\n", req, overclock ? "  [OVERCLOCK >20 MHz]" : "");

        uint32_t fails0 = ctx->lcd.xfer_fails;
        if (!apply_clock(ctx, req)) {
            printf("  FAIL: could not set SPI clock\n");
            snprintf(summary[summary_n++], sizeof(summary[0]), "%8.2f MHz  UNSTABLE   (set failed)",
                     req / 1e6);
            apply_clock(ctx, known_good_hz);
            verify_panel_alive(ctx);
            continue;
        }
        int actual = ctx->lcd.spi_hz_actual;

        /* Immediate visual — before any long benchmark */
        float ms16 = 0, fps16 = 0, mbps16 = 0, ms1 = 0, fps1 = 0, mbps1 = 0;
        size_t bytes16 = 0, bytes1 = 0;
        bool tx_ok = visual_color_proof(ctx);
        if (tx_ok) {
            tx_ok = visual_animated_proof(ctx, anim_ms, 16);
        }

        if (tx_ok) {
            fill_prng(&ctx->fb, 0x12345678u);
            measure_full_throughput(ctx, 16, thru_iters, &ms16, &fps16, &bytes16, &mbps16);
            measure_full_throughput(ctx, 1, 4, &ms1, &fps1, &bytes1, &mbps1);
            printf("  requested_spi_hz: %d\n", req);
            printf("  actual_spi_hz:    %d\n", actual);
            printf("  full@16row:  frame_ms=%.2f  FPS=%.2f  wire_bytes=%u  MB/s=%.2f\n", ms16, fps16,
                   (unsigned)bytes16, mbps16);
            printf("  full@1row:   frame_ms=%.2f  FPS=%.2f  wire_bytes=%u  MB/s=%.2f\n", ms1, fps1,
                   (unsigned)bytes1, mbps1);
            printf("  dirty partials:\n");
            measure_dirty_fps(ctx, 1, 1);
            measure_dirty_fps(ctx, 5, 1);
            measure_dirty_fps(ctx, 10, 1);
            measure_dirty_fps(ctx, 20, 1);
            tx_ok = visual_static_torture(ctx, pattern_hold_ms, 16);
        } else {
            printf("  skipping throughput — color proof failed (white/dead panel)\n");
        }

        if (ctx->lcd.xfer_fails != fails0) {
            tx_ok = false;
        }

        if (tx_ok && !auto_classify) {
            visual_animated_proof(ctx, 1200, 16);
        }

        vis_status_t st = ask_visual_status(ctx, auto_classify, tx_ok);
        if (auto_classify && overclock && st == VIS_STABLE) {
            st = VIS_SUSPECT;
            printf("  note: auto marked OVERCLOCK as SUSPECT until eyes confirm STABLE\n");
        }
        if (!tx_ok) {
            st = VIS_UNSTABLE;
        }

        const char *notes = "-";
        if (!tx_ok) {
            notes = "SPI/tx failure during visual";
        } else if (overclock && st == VIS_STABLE) {
            notes = "overclock; operator confirmed STABLE";
        } else if (overclock) {
            notes = "experimental >20 MHz datasheet max";
        } else if (st == VIS_STABLE) {
            notes = "within datasheet; visual OK";
        }

        printf("  visual_status: %s\n", vis_name(st));
        printf("  notes: %s\n", notes);
        printf("JSONL {\"test\":\"spi_sweep\",\"requested_hz\":%d,\"actual_hz\":%d,"
               "\"frame_ms\":%.3f,\"fps\":%.2f,\"bytes\":%u,\"mbps\":%.2f,"
               "\"frame_ms_1row\":%.3f,\"visual\":\"%s\",\"overclock\":%s}\n",
               req, actual, ms16, fps16, (unsigned)bytes16, mbps16, ms1, vis_name(st),
               overclock ? "true" : "false");

        snprintf(summary[summary_n++], sizeof(summary[0]), "%8.2f MHz  %-9s  %.1f FPS",
                 actual / 1e6, vis_name(st), fps16);

        if (st == VIS_STABLE) {
            known_good_hz = req;
            if (req > max_stable_hz) {
                max_stable_hz = req;
            }
        } else if (st == VIS_UNSTABLE) {
            printf("  restoring known-good %d Hz — watch for PANEL OK...\n", known_good_hz);
            if (!apply_clock(ctx, known_good_hz) || !verify_panel_alive(ctx)) {
                printf("  CRITICAL: recover failed — forcing %d Hz\n", LCD_SPI_HZ_DEFAULT);
                apply_clock(ctx, LCD_SPI_HZ_DEFAULT);
                verify_panel_alive(ctx);
                known_good_hz = LCD_SPI_HZ_DEFAULT;
            } else {
                printf("  recovered at %d Hz (green PANEL OK)\n", known_good_hz);
            }
            vTaskDelay(pdMS_TO_TICKS(800));
        }
    }

    apply_clock(ctx, LCD_SPI_HZ_DEFAULT);
    verify_panel_alive(ctx);

    printf("\nSPI CLOCK SWEEP\n\n");
    for (int i = 0; i < summary_n; ++i) {
        printf("%s\n", summary[i]);
    }
    printf("\nMAX VERIFIED STABLE: ");
    if (max_stable_hz > 0) {
        printf("%.2f MHz\n", max_stable_hz / 1e6);
    } else {
        printf("(none confirmed STABLE)\n");
    }
    printf("Restored everyday clock: %.2f MHz\n", LCD_SPI_HZ_DEFAULT / 1e6);
    printf("=== END SPI CLOCK SWEEP ===\n\n");

    ctx->cfg.overlay_on = true;
    ctx->cfg.frames_paused = false;
}
