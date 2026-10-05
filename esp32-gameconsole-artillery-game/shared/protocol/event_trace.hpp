#pragma once

#include <cstdint>
#include <cstdio>

/**
 * Cross-process correlation for net logs.
 * Format: eid=<id> t_ms=<boot-or-process ms> …
 * Firmware: t_ms = esp_timer since boot. Server: t_ms = steady_clock since process start.
 * Clients put eid on hello; server echoes it on welcome/error so UART and server stderr line up.
 */
namespace artillery {
namespace trace {

inline uint32_t next_eid()
{
    static uint32_t n = 0;
    return ++n;
}

}  // namespace trace
}  // namespace artillery

#if defined(ESP_PLATFORM)
#include "esp_timer.h"
#include "esp_log.h"

inline uint32_t artillery_trace_t_ms()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

#define ART_NET_LOGI(tag, eid, fmt, ...)                                                           \
    ESP_LOGI(tag, "eid=%lu t_ms=%lu " fmt, static_cast<unsigned long>(eid),                        \
             static_cast<unsigned long>(artillery_trace_t_ms()), ##__VA_ARGS__)
#define ART_NET_LOGW(tag, eid, fmt, ...)                                                           \
    ESP_LOGW(tag, "eid=%lu t_ms=%lu " fmt, static_cast<unsigned long>(eid),                        \
             static_cast<unsigned long>(artillery_trace_t_ms()), ##__VA_ARGS__)
#define ART_NET_LOGE(tag, eid, fmt, ...)                                                           \
    ESP_LOGE(tag, "eid=%lu t_ms=%lu " fmt, static_cast<unsigned long>(eid),                        \
             static_cast<unsigned long>(artillery_trace_t_ms()), ##__VA_ARGS__)

#else
#include <chrono>

inline uint32_t artillery_trace_t_ms()
{
    using clock = std::chrono::steady_clock;
    static const clock::time_point t0 = clock::now();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0).count());
}

#define ART_NET_LOGI(tag, eid, fmt, ...)                                                           \
    std::fprintf(stderr, "[%s] eid=%lu t_ms=%lu " fmt "\n", tag,                                  \
                 static_cast<unsigned long>(eid), static_cast<unsigned long>(artillery_trace_t_ms()), \
                 ##__VA_ARGS__)
#define ART_NET_LOGW(tag, eid, fmt, ...)                                                           \
    std::fprintf(stderr, "[%s] eid=%lu t_ms=%lu " fmt "\n", tag,                                  \
                 static_cast<unsigned long>(eid), static_cast<unsigned long>(artillery_trace_t_ms()), \
                 ##__VA_ARGS__)
#define ART_NET_LOGE(tag, eid, fmt, ...)                                                           \
    std::fprintf(stderr, "[%s] eid=%lu t_ms=%lu " fmt "\n", tag,                                  \
                 static_cast<unsigned long>(eid), static_cast<unsigned long>(artillery_trace_t_ms()), \
                 ##__VA_ARGS__)

#endif
