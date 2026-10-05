#include "p4bench.h"

#include <stdio.h>
#include <string.h>

#include "bench_console.h"
#include "bench_fb.h"
#include "bench_tests.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "p4bench";

static bench_ctx_t s_ctx;

void p4bench_app_main(void)
{
    ESP_LOGI(TAG, "P4Bench starting");
    memset(&s_ctx, 0, sizeof(s_ctx));
    bench_cfg_defaults(&s_ctx.cfg);

    if (!lcd_hw_init(&s_ctx.lcd, s_ctx.cfg.spi_hz, 64)) {
        ESP_LOGE(TAG, "LCD init failed");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    if (!fb_init(&s_ctx.fb, true)) {
        ESP_LOGE(TAG, "FB init failed");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    metrics_reset(&s_ctx.metrics);
    bench_print_metadata(&s_ctx);

    /* show pattern once so the panel is visibly alive */
    const bench_test_t *pat = bench_find_test("pattern");
    if (pat) {
        s_ctx.test = pat;
        strncpy(s_ctx.cfg.active_test, "pattern", sizeof(s_ctx.cfg.active_test) - 1);
        pat->init(&s_ctx);
        for (int i = 0; i < 5; ++i) {
            frame_sample_t f;
            metrics_begin_frame(&s_ctx.metrics, &f);
            pat->frame(&s_ctx, &f);
            metrics_end_frame(&s_ctx.metrics, &f);
        }
        if (pat->deinit) {
            pat->deinit(&s_ctx);
        }
        s_ctx.test = NULL;
        metrics_reset(&s_ctx.metrics);
    }

    lcd_hw_fill_screen(&s_ctx.lcd, COL_BLACK);
    /* Frame task + console (see bench_console.c). */
    bench_console_start(&s_ctx);
}
