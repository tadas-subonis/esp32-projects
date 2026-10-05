#include "bench_common.h"
#include "bench_fb.h"
#include "bench_tests.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void trim(char *s)
{
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) {
        *--e = 0;
    }
    char *p = s;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    if (p != s) {
        memmove(s, p, strlen(p) + 1);
    }
}

/* Drop UART boot junk / UTF-8 garbage so "help" is never "\xFFhelp". */
static void sanitize_cmd_line(char *s)
{
    char *d = s;
    for (char *p = s; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c >= 0x20 && c <= 0x7E) {
            *d++ = (char)c;
        } else if (c == '\t') {
            *d++ = ' ';
        }
    }
    *d = 0;
    trim(s);
}

static void cmd_help(void)
{
    printf(
        "\nP4Bench commands:\n"
        "  help                     this text\n"
        "  status                   current state\n"
        "  list                     list tests\n"
        "  meta                     print hardware metadata\n"
        "  run <test>               start test (pattern/lcd/memory/sprites/...)\n"
        "  stop                     stop current test\n"
        "  report                   print metrics for current/last run\n"
        "  reset                    reset metrics\n"
        "  set sprites N\n"
        "  set particles N\n"
        "  set enemies N\n"
        "  set towers N\n"
        "  set projectiles N\n"
        "  set sprite_w N / sprite_h N\n"
        "  set sprite_mode N        0=opaque 1=key 2=alpha 3=scale 4=rot\n"
        "  set dirty_pct N\n"
        "  set chunk_rows N\n"
        "  set layers N\n"
        "  set mode full|dirty|direct\n"
        "  set target_fps N\n"
        "  set overlay on|off\n"
        "  auto sprites|particles|simulation|td [fps] [secs]\n"
        "  chunks                   LCD chunk-size sweep (blocking)\n"
        "  crossover [secs]         dirty vs full sweep (blocking)\n"
        "  membench                 FB/PSRAM memory timings (blocking)\n"
        "  spisweep                 SPI clock sweep + visual animation (s/p/u)\n"
        "  spisweep auto            same, no prompt (TX_OK only; still watch LCD)\n"
        "  endurance [min]          60 MHz stress (default 45 min; q=abort)\n"
        "\n");
}

static void stop_test(bench_ctx_t *ctx)
{
    if (ctx->test && ctx->test->deinit) {
        ctx->test->deinit(ctx);
    }
    ctx->test = NULL;
    ctx->cfg.running = false;
    strncpy(ctx->cfg.active_test, "idle", sizeof(ctx->cfg.active_test) - 1);
}

static void start_test(bench_ctx_t *ctx, const char *name)
{
    const bench_test_t *t = bench_find_test(name);
    if (!t) {
        printf("unknown test '%s' — try list\n", name);
        return;
    }
    stop_test(ctx);
    metrics_reset(&ctx->metrics);
    ctx->test = t;
    strncpy(ctx->cfg.active_test, t->name, sizeof(ctx->cfg.active_test) - 1);
    t->init(ctx);
    ctx->cfg.running = true;
    printf("running %s (frame task will paint)\n", t->name);
}

static void run_chunk_sweep(bench_ctx_t *ctx)
{
    printf("\n=== LCD CHUNK SWEEP ===\n");
    printf("Wire bytes full frame: %u\n",
           (unsigned)lcd_hw_rect_wire_bytes(&ctx->lcd, LCD_H_RES, LCD_V_RES));
    int chunks[] = {1, 4, 8, 16, 32, 64, 0};
    fb_clear(&ctx->fb, COL_RED);
    for (int i = 0; i < LCD_H_RES * LCD_V_RES; ++i) {
        ctx->fb.pixels[i] = (uint16_t)(i & 0xFFFF);
    }
    for (int ci = 0; chunks[ci] > 0; ++ci) {
        int rows = chunks[ci];
        for (int w = 0; w < 3; ++w) {
            lcd_hw_blit_rgb565(&ctx->lcd, 0, 0, LCD_H_RES, LCD_V_RES, ctx->fb.pixels, LCD_H_RES,
                               rows);
        }
        const int N = 20;
        int64_t sum = 0;
        size_t bytes = 0;
        for (int n = 0; n < N; ++n) {
            int64_t t0 = esp_timer_get_time();
            bytes = lcd_hw_blit_rgb565(&ctx->lcd, 0, 0, LCD_H_RES, LCD_V_RES, ctx->fb.pixels,
                                       LCD_H_RES, rows);
            sum += esp_timer_get_time() - t0;
        }
        double ms = (double)sum / N / 1000.0;
        double fps = 1000.0 / ms;
        double mbps = (bytes / (ms / 1000.0)) / (1024.0 * 1024.0);
        printf("chunk_rows=%2d  avg=%.2f ms  equiv_fps=%.2f  bytes=%u  MB/s=%.2f\n", rows, ms, fps,
               (unsigned)bytes, mbps);
        printf("JSONL {\"test\":\"lcd_chunks\",\"chunk_rows\":%d,\"frame_ms\":%.3f,\"fps\":%.2f,"
               "\"bytes\":%u,\"mbps\":%.2f}\n",
               rows, ms, fps, (unsigned)bytes, mbps);
    }
    printf("=== END CHUNK SWEEP ===\n\n");
}

