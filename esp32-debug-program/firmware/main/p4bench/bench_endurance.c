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

/*
 * 60 MHz SPI endurance stress.
 * Operator watches for: pixel corruption, shifted lines, lost commands,
 * wrong colors, panel lockups. Firmware tracks TX failures / short blits.
 */

typedef enum {
    PAT_ALT_PIXELS = 0,
    PAT_CHECKER8,
    PAT_CHECKER1,
    PAT_VLINE1,
    PAT_HLINE1,
    PAT_PRNG,
    PAT_COLOR_BARS,
    PAT_SOLID_CYCLE,
    PAT_COUNT
} endo_pat_t;

static int64_t now_us(void)
{
    return esp_timer_get_time();
}

static uint32_t xorshift(uint32_t *s)
{
    uint32_t x = *s ? *s : 1u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

static const char *pat_name(endo_pat_t p)
{
    switch (p) {
    case PAT_ALT_PIXELS:
        return "alt_pixels";
    case PAT_CHECKER8:
        return "checker8";
    case PAT_CHECKER1:
        return "checker1";
    case PAT_VLINE1:
        return "vline1";
    case PAT_HLINE1:
        return "hline1";
    case PAT_PRNG:
        return "prng";
    case PAT_COLOR_BARS:
        return "color_bars";
    case PAT_SOLID_CYCLE:
        return "solid_cycle";
    default:
        return "?";
    }
}

static void fill_pattern(framebuffer_t *fb, endo_pat_t pat, uint32_t frame, uint32_t *rng)
{
    static const uint16_t solids[] = {COL_RED, COL_GREEN, COL_BLUE, COL_WHITE,
                                      COL_BLACK, COL_YELLOW, COL_CYAN, COL_MAGENTA};
    static const uint16_t bars[] = {COL_WHITE, COL_YELLOW, COL_CYAN, COL_GREEN,
                                    COL_MAGENTA, COL_RED, COL_BLUE, COL_BLACK};

    switch (pat) {
    case PAT_ALT_PIXELS: {
        uint16_t a = (frame & 1) ? COL_WHITE : COL_BLACK;
        uint16_t b = (frame & 1) ? COL_BLACK : COL_WHITE;
        for (int y = 0; y < fb->h; ++y) {
            for (int x = 0; x < fb->w; ++x) {
                fb->pixels[y * fb->w + x] = ((x ^ y) & 1) ? a : b;
            }
        }
        break;
    }
    case PAT_CHECKER8:
        for (int y = 0; y < fb->h; ++y) {
            for (int x = 0; x < fb->w; ++x) {
                fb->pixels[y * fb->w + x] =
                    (((x >> 3) ^ (y >> 3)) & 1) ? COL_WHITE : COL_BLACK;
            }
        }
        break;
    case PAT_CHECKER1:
        for (int y = 0; y < fb->h; ++y) {
            for (int x = 0; x < fb->w; ++x) {
                fb->pixels[y * fb->w + x] = ((x ^ y) & 1) ? COL_WHITE : COL_BLACK;
            }
        }
        break;
    case PAT_VLINE1:
        fb_clear(fb, COL_BLACK);
        for (int y = 0; y < fb->h; ++y) {
            for (int x = 0; x < fb->w; x += 2) {
                fb->pixels[y * fb->w + x] = COL_WHITE;
            }
        }
        break;
    case PAT_HLINE1:
        fb_clear(fb, COL_BLACK);
        for (int y = 0; y < fb->h; y += 2) {
            for (int x = 0; x < fb->w; ++x) {
                fb->pixels[y * fb->w + x] = COL_WHITE;
            }
        }
        break;
    case PAT_PRNG:
        for (int i = 0; i < fb->w * fb->h; ++i) {
            fb->pixels[i] = (uint16_t)(xorshift(rng) & 0xFFFF);
        }
        break;
    case PAT_COLOR_BARS: {
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
        break;
    }
    case PAT_SOLID_CYCLE:
    default:
        fb_clear(fb, solids[frame % 8]);
        break;
    }
}

static void draw_status_hud(framebuffer_t *fb, endo_pat_t pat, uint32_t frame, int elapsed_s,
                            int remain_s, uint32_t fails)
{
    char l1[40];
    char l2[40];
    snprintf(l1, sizeof(l1), "END %s", pat_name(pat));
    snprintf(l2, sizeof(l2), "F%u T%d/%ds E%u", (unsigned)frame, elapsed_s, elapsed_s + remain_s,
             (unsigned)fails);
    fb_fill_rect(fb, 4, 4, 360, 44, COL_DKGRAY);
    gfx_draw_string(fb, 8, 8, l1, COL_YELLOW, COL_DKGRAY, 2);
    gfx_draw_string(fb, 8, 28, l2, COL_WHITE, COL_DKGRAY, 1);
    /* Corner markers — shifted window shows up immediately */
    fb_fill_rect(fb, 0, 0, 8, 8, COL_RED);
    fb_fill_rect(fb, fb->w - 8, 0, 8, 8, COL_GREEN);
    fb_fill_rect(fb, 0, fb->h - 8, 8, 8, COL_BLUE);
    fb_fill_rect(fb, fb->w - 8, fb->h - 8, 8, 8, COL_YELLOW);
}

static bool blit_full(bench_ctx_t *ctx, size_t *out_bytes)
{
    size_t expect = lcd_hw_rect_wire_bytes(&ctx->lcd, LCD_H_RES, LCD_V_RES);
    size_t got = lcd_hw_blit_rgb565(&ctx->lcd, 0, 0, LCD_H_RES, LCD_V_RES, ctx->fb.pixels,
                                    LCD_H_RES, ctx->cfg.chunk_rows > 0 ? ctx->cfg.chunk_rows : 16);
    if (out_bytes) {
        *out_bytes = got;
    }
    return got == expect;
}

static int poll_quit(void)
{
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == 'q' || c == 'Q') {
            return 1;
        }
    }
    return 0;
}

