#include "serial_console.h"

#include <Arduino.h>
#include <WiFi.h>

#include "pf_log.h"
#include "photo_sender.h"
#include "secrets.h"

static const char* TAG = "console";

// Implemented in main.cpp (shared with touch/PWR capture).
extern bool cores3_capture_and_send(int* http_code_out, int* w_out, int* h_out, size_t* jpeg_len_out, uint32_t* ms_out);

static char s_line[192];
static size_t s_len = 0;

static const char* skip_ws(const char* s)
{
    while (*s == ' ' || *s == '\t') {
        ++s;
    }
    return s;
}

static void cmd_help()
{
    PF_LOGI(TAG, "commands: help, capture, status, wifi, ping");
    pf_result_json("{\"cmd\":\"help\",\"ok\":true}");
}

static void cmd_status()
{
    const bool wifi = WiFi.status() == WL_CONNECTED;
    const String ip = wifi ? WiFi.localIP().toString() : String("");
    const int rssi = wifi ? WiFi.RSSI() : 0;
    const uint32_t up = millis();
    const uint32_t heap = ESP.getFreeHeap();

    char json[256];
    snprintf(
        json,
        sizeof(json),
        "{\"cmd\":\"status\",\"ok\":true,\"wifi\":%s,\"ip\":\"%s\",\"rssi\":%d,\"paper_host\":\"%s\",\"heap_free\":%lu,\"uptime_ms\":%lu}",
        wifi ? "true" : "false",
        ip.c_str(),
        rssi,
        PAPERCOLOR_HOST,
        (unsigned long)heap,
        (unsigned long)up
    );
    pf_result_json(json);
}

static void cmd_wifi()
{
    const bool ok = wifi_photo_frame_connect();
    char json[128];
    snprintf(json, sizeof(json), "{\"cmd\":\"wifi\",\"ok\":%s}", ok ? "true" : "false");
    pf_result_json(json);
}

static void cmd_ping()
{
    WiFiClient c;
    const bool ok = c.connect(PAPERCOLOR_HOST, 80, 1500);
    if (ok) {
        c.stop();
    }
    char json[128];
    snprintf(json, sizeof(json), "{\"cmd\":\"ping\",\"ok\":%s}", ok ? "true" : "false");
    pf_result_json(json);
}

static void cmd_capture()
{
    int http_code = 0;
    int w = 0;
    int h = 0;
    size_t jpeg_len = 0;
    uint32_t ms = 0;

    const bool ok = cores3_capture_and_send(&http_code, &w, &h, &jpeg_len, &ms);

    char json[256];
    snprintf(
        json,
        sizeof(json),
        "{\"cmd\":\"capture\",\"ok\":%s,\"http\":%d,\"jpeg_bytes\":%lu,\"w\":%d,\"h\":%d,\"ms\":%lu}",
        ok ? "true" : "false",
        http_code,
        (unsigned long)jpeg_len,
        w,
        h,
        (unsigned long)ms
    );
    pf_result_json(json);
}

static void dispatch_line(const char* line)
{
    line = skip_ws(line);
    if (*line == 0) {
        return;
    }

    if (strcmp(line, "help") == 0 || strcmp(line, "?") == 0) {
        cmd_help();
        return;
    }
    if (strcmp(line, "status") == 0) {
        cmd_status();
        return;
    }
    if (strcmp(line, "wifi") == 0) {
        cmd_wifi();
        return;
    }
    if (strcmp(line, "ping") == 0) {
        cmd_ping();
        return;
    }
    if (strcmp(line, "capture") == 0) {
        cmd_capture();
        return;
    }

    PF_LOGW(TAG, "unknown command: %s", line);
    pf_result_json("{\"cmd\":\"unknown\",\"ok\":false,\"error\":\"unknown_command\"}");
}

void serial_console_poll()
{
    while (Serial.available() > 0) {
        const int ch = Serial.read();
        if (ch < 0) {
            return;
        }
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            s_line[s_len] = 0;
            dispatch_line(s_line);
            s_len = 0;
            continue;
        }
        if (s_len + 1 >= sizeof(s_line)) {
            s_len = 0;
            PF_LOGW(TAG, "input line too long; dropping");
            continue;
        }
        s_line[s_len++] = (char)ch;
    }
}

