#include "bench_common.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#ifndef P4BENCH_GIT_COMMIT
#define P4BENCH_GIT_COMMIT "unknown"
#endif

void metrics_reset(metrics_t *m)
{
    memset(m, 0, sizeof(*m));
    m->min_total_us = INT64_MAX;
    m->win_start_us = esp_timer_get_time();
}

void metrics_begin_frame(metrics_t *m, frame_sample_t *f)
{
    (void)m;
    memset(f, 0, sizeof(*f));
}

void metrics_end_frame(metrics_t *m, frame_sample_t *f)
{
    if (f->total_us <= 0) {
        f->total_us = f->sim_us + f->render_us + f->lcd_us + f->other_us;
    }
    int64_t accounted = f->sim_us + f->render_us + f->lcd_us;
    if (f->other_us <= 0 && f->total_us > accounted) {
        f->other_us = f->total_us - accounted;
    }
    if (f->total_us > 0) {
        f->fps = 1000000.0f / (float)f->total_us;
    }

    m->last = *f;
    m->sum_sim_us += (double)f->sim_us;
    m->sum_render_us += (double)f->render_us;
    m->sum_lcd_us += (double)f->lcd_us;
    m->sum_other_us += (double)f->other_us;
    m->sum_total_us += (double)f->total_us;
    m->sum_lcd_bytes += (double)f->lcd_bytes;
    m->frames++;
    if (f->total_us < m->min_total_us) {
        m->min_total_us = f->total_us;
    }
    if (f->total_us > m->max_total_us) {
        m->max_total_us = f->total_us;
    }
    m->hist[m->hist_i] = f->total_us;
    m->hist_i = (m->hist_i + 1) % BENCH_HIST_FRAMES;
    if (m->hist_n < BENCH_HIST_FRAMES) {
        m->hist_n++;
    }

    m->win_sum_total_us += (double)f->total_us;
    m->win_frames++;
    int64_t now = esp_timer_get_time();
    if (now - m->win_start_us >= 1000000) {
        m->win_sum_total_us = (double)f->total_us;
        m->win_frames = 1;
        m->win_start_us = now;
    }
}

float metrics_avg_fps(const metrics_t *m)
{
    if (m->frames == 0 || m->sum_total_us <= 0.0) {
        return 0.0f;
    }
    return (float)((double)m->frames * 1000000.0 / m->sum_total_us);
}

float metrics_p95_ms(const metrics_t *m)
{
    if (m->hist_n <= 0) {
        return 0.0f;
    }
    int64_t tmp[BENCH_HIST_FRAMES];
    memcpy(tmp, m->hist, (size_t)m->hist_n * sizeof(int64_t));
    /* insertion sort — n<=256, no qsort dependency issues */
    for (int i = 1; i < m->hist_n; ++i) {
        int64_t v = tmp[i];
        int j = i - 1;
        while (j >= 0 && tmp[j] > v) {
            tmp[j + 1] = tmp[j];
            --j;
        }
        tmp[j + 1] = v;
    }
    int idx = (m->hist_n * 95) / 100;
    if (idx >= m->hist_n) {
        idx = m->hist_n - 1;
    }
    return (float)tmp[idx] / 1000.0f;
}

static float avg_ms(double sum_us, uint32_t frames)
{
    if (frames == 0) {
        return 0.0f;
    }
    return (float)(sum_us / (double)frames / 1000.0);
}