void bench_run_endurance(bench_ctx_t *ctx, int minutes)
{
    if (minutes < 1) {
        minutes = 1;
    }
    if (minutes > 180) {
        minutes = 180;
    }

    ctx->cfg.running = false;
    ctx->cfg.frames_paused = true;
    ctx->cfg.overlay_on = false;
    if (ctx->test && ctx->test->deinit) {
        ctx->test->deinit(ctx);
        ctx->test = NULL;
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }

    int spi_mhz = (ctx->lcd.spi_hz_actual + 500000) / 1000000;
    printf("\n=== SPI ENDURANCE ===\n");
    printf("spi_actual=%d Hz (~%d MHz)  duration=%d min  chunk_rows=%d\n", ctx->lcd.spi_hz_actual,
           spi_mhz, minutes, ctx->cfg.chunk_rows);
    printf("Patterns: alt_pixels checker8 checker1 vline1 hline1 prng color_bars solid_cycle\n");
    printf("Watch for: corruption, shifted lines, wrong colors, lockups, sparkle on glyphs.\n");
    printf("Type q to abort early.\n");
    printf("JSONL {\"test\":\"endurance_start\",\"spi_hz\":%d,\"minutes\":%d}\n",
           ctx->lcd.spi_hz_actual, minutes);
    fflush(stdout);

    uint32_t fails0 = ctx->lcd.xfer_fails;
    uint32_t short_blits = 0;
    uint32_t frames = 0;
    uint64_t bytes_total = 0;
    uint32_t rng = 0x60F1A57Eu;
    endo_pat_t pat = PAT_ALT_PIXELS;
    int64_t t_start = now_us();
    int64_t t_end = t_start + (int64_t)minutes * 60LL * 1000000LL;
    int64_t t_next_hb = t_start + 10000000;
    int64_t t_next_pat = t_start + 8000000; /* change pattern every 8 s */
    int aborted = 0;
    int locked = 0;

    while (now_us() < t_end) {
        if (poll_quit()) {
            aborted = 1;
            printf("endurance: abort requested\n");
            break;
        }

        int64_t t_now = now_us();
        if (t_now >= t_next_pat) {
            pat = (endo_pat_t)(((int)pat + 1) % PAT_COUNT);
            t_next_pat = t_now + 8000000;
            /* Deterministic reseed when entering PRNG so repeats are comparable */
            if (pat == PAT_PRNG) {
                rng = 0x60F1A57Eu ^ (uint32_t)frames;
            }
        }

        fill_pattern(&ctx->fb, pat, frames, &rng);
        int elapsed_s = (int)((t_now - t_start) / 1000000);
        int remain_s = (int)((t_end - t_now) / 1000000);
        if (remain_s < 0) {
            remain_s = 0;
        }
        draw_status_hud(&ctx->fb, pat, frames, elapsed_s, remain_s,
                        ctx->lcd.xfer_fails - fails0);

        size_t got = 0;
        int64_t t0 = now_us();
        bool ok = blit_full(ctx, &got);
        int64_t blit_us = now_us() - t0;
        if (!ok) {
            short_blits++;
        }
        if (blit_us > 2000000) {
            /* >2 s for one frame at 60 MHz ≈ panel/SPI lockup */
            locked++;
            printf("endurance: WARN slow_blit_us=%lld fails=%u\n", (long long)blit_us,
                   (unsigned)(ctx->lcd.xfer_fails - fails0));
        }
        bytes_total += got;
        frames++;

        if (now_us() >= t_next_hb) {
            uint32_t fails = ctx->lcd.xfer_fails - fails0;
            double mb = bytes_total / (1024.0 * 1024.0);
            printf("HB t=%ds remain=%ds frames=%u pat=%s fails=%u short=%u slow=%u bytes_MB=%.1f "
                   "last_blit_ms=%.1f\n",
                   elapsed_s, remain_s, (unsigned)frames, pat_name(pat), (unsigned)fails,
                   (unsigned)short_blits, (unsigned)locked, mb, blit_us / 1000.0);
            printf("JSONL {\"test\":\"endurance_hb\",\"t_s\":%d,\"frames\":%u,\"pattern\":\"%s\","
                   "\"xfer_fails\":%u,\"short_blits\":%u,\"slow_blits\":%u,\"bytes\":%llu}\n",
                   elapsed_s, (unsigned)frames, pat_name(pat), (unsigned)fails,
                   (unsigned)short_blits, (unsigned)locked,
                   (unsigned long long)bytes_total);
            fflush(stdout);
            t_next_hb = now_us() + 10000000;

            /* Hard fail: many TX errors or repeated lockups */
            if (fails >= 50 || locked >= 10) {
                printf("endurance: FAIL threshold (fails=%u slow=%u)\n", (unsigned)fails,
                       (unsigned)locked);
                break;
            }
        }
    }

    uint32_t fails = ctx->lcd.xfer_fails - fails0;
    int elapsed_s = (int)((now_us() - t_start) / 1000000);
    int pass = (!aborted && fails == 0 && short_blits == 0 && locked == 0);

    /* Final solid proof — operator must see GREEN "PASS" or RED "FAIL" */
    uint16_t prove = pass ? COL_GREEN : COL_RED;
    lcd_hw_fill_screen(&ctx->lcd, prove);
    fb_clear(&ctx->fb, prove);
    gfx_draw_string(&ctx->fb, 40, 120, pass ? "PASS" : "FAIL", COL_BLACK, prove, 4);
    blit_full(ctx, NULL);
    vTaskDelay(pdMS_TO_TICKS(1500));

    printf("\n=== ENDURANCE RESULT ===\n");
    printf("elapsed_s=%d  frames=%u  xfer_fails=%u  short_blits=%u  slow_blits=%u  aborted=%d\n",
           elapsed_s, (unsigned)frames, (unsigned)fails, (unsigned)short_blits, (unsigned)locked,
           aborted);
    printf("bytes=%llu  spi_hz=%d\n", (unsigned long long)bytes_total, ctx->lcd.spi_hz_actual);
    printf("verdict: %s\n", pass ? "PASS" : (aborted ? "ABORT" : "FAIL"));
    printf("JSONL {\"test\":\"endurance_done\",\"ok\":%s,\"elapsed_s\":%d,\"frames\":%u,"
           "\"xfer_fails\":%u,\"short_blits\":%u,\"slow_blits\":%u,\"aborted\":%s,"
           "\"spi_hz\":%d,\"bytes\":%llu}\n",
           pass ? "true" : "false", elapsed_s, (unsigned)frames, (unsigned)fails,
           (unsigned)short_blits, (unsigned)locked, aborted ? "true" : "false",
           ctx->lcd.spi_hz_actual, (unsigned long long)bytes_total);
    printf("<<< {\"ok\":%s,\"cmd\":\"endurance\",\"xfer_fails\":%u,\"frames\":%u,"
           "\"elapsed_s\":%d,\"spi_hz\":%d}\n",
           pass ? "true" : "false", (unsigned)fails, (unsigned)frames, elapsed_s,
           ctx->lcd.spi_hz_actual);
    fflush(stdout);

    ctx->cfg.frames_paused = false;
}
