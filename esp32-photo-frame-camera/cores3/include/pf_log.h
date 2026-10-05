#pragma once

#include <Arduino.h>
#include <cstdarg>

inline void pf_logf(const char* level, const char* tag, const char* fmt, ...)
{
    Serial.printf("%s (%lu) %s: ", level, (unsigned long)millis(), tag);
    va_list args;
    va_start(args, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    Serial.print(buf);
    va_end(args);
    Serial.print("\n");
}

#define PF_LOGI(tag, fmt, ...) pf_logf("I", (tag), (fmt), ##__VA_ARGS__)
#define PF_LOGW(tag, fmt, ...) pf_logf("W", (tag), (fmt), ##__VA_ARGS__)
#define PF_LOGE(tag, fmt, ...) pf_logf("E", (tag), (fmt), ##__VA_ARGS__)

inline void pf_result_json(const char* json)
{
    Serial.print("<<< ");
    Serial.println(json);
}