void metrics_print_human(const char *test, const bench_config_t *cfg, const metrics_t *m,
                         const char *extra_json_fields)
{
    (void)extra_json_fields;
    float fps = metrics_avg_fps(m);
    printf("\n=== P4BENCH RESULT ===\n\n");
    printf("Test: %s\n", test);
    printf("Resolution: %dx%d\n", LCD_H_RES, LCD_V_RES);
    if (cfg->sprites) {
        printf("Sprites: %d  size=%dx%d mode=%d\n", cfg->sprites, cfg->sprite_w, cfg->sprite_h,
               cfg->sprite_mode);
    }
    if (cfg->particles) {
        printf("Particles: %d  size=%d\n", cfg->particles, cfg->particle_size);
    }
    if (cfg->enemies || cfg->towers) {
        printf("TD: enemies=%d towers=%d projectiles=%d\n", cfg->enemies, cfg->towers,
               cfg->projectiles);
    }
    printf("Render mode: %d  chunk_rows=%d  spi_hz=%d\n", (int)cfg->render_mode, cfg->chunk_rows,
           cfg->spi_hz);
    printf("\n");
    printf("FPS avg:       %.1f\n", fps);
    printf("Frame avg:     %.1f ms\n", avg_ms(m->sum_total_us, m->frames));
    printf("Frame min:     %.1f ms\n", m->min_total_us == INT64_MAX ? 0.0f : m->min_total_us / 1000.0f);
    printf("Frame max:     %.1f ms\n", m->max_total_us / 1000.0f);
    printf("Frame p95:     %.1f ms\n", metrics_p95_ms(m));
    printf("\n");
    printf("Simulation:    %.2f ms\n", avg_ms(m->sum_sim_us, m->frames));
    printf("Render:        %.2f ms\n", avg_ms(m->sum_render_us, m->frames));
    printf("LCD:           %.2f ms\n", avg_ms(m->sum_lcd_us, m->frames));
    printf("Other:         %.2f ms\n", avg_ms(m->sum_other_us, m->frames));
    printf("\n");
    if (m->frames) {
        printf("LCD bytes/frame: %.0f\n", m->sum_lcd_bytes / (double)m->frames);
        double mbps = (m->sum_lcd_bytes / (m->sum_lcd_us / 1e6)) / (1024.0 * 1024.0);
        if (m->sum_lcd_us > 0) {
            printf("LCD MB/s:        %.2f\n", mbps);
        }
    }
    printf("Frames:        %lu\n", (unsigned long)m->frames);
    printf("Heap free:     %u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    printf("PSRAM free:    %u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    if (cfg->target_fps > 0) {
        printf("\n%s target_%dfps\n", fps >= (float)cfg->target_fps ? "PASS" : "FAIL",
               cfg->target_fps);
    }
    printf("\n");
}

void metrics_print_jsonl(const char *test, const bench_config_t *cfg, const metrics_t *m,
                         const char *extra_fields)
{
    float fps = metrics_avg_fps(m);
    printf("JSONL {\"test\":\"%s\",\"fps_avg\":%.2f,\"frame_ms\":%.2f,\"frame_p95_ms\":%.2f,"
           "\"sim_ms\":%.2f,\"render_ms\":%.2f,\"lcd_ms\":%.2f,\"other_ms\":%.2f,"
           "\"lcd_bytes\":%.0f,\"frames\":%lu,\"sprites\":%d,\"particles\":%d,"
           "\"enemies\":%d,\"towers\":%d,\"projectiles\":%d,\"render_mode\":%d,"
           "\"chunk_rows\":%d,\"spi_hz\":%d,\"target_fps\":%d,\"pass\":%s%s%s}\n",
           test, fps, avg_ms(m->sum_total_us, m->frames), metrics_p95_ms(m),
           avg_ms(m->sum_sim_us, m->frames), avg_ms(m->sum_render_us, m->frames),
           avg_ms(m->sum_lcd_us, m->frames), avg_ms(m->sum_other_us, m->frames),
           m->frames ? m->sum_lcd_bytes / (double)m->frames : 0.0, (unsigned long)m->frames,
           cfg->sprites, cfg->particles, cfg->enemies, cfg->towers, cfg->projectiles,
           (int)cfg->render_mode, cfg->chunk_rows, cfg->spi_hz, cfg->target_fps,
           (cfg->target_fps > 0 && fps >= (float)cfg->target_fps) ? "true" : "false",
           extra_fields && extra_fields[0] ? "," : "", extra_fields ? extra_fields : "");
}

void bench_cfg_defaults(bench_config_t *c)
{
    memset(c, 0, sizeof(*c));
    c->sprites = 100;
    c->sprite_w = 32;
    c->sprite_h = 32;
    c->sprite_mode = 0;
    c->particles = 500;
    c->particle_size = 4;
    c->enemies = 100;
    c->towers = 20;
    c->projectiles = 100;
    c->tile_layers = 1;
    c->dirty_pct = 20;
    c->chunk_rows = 16;
    c->target_fps = 30;
    c->spi_hz = LCD_SPI_HZ_DEFAULT;
    c->render_mode = RENDER_MODE_FULL;
    c->overlay_on = true;
    c->running = false;
    c->frames_paused = false;
    strncpy(c->active_test, "idle", sizeof(c->active_test) - 1);
}

void bench_print_metadata(const bench_ctx_t *ctx)
{
    esp_chip_info_t info;
    esp_chip_info(&info);
    printf("\n=== P4BENCH METADATA ===\n");
    printf("git:           %s\n", P4BENCH_GIT_COMMIT);
    printf("idf:           %s\n", IDF_VER);
    printf("chip:          ESP32-P4 rev%d cores=%d\n", info.revision, info.cores);
    printf("cpu_freq_mhz:  (see sdkconfig; default target 360)\n");
    printf("psram_free:    %u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    printf("heap_free:     %u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    printf("screen:        %dx%d\n", LCD_H_RES, LCD_V_RES);
    printf("pixel_format:  RGB565 fb / RGB666 wire (%d bpp)\n", ctx->lcd.bpp);
    printf("spi_hz:        %d\n", ctx->lcd.spi_hz);
    printf("xfer_buf:      %u bytes\n", (unsigned)ctx->lcd.line_cap);
    printf("dma:           SPI_DMA_CH_AUTO\n");
    printf("full_frame_wire_bytes: %u\n", (unsigned)lcd_hw_rect_wire_bytes(&ctx->lcd, LCD_H_RES, LCD_V_RES));
    printf("========================\n\n");
}
