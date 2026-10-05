#include "photo_sender.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "photo_frame_protocol.h"
#include "photo_frame_wifi.h"
#include "pf_log.h"
#include "secrets.h"

static const char* TAG = "photo_sender";

static bool s_wifi_ready = false;

bool wifi_photo_frame_connect()
{
    if (s_wifi_ready && WiFi.status() == WL_CONNECTED) {
        return true;
    }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);

    IPAddress local_ip;
    IPAddress gateway;
    IPAddress subnet;
    local_ip.fromString(PHOTO_FRAME_CORES3_IP);
    gateway.fromString(PHOTO_FRAME_GATEWAY);
    subnet.fromString(PHOTO_FRAME_NETMASK);
    WiFi.config(local_ip, gateway, subnet);

    PF_LOGI(TAG, "connecting to AP %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    const uint32_t timeout_ms = 20000;
    const uint32_t start      = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeout_ms) {
            PF_LOGW(TAG, "Wi-Fi timeout");
            return false;
        }
        delay(200);
    }

    PF_LOGI(TAG, "connected %s", WiFi.localIP().toString().c_str());
    s_wifi_ready = true;
    return true;
}

int photo_frame_send(const uint8_t* jpeg, size_t jpeg_len, int width, int height)
{
    if (!jpeg || jpeg_len == 0 || jpeg_len > PHOTO_FRAME_MAX_JPEG_BYTES) {
        return 0;
    }

    if (!wifi_photo_frame_connect()) {
        return 0;
    }

    StaticJsonDocument<256> meta_doc;
    meta_doc["version"]      = PHOTO_FRAME_PROTOCOL_VERSION;
    meta_doc["id"]           = String(millis());
    meta_doc["caption"]      = "";
    meta_doc["width"]        = width;
    meta_doc["height"]       = height;
    meta_doc["jpeg_quality"] = PHOTO_FRAME_JPEG_QUALITY;
    meta_doc["captured_ms"]  = (uint64_t)millis();

    String meta_json;
    serializeJson(meta_doc, meta_json);

    const char* boundary = "----pfboundary";

    String preamble;
    preamble.reserve(256 + meta_json.length());
    preamble += "--";
    preamble += boundary;
    preamble += "\r\nContent-Disposition: form-data; name=\"meta\"\r\n";
    preamble += "Content-Type: application/json\r\n\r\n";
    preamble += meta_json;
    preamble += "\r\n--";
    preamble += boundary;
    preamble += "\r\nContent-Disposition: form-data; name=\"image\"; filename=\"photo.jpg\"\r\n";
    preamble += "Content-Type: image/jpeg\r\n\r\n";

    const String epilogue = String("\r\n--") + boundary + "--\r\n";

    const size_t total_len = preamble.length() + jpeg_len + epilogue.length();
    uint8_t*     body      = (uint8_t*)malloc(total_len);
    if (!body) {
        PF_LOGE(TAG, "OOM building multipart (%u bytes)", (unsigned)total_len);
        return 0;
    }

    size_t off = 0;
    memcpy(body + off, preamble.c_str(), preamble.length());
    off += preamble.length();
    memcpy(body + off, jpeg, jpeg_len);
    off += jpeg_len;
    memcpy(body + off, epilogue.c_str(), epilogue.length());

    HTTPClient http;
    const String url = String("http://") + PAPERCOLOR_HOST + PHOTO_FRAME_API_PATH;
    if (!http.begin(url)) {
        free(body);
        return 0;
    }

    http.addHeader("Content-Type", String("multipart/form-data; boundary=") + boundary);
    http.setTimeout(45000);

    const int code = http.POST(body, total_len);
    free(body);

    if (code > 0) {
        PF_LOGI(TAG, "POST %d: %s", code, http.getString().c_str());
    } else {
        PF_LOGW(TAG, "POST failed: %s", http.errorToString(code).c_str());
    }

    http.end();
    return code > 0 ? code : 0;
}
