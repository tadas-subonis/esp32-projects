#include "photo_gallery.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hal.hpp"
#include "image_utils.hpp"
#include "photo_frame_protocol.h"
#include "storage.hpp"

static const char* TAG = "gallery";

PhotoGallery g_gallery;

static bool is_jpeg_name(const char* name)
{
    const char* dot = strrchr(name, '.');
    if (!dot) {
        return false;
    }
    return strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0;
}

static bool dirent_is_file(const struct dirent* ent, const char* full_path)
{
    if (ent->d_type == DT_DIR) {
        return false;
    }
    if (ent->d_type == DT_REG) {
        return true;
    }
    struct stat st {};
    if (stat(full_path, &st) != 0) {
        ESP_LOGW(TAG, "stat failed %s errno=%d", full_path, errno);
        return false;
    }
    return S_ISREG(st.st_mode);
}

static int extract_index(const std::string& name)
{
    unsigned n      = 0;
    char     prefix = 0;
    if (sscanf(name.c_str(), "imaged%3u", &n) == 1) {
        return (int)n;
    }
    if (sscanf(name.c_str(), "image%c%3u", &prefix, &n) == 2) {
        return (int)n;
    }
    return -1;
}

bool PhotoGallery::refresh_busy() const
{
    if (refresh_in_progress_) {
        return true;
    }
    if (!pending_mu_) {
        return pending_;
    }
    if (xSemaphoreTake(pending_mu_, 0) != pdTRUE) {
        return true;
    }
    const bool busy = pending_;
    xSemaphoreGive(pending_mu_);
    return busy;
}

void PhotoGallery::scan()
{
    photos_.clear();
    if (!photo_storage_is_mounted()) {
        ESP_LOGW(TAG, "scan skipped — SD not mounted");
        return;
    }

    if (!g_hal.lock_bus(pdMS_TO_TICKS(60000))) {
        ESP_LOGE(TAG, "scan: bus lock timeout");
        return;
    }

    DIR* dir = opendir(PHOTO_STORAGE_BASE_PATH);
    if (!dir) {
        ESP_LOGE(TAG, "opendir %s failed errno=%d", PHOTO_STORAGE_BASE_PATH, errno);
        g_hal.unlock_bus();
        return;
    }

    int seen = 0;
    int kept = 0;
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        ++seen;
        if (strncmp(ent->d_name, "image", 5) != 0 || !is_jpeg_name(ent->d_name)) {
            ESP_LOGI(TAG, "scan other entry name=%s d_type=%u", ent->d_name, (unsigned)ent->d_type);
            continue;
        }

        const std::string path = std::string(PHOTO_STORAGE_BASE_PATH "/") + ent->d_name;
        if (!dirent_is_file(ent, path.c_str())) {
            ESP_LOGD(TAG, "scan skip non-file %s d_type=%u", ent->d_name, (unsigned)ent->d_type);
            continue;
        }

        photos_.push_back(path);
        ++kept;
        ESP_LOGI(TAG, "scan found [%d] %s d_type=%u", kept - 1, ent->d_name, (unsigned)ent->d_type);
    }
    closedir(dir);
    g_hal.unlock_bus();

    std::sort(photos_.begin(), photos_.end(), [](const std::string& a, const std::string& b) {
        const size_t slash_a = a.find_last_of('/');
        const size_t slash_b = b.find_last_of('/');
        const std::string na = slash_a == std::string::npos ? a : a.substr(slash_a + 1);
        const std::string nb = slash_b == std::string::npos ? b : b.substr(slash_b + 1);
        return extract_index(na) < extract_index(nb);
    });

    if (!photos_.empty() && current_index_ >= photos_.size()) {
        ESP_LOGW(TAG, "clamp index %u -> %u", (unsigned)current_index_,
                 (unsigned)(photos_.size() - 1));
        current_index_ = (uint16_t)(photos_.size() - 1);
    }

    ESP_LOGI(TAG, "scan done entries=%d photos=%u index=%u", seen, (unsigned)photos_.size(),
             (unsigned)current_index_);
}

void PhotoGallery::play_nav_tone(int midi_note) const
{
    M5.Speaker.tone(midi_note, 80);
}