static int handle_line(bench_ctx_t *ctx, char *line)
{
    sanitize_cmd_line(line);
    if (!line[0]) {
        return 0;
    }
    char cmd[32] = {0};
    char a1[32] = {0};
    char a2[32] = {0};
    char a3[32] = {0};
    sscanf(line, "%31s %31s %31s %31s", cmd, a1, a2, a3);

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "list") == 0) {
        bench_list_tests();
    } else if (strcmp(cmd, "meta") == 0 || strcmp(cmd, "status") == 0) {
        printf("active=%s running=%d sprites=%d particles=%d enemies=%d mode=%d chunk=%d\n",
               ctx->cfg.active_test, ctx->cfg.running, ctx->cfg.sprites, ctx->cfg.particles,
               ctx->cfg.enemies, (int)ctx->cfg.render_mode, ctx->cfg.chunk_rows);
        if (strcmp(cmd, "meta") == 0) {
            bench_print_metadata(ctx);
        }
    } else if (strcmp(cmd, "run") == 0) {
        if (!a1[0]) {
            printf("usage: run <test>\n");
        } else {
            start_test(ctx, a1);
        }
    } else if (strcmp(cmd, "stop") == 0) {
        stop_test(ctx);
        printf("stopped\n");
    } else if (strcmp(cmd, "report") == 0) {
        metrics_print_human(ctx->cfg.active_test, &ctx->cfg, &ctx->metrics, NULL);
        metrics_print_jsonl(ctx->cfg.active_test, &ctx->cfg, &ctx->metrics, NULL);
    } else if (strcmp(cmd, "reset") == 0) {
        metrics_reset(&ctx->metrics);
        printf("metrics reset\n");
    } else if (strcmp(cmd, "chunks") == 0) {
        run_chunk_sweep(ctx);
    } else if (strcmp(cmd, "crossover") == 0) {
        int secs = a1[0] ? atoi(a1) : 2;
        if (secs < 1) {
            secs = 1;
        }
        ctx->cfg.running = false;
        bench_run_crossover(ctx, secs);
    } else if (strcmp(cmd, "membench") == 0) {
        bench_run_memory_bench(ctx);
    } else if (strcmp(cmd, "spisweep") == 0) {
        ctx->cfg.running = false;
        bool auto_cls = (strcmp(a1, "auto") == 0);
        bench_run_spisweep(ctx, auto_cls);
    } else if (strcmp(cmd, "endurance") == 0) {
        int minutes = a1[0] ? atoi(a1) : 45;
        if (minutes < 1) {
            minutes = 1;
        }
        ctx->cfg.running = false;
        bench_run_endurance(ctx, minutes);
    } else if (strcmp(cmd, "auto") == 0) {
        int fps = a2[0] ? atoi(a2) : 30;
        int secs = a3[0] ? atoi(a3) : 3;
        if (!a1[0]) {
            printf("usage: auto sprites|particles|simulation|td [fps] [secs]\n");
        } else {
            ctx->cfg.running = false;
            bench_run_auto(ctx, a1, fps, secs);
        }
    } else if (strcmp(cmd, "set") == 0) {
        int v = atoi(a2);
        if (strcmp(a1, "sprites") == 0) {
            ctx->cfg.sprites = v;
        } else if (strcmp(a1, "particles") == 0) {
            ctx->cfg.particles = v;
        } else if (strcmp(a1, "enemies") == 0) {
            ctx->cfg.enemies = v;
        } else if (strcmp(a1, "towers") == 0) {
            ctx->cfg.towers = v;
        } else if (strcmp(a1, "projectiles") == 0) {
            ctx->cfg.projectiles = v;
        } else if (strcmp(a1, "sprite_w") == 0) {
            ctx->cfg.sprite_w = v;
        } else if (strcmp(a1, "sprite_h") == 0) {
            ctx->cfg.sprite_h = v;
        } else if (strcmp(a1, "sprite_mode") == 0) {
            ctx->cfg.sprite_mode = v;
        } else if (strcmp(a1, "dirty_pct") == 0) {
            ctx->cfg.dirty_pct = v;
        } else if (strcmp(a1, "chunk_rows") == 0) {
            ctx->cfg.chunk_rows = v;
        } else if (strcmp(a1, "layers") == 0) {
            ctx->cfg.tile_layers = v;
        } else if (strcmp(a1, "target_fps") == 0) {
            ctx->cfg.target_fps = v;
        } else if (strcmp(a1, "mode") == 0) {
            if (strcmp(a2, "full") == 0) {
                ctx->cfg.render_mode = RENDER_MODE_FULL;
            } else if (strcmp(a2, "dirty") == 0) {
                ctx->cfg.render_mode = RENDER_MODE_DIRTY;
            } else if (strcmp(a2, "direct") == 0) {
                ctx->cfg.render_mode = RENDER_MODE_DIRECT;
            } else {
                printf("mode: full|dirty|direct\n");
            }
        } else if (strcmp(a1, "overlay") == 0) {
            ctx->cfg.overlay_on = (strcmp(a2, "on") == 0 || strcmp(a2, "1") == 0);
        } else {
            printf("unknown set key '%s'\n", a1);
        }
        printf("ok\n");
    } else {
        printf("unknown command '%s' (help)\n", cmd);
    }
    return 0;
}

