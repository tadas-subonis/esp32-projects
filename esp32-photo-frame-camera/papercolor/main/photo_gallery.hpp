#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class PhotoGallery {
public:
    bool refresh_busy() const;
    uint16_t current_index() const { return current_index_; }
    size_t photo_count() const { return photos_.size(); }

    void scan();
    void display_index(uint16_t index);
    void next_photo();
    void prev_photo();
    void handle_buttons();

    /**
     * Validate + save JPEG (SD when mounted), queue e-ink refresh for service().
     * Safe to call from the HTTP task — does not push the panel here.
     */
    esp_err_t save_and_display_jpeg(const uint8_t* data, size_t len, int meta_w, int meta_h,
                                    std::string* saved_name);

    /** Run from the main loop: perform any queued e-ink refresh. */
    void service();

    void display_most_recent();

private:
    bool display_jpeg_memory(const uint8_t* data, size_t len, int meta_w, int meta_h);
    bool display_file(const std::string& path);
    void play_nav_tone(int midi_note) const;
    void play_receive_chime() const;
    void play_done_chime() const;
    std::string next_filename() const;
    bool push_canvas_to_epd(const char* reason);

    std::vector<std::string> photos_;
    uint16_t                 current_index_       = 0;
    bool                     refresh_in_progress_ = false;
    uint32_t                 last_button_ms_      = 0;

    // Queued display job (filled by HTTP task, consumed by service()).
    SemaphoreHandle_t pending_mu_     = nullptr;
    bool              pending_        = false;
    std::string       pending_path_;
    uint8_t*          pending_jpeg_   = nullptr;
    size_t            pending_jpeg_len_ = 0;
    int               pending_meta_w_ = 0;
    int               pending_meta_h_ = 0;
};

extern PhotoGallery g_gallery;