void PhotoGallery::play_receive_chime() const
{
    M5.Speaker.tone(784, 100);
    vTaskDelay(pdMS_TO_TICKS(110));
    M5.Speaker.tone(988, 120);
}

void PhotoGallery::play_done_chime() const
{
    M5.Speaker.tone(523, 120);
    vTaskDelay(pdMS_TO_TICKS(130));
    M5.Speaker.tone(659, 150);
}

std::string PhotoGallery::next_filename() const
{
    bool used[1000] = {false};
    DIR* dir        = opendir(PHOTO_STORAGE_BASE_PATH);
    if (!dir) {
        ESP_LOGE(TAG, "next_filename opendir failed errno=%d", errno);
        return {};
    }

    int scanned = 0;
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        unsigned n = 0;
        char     p = 0;
        if (sscanf(ent->d_name, "image%c%3u", &p, &n) == 2 && p == 'd' && n < 1000) {
            const std::string path = std::string(PHOTO_STORAGE_BASE_PATH "/") + ent->d_name;
            if (dirent_is_file(ent, path.c_str())) {
                used[n] = true;
                ++scanned;
            }
        }
    }
    closedir(dir);
    ESP_LOGI(TAG, "next_filename scanned %d imagedNNN slots", scanned);

    for (unsigned n = 1; n <= 999; ++n) {
        if (!used[n]) {
            char buf[32];
            snprintf(buf, sizeof(buf), "imaged%03u.jpg", n);
            const std::string path = std::string(PHOTO_STORAGE_BASE_PATH "/") + buf;
            ESP_LOGI(TAG, "next_filename -> %s", path.c_str());
            return path;
        }
    }
    ESP_LOGE(TAG, "next_filename exhausted (999 files)");
    return {};
}

bool PhotoGallery::push_canvas_to_epd(const char* reason)
{
    if (!g_hal.canvas) {
        ESP_LOGE(TAG, "push_canvas: no canvas (%s)", reason);
        return false;
    }

    // A full Spectra-6 refresh takes tens of seconds, so allow a generous lock wait.
    if (!g_hal.lock_bus(pdMS_TO_TICKS(60000))) {
        ESP_LOGE(TAG, "push_canvas: bus lock timeout (%s)", reason);
        return false;
    }

    refresh_in_progress_ = true;
    ESP_LOGI(TAG, "refresh begin (%s) busy_pin=%d", reason, (int)g_hal.epd_busy());
    const int64_t t0 = esp_timer_get_time();
    g_hal.canvas->pushSprite(0, 0);  // _auto_display: endWrite() triggers the panel refresh
    const int64_t dt_ms = (esp_timer_get_time() - t0) / 1000;
    refresh_in_progress_ = false;
    g_hal.unlock_bus();
    ESP_LOGI(TAG, "refresh done (%s) in %lldms", reason, (long long)dt_ms);
    return true;
}

bool PhotoGallery::display_file(const std::string& path)
{
    if (!g_hal.canvas) {
        ESP_LOGE(TAG, "display_file: no canvas");
        return false;
    }

    int image_width  = 0;
    int image_height = 0;

    if (!g_hal.lock_bus(pdMS_TO_TICKS(60000))) {
        ESP_LOGE(TAG, "display_file: bus lock for read failed");
        return false;
    }
    const bool size_ok = get_image_size_from_file(path.c_str(), &image_width, &image_height);
    g_hal.unlock_bus();

    if (!size_ok) {
        ESP_LOGE(TAG, "image size failed: %s", path.c_str());
        return false;
    }

    const int scr_w = g_hal.canvas->width();
    const int scr_h = g_hal.canvas->height();
    const float scale =
        std::min((float)scr_w / (float)image_width, (float)scr_h / (float)image_height);
    const int draw_x = (scr_w - (int)(image_width * scale)) / 2;
    const int draw_y = (scr_h - (int)(image_height * scale)) / 2;

    ESP_LOGI(TAG, "display file %s %dx%d scale=%.2f at (%d,%d)", path.c_str(), image_width,
             image_height, scale, draw_x, draw_y);

    g_hal.canvas->fillScreen(TFT_WHITE);

    if (!g_hal.lock_bus(pdMS_TO_TICKS(60000))) {
        ESP_LOGE(TAG, "display_file: bus lock for decode failed");
        return false;
    }
    // drawJpgFile reads from SD on the shared SPI bus.
    const bool drawn =
        g_hal.canvas->drawJpgFile(path.c_str(), draw_x, draw_y, 0, 0, 0, 0, scale, scale);
    g_hal.unlock_bus();

    if (!drawn) {
        ESP_LOGE(TAG, "drawJpgFile failed: %s", path.c_str());
        return false;
    }

    if (!push_canvas_to_epd("file")) {
        return false;
    }

    play_done_chime();
    ESP_LOGI(TAG, "display file done %s", path.c_str());
    return true;
}