/*
 * getchar() on ESP-IDF UART console blocks even with O_NONBLOCK on the fd.
 * That meant: after `run lcd`, no input → no pump_frames → white/black forever.
 * Dedicated frame task paints independently of the serial readline.
 */
static void frame_task(void *arg)
{
    bench_ctx_t *ctx = (bench_ctx_t *)arg;
    while (1) {
        if (ctx->cfg.frames_paused || !ctx->cfg.running || !ctx->test || !ctx->test->frame) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        frame_sample_t f;
        metrics_begin_frame(&ctx->metrics, &f);
        int64_t t0 = esp_timer_get_time();
        ctx->test->frame(ctx, &f);
        if (f.total_us <= 0) {
            f.total_us = esp_timer_get_time() - t0;
        }
        metrics_end_frame(&ctx->metrics, &f);
        /* Yield so console + idle can run between ~370ms full-frame blits. */
        vTaskDelay(1);
    }
}

static void drain_stdin(void)
{
    unsigned char junk;
    for (int i = 0; i < 256; ++i) {
        ssize_t n = read(STDIN_FILENO, &junk, 1);
        if (n <= 0) {
            break;
        }
    }
}

static int read_stdin_byte(void)
{
    unsigned char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n == 1) {
        return (int)c;
    }
    return -1;
}

static void console_task(void *arg)
{
    bench_ctx_t *ctx = (bench_ctx_t *)arg;
    char line[128];
    int n = 0;

    vTaskDelay(pdMS_TO_TICKS(50));
    drain_stdin();

    printf("\nP4Bench ready. Type 'help' or 'run lcd'.\n> ");
    fflush(stdout);

    while (1) {
        int c = read_stdin_byte();
        if (c < 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (c == '\r') {
            continue;
        }
        /* Only printable ASCII + newline/backspace — drop boot noise (0xFF etc.). */
        if (c < 32 && c != '\n' && c != 8 && c != 127) {
            continue;
        }
        if (c > 127) {
            continue;
        }
        if (c == '\n') {
            line[n] = 0;
            printf("\n");
            handle_line(ctx, line);
            n = 0;
            printf("> ");
            fflush(stdout);
            continue;
        }
        if (c == 8 || c == 127) {
            if (n > 0) {
                n--;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }
        if (n < (int)sizeof(line) - 1) {
            line[n++] = (char)c;
            putchar(c);
            fflush(stdout);
        }
    }
}

void bench_console_start(bench_ctx_t *ctx)
{
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }
    /* Frame task priority above console so LCD keeps updating while you type. */
    xTaskCreate(frame_task, "p4bench_frm", 8192, ctx, 6, NULL);
    xTaskCreate(console_task, "p4bench_con", 12288, ctx, 5, NULL);
}
