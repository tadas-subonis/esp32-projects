#include "http_photo_server.hpp"

#include <cstdlib>
#include <cstring>

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "photo_frame_protocol.h"
#include "photo_frame_wifi.h"
#include "photo_gallery.hpp"

static const char* TAG = "http_photo";

static httpd_handle_t s_server = nullptr;

static const char* mem_find(const char* hay, size_t hlen, const char* needle, size_t nlen)
{
    if (nlen > hlen) {
        return nullptr;
    }
    for (size_t i = 0; i <= hlen - nlen; ++i) {
        if (memcmp(hay + i, needle, nlen) == 0) {
            return hay + i;
        }
    }
    return nullptr;
}

static esp_err_t send_json(httpd_req_t* req, int status, const char* json)
{
    httpd_resp_set_status(req, status == 202 ? "202 Accepted" : "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_photo_post(httpd_req_t* req)
{
    const int64_t t0_us = esp_timer_get_time();

    if (g_gallery.refresh_busy()) {
        ESP_LOGW(TAG, "reject busy");
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"status\":\"busy\"}", HTTPD_RESP_USE_STRLEN);
    }

    if (req->content_len <= 0 || req->content_len > (int)PHOTO_FRAME_MAX_JPEG_BYTES + 4096) {
        ESP_LOGW(TAG, "reject size=%d", (int)req->content_len);
        httpd_resp_set_status(req, "413 Payload Too Large");
        return httpd_resp_send(req, "too large", HTTPD_RESP_USE_STRLEN);
    }

    ESP_LOGI(TAG, "POST %s len=%d", PHOTO_FRAME_API_PATH, (int)req->content_len);

    char* body = (char*)malloc(req->content_len + 1);
    if (!body) {
        ESP_LOGE(TAG, "oom allocating %d", (int)req->content_len + 1);
        return send_json(req, 500, "{\"status\":\"oom\"}");
    }

    size_t left = req->content_len;
    size_t off  = 0;
    while (left > 0) {
        const int ret = httpd_req_recv(req, body + off, left);
        if (ret <= 0) {
            free(body);
            ESP_LOGE(TAG, "read failed ret=%d off=%u left=%u", ret, (unsigned)off, (unsigned)left);
            return send_json(req, 500, "{\"status\":\"read\"}");
        }
        off += (size_t)ret;
        left -= (size_t)ret;
    }
    body[off] = '\0';

    char ct[256];
    const char* boundary_ptr = nullptr;
    if (httpd_req_get_hdr_value_str(req, "Content-Type", ct, sizeof(ct)) == ESP_OK) {
        boundary_ptr = strstr(ct, "boundary=");
    }
    if (!boundary_ptr) {
        free(body);
        ESP_LOGW(TAG, "missing boundary (Content-Type=%s)", ct);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "no boundary", HTTPD_RESP_USE_STRLEN);
    }
    boundary_ptr += 9;
    if (*boundary_ptr == '"') {
        boundary_ptr++;
    }

    char boundary[128];
    int bi = 0;
    while (*boundary_ptr && *boundary_ptr != ' ' && *boundary_ptr != ';' && *boundary_ptr != '"' &&
           bi < (int)sizeof(boundary) - 1) {
        boundary[bi++] = *boundary_ptr++;
    }
    boundary[bi] = '\0';

    char marker[140];
    snprintf(marker, sizeof(marker), "--%s", boundary);
    const size_t marker_len = strlen(marker);

    char sep[144];
    snprintf(sep, sizeof(sep), "\r\n--%s", boundary);
    const size_t sep_len = strlen(sep);

    const char* meta_json = nullptr;
    size_t      meta_len  = 0;
    const char* image     = nullptr;
    size_t      image_len = 0;

    const char* pos = body;
    const char* end = body + off;
    while (pos < end) {
        const size_t remaining = end - pos;
        const char*  part      = mem_find(pos, remaining, marker, marker_len);
        if (!part) {
            break;
        }
        part += marker_len;
        if (part + 2 <= end && part[0] == '\r' && part[1] == '\n') {
            part += 2;
        } else if (part + 2 <= end && part[0] == '-' && part[1] == '-') {
            break;
        }

        const char* hdr_end = mem_find(part, end - part, "\r\n\r\n", 4);
        if (!hdr_end) {
            break;
        }
        hdr_end += 4;

        const char* next = mem_find(hdr_end, end - hdr_end, sep, sep_len);
        const char* val_end = next ? next : end;
        while (val_end > hdr_end && (val_end[-1] == '\n' || val_end[-1] == '\r')) {
            val_end--;
        }
        const size_t vlen = (size_t)(val_end - hdr_end);

        if (mem_find(part, hdr_end - part, "name=\"meta\"", 11)) {
            meta_json = hdr_end;
            meta_len  = vlen;
        } else if (mem_find(part, hdr_end - part, "name=\"image\"", 12)) {
            image     = hdr_end;
            image_len = vlen;
        }

        pos = next ? next : end;
    }

    if (!image || image_len < 3) {
        free(body);
        ESP_LOGW(TAG, "missing image part");
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "missing image", HTTPD_RESP_USE_STRLEN);
    }

    int meta_w = 0;
    int meta_h = 0;
    if (meta_json && meta_len > 0) {
        char* meta_copy = (char*)malloc(meta_len + 1);
        if (meta_copy) {
            memcpy(meta_copy, meta_json, meta_len);
            meta_copy[meta_len] = '\0';
            cJSON* root = cJSON_Parse(meta_copy);
            if (root) {
                cJSON* w = cJSON_GetObjectItem(root, "width");
                cJSON* h = cJSON_GetObjectItem(root, "height");
                if (cJSON_IsNumber(w)) {
                    meta_w = w->valueint;
                }
                if (cJSON_IsNumber(h)) {
                    meta_h = h->valueint;
                }
                cJSON_Delete(root);
            }
            free(meta_copy);
        }
    }

    ESP_LOGI(TAG, "parsed meta=%dx%d image_len=%u", meta_w, meta_h, (unsigned)image_len);

    std::string saved_name;
    const esp_err_t save_err =
        g_gallery.save_and_display_jpeg((const uint8_t*)image, image_len, meta_w, meta_h, &saved_name);
    free(body);

    if (save_err == ESP_ERR_INVALID_STATE) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        ESP_LOGW(TAG, "save busy");
        return httpd_resp_send(req, "{\"status\":\"busy\"}", HTTPD_RESP_USE_STRLEN);
    }
    if (save_err == ESP_ERR_INVALID_ARG) {
        httpd_resp_set_status(req, "415 Unsupported Media Type");
        ESP_LOGW(TAG, "reject not jpeg");
        return httpd_resp_send(req, "{\"status\":\"not_jpeg\"}", HTTPD_RESP_USE_STRLEN);
    }
    if (save_err != ESP_OK) {
        ESP_LOGE(TAG, "save failed err=%s", esp_err_to_name(save_err));
        return send_json(req, 500, "{\"status\":\"error\"}");
    }

    const int64_t dt_us = esp_timer_get_time() - t0_us;
    ESP_LOGI(TAG, "accepted saved=%s idx=%u dt_ms=%lld", saved_name.c_str(), (unsigned)g_gallery.current_index(),
             (long long)(dt_us / 1000));

    char response[192];
    snprintf(response, sizeof(response),
             "{\"status\":\"queued\",\"saved_path\":\"%s\",\"index\":%u,\"refresh_eta_s\":%d}",
             saved_name.c_str(), (unsigned)g_gallery.current_index(), PHOTO_FRAME_REFRESH_ETA_SEC);
  return send_json(req, 202, response);
}

esp_err_t http_photo_server_start()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port    = PHOTO_FRAME_HTTP_PORT;
    // Multipart parse + SD write; e-ink refresh is queued to the main loop.
    config.stack_size       = 24576;
    config.lru_purge_enable = true;

    if (httpd_start(&s_server, &config) != ESP_OK) {
        return ESP_FAIL;
    }

    httpd_uri_t photo_uri = {
        .uri      = PHOTO_FRAME_API_PATH,
        .method   = HTTP_POST,
        .handler  = handle_photo_post,
        .user_ctx = nullptr,
    };
    httpd_register_uri_handler(s_server, &photo_uri);
    ESP_LOGI(TAG, "listening on %s", PHOTO_FRAME_API_PATH);
    return ESP_OK;
}