bool PhotoGallery::display_jpeg_memory(const uint8_t* data, size_t len, int meta_w, int meta_h)
{
    if (!g_hal.canvas) {
        ESP_LOGE(TAG, "display_jpeg_memory: no canvas");
        return false;
    }

    int image_width  = 0;
    int image_height = 0;
    if (!get_image_size_from_memory(data, len, &image_width, &image_height, ".jpg")) {
        ESP_LOGE(TAG, "jpeg size decode failed len=%u", (unsigned)len);
        return false;
    }
    if (meta_w > 0 && meta_h > 0 && (image_width != meta_w || image_height != meta_h)) {
        ESP_LOGW(TAG, "meta %dx%d != jpeg %dx%d", meta_w, meta_h, image_width, image_height);
    }

    const int scr_w = g_hal.canvas->width();
    const int scr_h = g_hal.canvas->height();
    const float scale =
        std::min((float)scr_w / (float)image_width, (float)scr_h / (float)image_height);
    const int draw_x = (scr_w - (int)(image_width * scale)) / 2;
    const int draw_y = (scr_h - (int)(image_height * scale)) / 2;

    ESP_LOGI(TAG, "display jpeg RAM %dx%d len=%u scale=%.2f at (%d,%d)", image_width, image_height,
             (unsigned)len, scale, draw_x, draw_y);

    g_hal.canvas->fillScreen(TFT_WHITE);
    const bool drawn = g_hal.canvas->drawJpg(data, len, draw_x, draw_y, 0, 0, 0, 0, scale, scale);
    if (!drawn) {
        ESP_LOGE(TAG, "drawJpg (RAM) failed len=%u", (unsigned)len);
        return false;
    }

    if (!push_canvas_to_epd("ram")) {
        return false;
    }
    play_done_chime();
    ESP_LOGI(TAG, "display jpeg RAM done");
    return true;
}

void PhotoGallery::display_index(uint16_t index)
{
    if (photos_.empty()) {
        ESP_LOGI(TAG, "display_index %u — gallery empty, rescanning", (unsigned)index);
        scan();
    }
    if (photos_.empty()) {
        ESP_LOGW(TAG, "display_index aborted — no photos on SD");
        return;
    }
    index %= (uint16_t)photos_.size();
    current_index_ = index;
    ESP_LOGI(TAG, "display_index %u/%u -> %s", (unsigned)index, (unsigned)photos_.size(),
             photos_[index].c_str());
    display_file(photos_[index]);
}

void PhotoGallery::display_most_recent()
{
    ESP_LOGI(TAG, "display_most_recent");
    scan();
    if (!photos_.empty()) {
        display_index((uint16_t)(photos_.size() - 1));
    } else {
        ESP_LOGW(TAG, "display_most_recent — no photos");
    }
}

void PhotoGallery::next_photo()
{
    if (photos_.empty()) {
        scan();
    }
    if (photos_.empty()) {
        ESP_LOGW(TAG, "next_photo — gallery empty (sd_mounted=%d)",
                 (int)photo_storage_is_mounted());
        return;
    }
    const uint16_t next = (uint16_t)((current_index_ + 1) % photos_.size());
    ESP_LOGI(TAG, "next_photo %u -> %u (count=%u)", (unsigned)current_index_, (unsigned)next,
             (unsigned)photos_.size());
    display_index(next);
}

void PhotoGallery::prev_photo()
{
    if (photos_.empty()) {
        scan();
    }
    if (photos_.empty()) {
        ESP_LOGW(TAG, "prev_photo — gallery empty (sd_mounted=%d)",
                 (int)photo_storage_is_mounted());
        return;
    }
    const uint16_t n    = (uint16_t)photos_.size();
    const uint16_t prev = (uint16_t)((current_index_ + n - 1) % n);
    ESP_LOGI(TAG, "prev_photo %u -> %u (count=%u)", (unsigned)current_index_, (unsigned)prev,
             (unsigned)n);
    display_index(prev);
}

