idf_component_register(
    SRCS
        "main.c"
        "lcd_hw.c"
        "p4bench/p4bench_main.c"
        "p4bench/bench_metrics.c"
        "p4bench/bench_fb.c"
        "p4bench/bench_gfx.c"
        "p4bench/bench_overlay.c"
        "p4bench/bench_console.c"
        "p4bench/bench_tests.c"
        "p4bench/bench_spisweep.c"
        "p4bench/bench_endurance.c"
    INCLUDE_DIRS "." "p4bench"
    REQUIRES driver esp_driver_gpio esp_driver_spi esp_lcd esp_timer esp_system
)
