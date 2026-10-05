#pragma once

#include "bench_common.h"

const bench_test_t *bench_find_test(const char *name);
void bench_list_tests(void);
void bench_run_auto(bench_ctx_t *ctx, const char *which, int target_fps, int seconds_per_step);
void bench_run_crossover(bench_ctx_t *ctx, int seconds_per_step);
void bench_run_memory_bench(bench_ctx_t *ctx);
void bench_run_spisweep(bench_ctx_t *ctx, bool auto_classify);
void bench_run_endurance(bench_ctx_t *ctx, int minutes);

/* Individual test descriptors (exported for runner). */
extern const bench_test_t TEST_PATTERN;
extern const bench_test_t TEST_LCD;
extern const bench_test_t TEST_MEMORY;
extern const bench_test_t TEST_SPRITES;
extern const bench_test_t TEST_DIRTY;
extern const bench_test_t TEST_PARTICLES;
extern const bench_test_t TEST_TILES;
extern const bench_test_t TEST_COMPOSE;
extern const bench_test_t TEST_TD_SIM;
extern const bench_test_t TEST_TD;
extern const bench_test_t TEST_CHAOS;