void PhotoGallery::handle_buttons()
{
    if (refresh_busy()) {
        return;
    }

    M5.update();
    const bool btn_a = M5.BtnA.wasPressed();
    const bool btn_b = M5.BtnB.wasPressed();
    const bool btn_c = M5.BtnC.wasPressed();

    if (btn_a) {
        ESP_LOGI(TAG, "BtnA pressed (unused) index=%u count=%u", (unsigned)current_index_,
                 (unsigned)photos_.size());
    }
    if (btn_c) {
        last_button_ms_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
        ESP_LOGI(TAG, "BtnC (prev) pressed t=%lu index=%u count=%u sd=%d",
                 (unsigned long)last_button_ms_, (unsigned)current_index_, (unsigned)photos_.size(),
                 (int)photo_storage_is_mounted());
        play_nav_tone(119);
        prev_photo();
    }
    if (btn_b) {
        last_button_ms_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
        ESP_LOGI(TAG, "BtnB (next) pressed t=%lu index=%u count=%u sd=%d",
                 (unsigned long)last_button_ms_, (unsigned)current_index_, (unsigned)photos_.size(),
                 (int)photo_storage_is_mounted());
        play_nav_tone(120);
        next_photo();
    }
}

void PhotoGallery::service()
{
    if (!pending_mu_) {
        pending_mu_ = xSemaphoreCreateMutex();
        if (!pending_mu_) {
            ESP_LOGE(TAG, "pending mutex create failed");
            return;
        }
    }

    if (xSemaphoreTake(pending_mu_, 0) != pdTRUE) {
        return;
    }
    if (!pending_) {
        xSemaphoreGive(pending_mu_);
        return;
    }

    const std::string path     = pending_path_;
    uint8_t*          jpeg     = pending_jpeg_;
    const size_t      jpeg_len = pending_jpeg_len_;
    const int         meta_w   = pending_meta_w_;
    const int         meta_h   = pending_meta_h_;
    pending_                   = false;
    pending_path_.clear();
    pending_jpeg_     = nullptr;
    pending_jpeg_len_ = 0;
    xSemaphoreGive(pending_mu_);

    ESP_LOGI(TAG, "service: running queued display path=%s ram_len=%u",
             path.empty() ? "(none)" : path.c_str(), (unsigned)jpeg_len);

    bool ok = false;
    if (!path.empty()) {
        ok = display_file(path);
    } else if (jpeg && jpeg_len > 0) {
        ok = display_jpeg_memory(jpeg, jpeg_len, meta_w, meta_h);
    } else {
        ESP_LOGE(TAG, "service: empty queue job");
    }

    if (jpeg) {
        heap_caps_free(jpeg);
    }

    if (!ok) {
        ESP_LOGE(TAG, "service: queued display failed");
    }
}

