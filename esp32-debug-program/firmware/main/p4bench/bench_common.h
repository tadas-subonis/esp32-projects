#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lcd_hw.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BENCH_MAX_DIRTY         64
#define BENCH_HIST_FRAMES       256

typedef enum {
    RENDER_MODE_FULL = 0,
    RENDER_MODE_DIRTY = 1,
    RENDER_MODE_DIRECT = 2,
} render_mode_t;

typedef struct {
    int64_t sim_us;
    int64_t render_us;
    int64_t lcd_us;
    int64_t other_us;
    int64_t total_us;
    size_t lcd_bytes;
    int dirty_px;
    int dirty_rects_raw;
    int dirty_rects_merged;
    float fps;
} frame_sample_t;

typedef struct {
    frame_sample_t last;
    double sum_sim_us;
    double sum_render_us;
    double sum_lcd_us;
    double sum_other_us;
    double sum_total_us;
    double sum_lcd_bytes;
    uint32_t frames;
    int64_t min_total_us;
    int64_t max_total_us;
    int64_t hist[BENCH_HIST_FRAMES];
    int hist_n;
    int hist_i;
    /* rolling window for overlay (~1s) */
    double win_sum_total_us;
    uint32_t win_frames;
    int64_t win_start_us;
} metrics_t;

typedef struct {
    int x, y, w, h;
} rect_t;

typedef struct {
    uint16_t *pixels;       /* RGB565, LCD_H_RES * LCD_V_RES */
    int w, h;
    bool in_psram;
    rect_t dirty[BENCH_MAX_DIRTY];
    int dirty_n;
    int dirty_px;
} framebuffer_t;

typedef struct {
    /* tunable parameters */
    int sprites;
    int sprite_w;
    int sprite_h;
    int sprite_mode;        /* 0=opaque 1=colorkey 2=alpha 3=scale 4=rotate */
    int particles;
    int particle_size;
    int enemies;
    int towers;
    int projectiles;
    int tile_layers;
    int dirty_pct;          /* for dirty crossover test */
    int chunk_rows;
    int target_fps;
    int spi_hz;
    render_mode_t render_mode;
    bool overlay_on;
    bool running;
    bool frames_paused; /* true: frame task must not touch LCD/FB (spisweep etc.) */
    char active_test[32];
} bench_config_t;

typedef struct bench_ctx bench_ctx_t;

typedef struct {
    const char *name;
    const char *help;
    void (*init)(bench_ctx_t *ctx);
    void (*frame)(bench_ctx_t *ctx, frame_sample_t *out);
    void (*deinit)(bench_ctx_t *ctx);
} bench_test_t;

struct bench_ctx {
    lcd_hw_t lcd;
    framebuffer_t fb;
    metrics_t metrics;
    bench_config_t cfg;
    const bench_test_t *test;
    void *test_state;
    int64_t overlay_last_us;
    char overlay_lines[8][40];
    int overlay_n;
};

void metrics_reset(metrics_t *m);
void metrics_begin_frame(metrics_t *m, frame_sample_t *f);
void metrics_end_frame(metrics_t *m, frame_sample_t *f);
float metrics_avg_fps(const metrics_t *m);
float metrics_p95_ms(const metrics_t *m);
void metrics_print_human(const char *test, const bench_config_t *cfg, const metrics_t *m,
                         const char *extra_json_fields);
void metrics_print_jsonl(const char *test, const bench_config_t *cfg, const metrics_t *m,
                         const char *extra_fields);

void bench_cfg_defaults(bench_config_t *c);
void bench_print_metadata(const bench_ctx_t *ctx);

#ifdef __cplusplus
}
#endif