esp_err_t PhotoGallery::save_and_display_jpeg(const uint8_t* data, size_t len, int meta_w, int meta_h,
                                              std::string* saved_name)
{
    if (!pending_mu_) {
        pending_mu_ = xSemaphoreCreateMutex();
        if (!pending_mu_) {
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_LOGI(TAG, "save_and_queue start len=%u meta=%dx%d busy=%d sd_mounted=%d inserted=%d",
             (unsigned)len, meta_w, meta_h, (int)refresh_busy(), (int)photo_storage_is_mounted(),
             (int)g_hal.sd_inserted());

    if (refresh_busy()) {
        ESP_LOGW(TAG, "save rejected — refresh/queue busy");
        return ESP_ERR_INVALID_STATE;
    }

    if (!data || len == 0 || len > PHOTO_FRAME_MAX_JPEG_BYTES) {
        ESP_LOGE(TAG, "save rejected — bad size len=%u max=%u", (unsigned)len,
                 (unsigned)PHOTO_FRAME_MAX_JPEG_BYTES);
        return ESP_ERR_INVALID_SIZE;
    }
    if (len < 3 || data[0] != 0xFF || data[1] != 0xD8 || data[2] != 0xFF) {
        ESP_LOGE(TAG, "save rejected — not JPEG magic %02X %02X %02X", data[0], data[1], data[2]);
        return ESP_ERR_INVALID_ARG;
    }

    play_receive_chime();

    std::string path;
    if (photo_storage_is_mounted()) {
        if (!g_hal.lock_bus(pdMS_TO_TICKS(60000))) {
            ESP_LOGE(TAG, "save: bus lock timeout");
            return ESP_FAIL;
        }

        path = next_filename();
        if (path.empty()) {
            g_hal.unlock_bus();
            ESP_LOGE(TAG, "save failed — no free filename");
            return ESP_ERR_NO_MEM;
        }

        FILE* f = fopen(path.c_str(), "wb");
        if (!f) {
            ESP_LOGE(TAG, "fopen %s failed errno=%d", path.c_str(), errno);
            g_hal.unlock_bus();
            return ESP_FAIL;
        }
        const size_t written = fwrite(data, 1, len, f);
        const int flush_err  = fflush(f);
        const int close_err  = fclose(f);
        ESP_LOGI(TAG, "wrote %s bytes=%u/%u fflush=%d fclose=%d", path.c_str(), (unsigned)written,
                 (unsigned)len, flush_err, close_err);

        struct stat st {};
        if (stat(path.c_str(), &st) == 0) {
            ESP_LOGI(TAG, "verify %s size=%ld", path.c_str(), (long)st.st_size);
        } else {
            ESP_LOGW(TAG, "verify stat failed %s errno=%d", path.c_str(), errno);
        }
        g_hal.unlock_bus();

        if (written != len || flush_err != 0 || close_err != 0) {
            ESP_LOGE(TAG, "save write incomplete errno=%d", errno);
            return ESP_FAIL;
        }

        scan();
        bool found = false;
        for (uint16_t i = 0; i < photos_.size(); ++i) {
            if (photos_[i] == path) {
                current_index_ = i;
                found          = true;
                break;
            }
        }
        if (!found) {
            ESP_LOGW(TAG, "saved %s but scan did not list it (count=%u)", path.c_str(),
                     (unsigned)photos_.size());
        } else {
            ESP_LOGI(TAG, "saved listed as index %u/%u", (unsigned)current_index_,
                     (unsigned)photos_.size());
        }

        if (saved_name) {
            const char* base = strrchr(path.c_str(), '/');
            *saved_name      = base ? (base + 1) : path;
        }
    } else {
        ESP_LOGW(TAG, "SD not mounted — queue RAM display only (inserted=%d)",
                 (int)g_hal.sd_inserted());
        current_index_ = 0;
        if (saved_name) {
            *saved_name = "";
        }
    }

    // Queue e-ink work for the main loop (HTTP task must not pushSprite / decode on 8–32K stack).
    uint8_t* ram_copy = nullptr;
    if (path.empty()) {
        ram_copy = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!ram_copy) {
            ram_copy = (uint8_t*)malloc(len);
        }
        if (!ram_copy) {
            ESP_LOGE(TAG, "queue: OOM copying JPEG (%u)", (unsigned)len);
            return ESP_ERR_NO_MEM;
        }
        memcpy(ram_copy, data, len);
    }

    if (xSemaphoreTake(pending_mu_, pdMS_TO_TICKS(1000)) != pdTRUE) {
        if (ram_copy) {
            heap_caps_free(ram_copy);
        }
        return ESP_ERR_INVALID_STATE;
    }
    if (pending_) {
        xSemaphoreGive(pending_mu_);
        if (ram_copy) {
            heap_caps_free(ram_copy);
        }
        ESP_LOGW(TAG, "queue rejected — already pending");
        return ESP_ERR_INVALID_STATE;
    }
    pending_path_     = path;
    pending_jpeg_     = ram_copy;
    pending_jpeg_len_ = ram_copy ? len : 0;
    pending_meta_w_   = meta_w;
    pending_meta_h_   = meta_h;
    pending_          = true;
    xSemaphoreGive(pending_mu_);

    ESP_LOGI(TAG, "queued display saved=%s index=%u count=%u (await service)",
             saved_name ? saved_name->c_str() : "?", (unsigned)current_index_,
             (unsigned)photos_.size());
    return ESP_OK;
}
