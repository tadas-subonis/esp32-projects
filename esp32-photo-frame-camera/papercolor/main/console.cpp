#include "console.hpp"

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "argtable3/argtable3.h"
#include "esp_app_desc.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hal.hpp"
#include "photo_frame_wifi.h"
#include "photo_gallery.hpp"
#include "storage.hpp"

static const char* TAG = "console";

static void print_result(const char* json)
{
    printf("<<< %s\n", json);
    fflush(stdout);
}

static int get_ap_clients()
{
    wifi_sta_list_t sta{};
    if (esp_wifi_ap_get_sta_list(&sta) != ESP_OK) {
        return 0;
    }
    return (int)sta.num;
}

static int cmd_status(int, char**)
{
    const bool sd_mounted = photo_storage_is_mounted();
    const bool sd_inserted = g_hal.sd_inserted();
    const bool refresh_busy = g_gallery.refresh_busy();
    const uint32_t heap_free = esp_get_free_heap_size();
    const uint64_t uptime_ms = (uint64_t)(esp_timer_get_time() / 1000);
    const int ap_clients = get_ap_clients();
    const unsigned current_index = (unsigned)g_gallery.current_index();
    const unsigned photo_count = (unsigned)g_gallery.photo_count();

    ESP_LOGI(TAG, "status sd_mounted=%d sd_inserted=%d epd_busy=%d photos=%u index=%u busy=%d heap=%u ap=%d",
             (int)sd_mounted, (int)sd_inserted, (int)g_hal.epd_busy(), photo_count, current_index,
             (int)refresh_busy, (unsigned)heap_free, ap_clients);

    char json[352];
    snprintf(
        json,
        sizeof(json),
        "{\"cmd\":\"status\",\"ok\":true,\"sd_mounted\":%s,\"sd_inserted\":%s,\"epd_busy\":%s,\"refresh_busy\":%s,\"current_index\":%u,\"photo_count\":%u,\"heap_free\":%u,\"uptime_ms\":%llu,\"ap_clients\":%d}",
        sd_mounted ? "true" : "false",
        sd_inserted ? "true" : "false",
        g_hal.epd_busy() ? "true" : "false",
        refresh_busy ? "true" : "false",
        current_index,
        photo_count,
        (unsigned)heap_free,
        (unsigned long long)uptime_ms,
        ap_clients
    );
    print_result(json);
    return 0;
}

static int cmd_version(int, char**)
{
    const esp_app_desc_t* desc = esp_app_get_description();
    char json[256];
    snprintf(
        json,
        sizeof(json),
        "{\"cmd\":\"version\",\"ok\":true,\"project\":\"%s\",\"version\":\"%s\",\"idf\":\"%s\"}",
        desc ? desc->project_name : "unknown",
        desc ? desc->version : "unknown",
        desc ? desc->idf_ver : "unknown"
    );
    print_result(json);
    return 0;
}

static int cmd_home(int, char**)
{
    if (g_gallery.refresh_busy()) {
        print_result("{\"cmd\":\"home\",\"ok\":false,\"error\":\"busy\"}");
        return 1;
    }

    char ap_line[64];
    snprintf(ap_line, sizeof(ap_line), "AP: %s", PHOTO_FRAME_AP_SSID);
    const char* sd_line = !photo_storage_is_mounted()
                              ? (g_hal.sd_inserted() ? "SD mount failed" : "No SD card")
                              : (g_gallery.photo_count() == 0 ? "SD empty — waiting for photo"
                                                             : "gallery ready");
    ESP_LOGI(TAG, "home: forcing status screen refresh");
    g_hal.show_status_screen("photo_frame_receiver", ap_line, sd_line);
    print_result("{\"cmd\":\"home\",\"ok\":true}");
    return 0;
}

static int cmd_epdtest(int, char**)
{
    const int64_t dt_ms = g_hal.show_test_pattern();
    if (dt_ms < 0) {
        print_result("{\"cmd\":\"epdtest\",\"ok\":false,\"error\":\"refresh_failed\"}");
        return 1;
    }
    char json[128];
    snprintf(json, sizeof(json), "{\"cmd\":\"epdtest\",\"ok\":true,\"refresh_ms\":%lld}",
             (long long)dt_ms);
    print_result(json);
    return 0;
}

static int cmd_gallery(int, char**)
{
    if (!photo_storage_is_mounted()) {
        ESP_LOGW(TAG, "gallery: sd_not_mounted inserted=%d", (int)g_hal.sd_inserted());
        print_result("{\"cmd\":\"gallery\",\"ok\":false,\"error\":\"sd_not_mounted\"}");
        return 1;
    }

    if (!g_hal.lock_bus(pdMS_TO_TICKS(60000))) {
        print_result("{\"cmd\":\"gallery\",\"ok\":false,\"error\":\"bus_busy\"}");
        return 1;
    }

    DIR* dir = opendir(PHOTO_STORAGE_BASE_PATH);
    if (!dir) {
        g_hal.unlock_bus();
        ESP_LOGE(TAG, "gallery: opendir failed");
        print_result("{\"cmd\":\"gallery\",\"ok\":false,\"error\":\"opendir_failed\"}");
        return 1;
    }

    printf("I gallery: listing %s (in_memory_count=%u)\n", PHOTO_STORAGE_BASE_PATH,
           (unsigned)g_gallery.photo_count());
    struct dirent* ent;
    int count = 0;
    int seen  = 0;
    while ((ent = readdir(dir)) != nullptr) {
        ++seen;
        printf("I gallery: entry %s d_type=%u\n", ent->d_name, (unsigned)ent->d_type);
        if (ent->d_type == DT_DIR) {
            continue;
        }
        if (strncmp(ent->d_name, "image", 5) != 0) {
            continue;
        }
        const char* dot = strrchr(ent->d_name, '.');
        if (!dot) {
            continue;
        }
        if (strcasecmp(dot, ".jpg") != 0 && strcasecmp(dot, ".jpeg") != 0) {
            continue;
        }
        count++;
    }
    closedir(dir);
    g_hal.unlock_bus();

    ESP_LOGI(TAG, "gallery listed count=%d dir_entries=%d memory_count=%u index=%u", count, seen,
             (unsigned)g_gallery.photo_count(), (unsigned)g_gallery.current_index());

    char json[160];
    snprintf(json, sizeof(json),
             "{\"cmd\":\"gallery\",\"ok\":true,\"count\":%d,\"memory_count\":%u,\"current_index\":%u}",
             count, (unsigned)g_gallery.photo_count(), (unsigned)g_gallery.current_index());
    print_result(json);
    return 0;
}

static int cmd_sdtest(int, char**)
{
    if (!photo_storage_is_mounted()) {
        print_result("{\"cmd\":\"sdtest\",\"ok\":false,\"error\":\"sd_not_mounted\"}");
        return 1;
    }

    const char* path = PHOTO_STORAGE_BASE_PATH "/_sdtest.bin";
    const char  payload[] = "pf-sdtest-ok";

    if (!g_hal.lock_bus(pdMS_TO_TICKS(60000))) {
        print_result("{\"cmd\":\"sdtest\",\"ok\":false,\"error\":\"bus_busy\"}");
        return 1;
    }

    FILE* f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "sdtest fopen write errno=%d", errno);
        g_hal.unlock_bus();
        print_result("{\"cmd\":\"sdtest\",\"ok\":false,\"error\":\"fopen_write\"}");
        return 1;
    }
    const size_t written = fwrite(payload, 1, sizeof(payload), f);
    fclose(f);

    char buf[32] = {0};
    f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "sdtest fopen read errno=%d", errno);
        g_hal.unlock_bus();
        print_result("{\"cmd\":\"sdtest\",\"ok\":false,\"error\":\"fopen_read\"}");
        return 1;
    }
    const size_t nread = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    unlink(path);
    g_hal.unlock_bus();

    const bool ok = written == sizeof(payload) && nread == sizeof(payload) &&
                    memcmp(buf, payload, sizeof(payload)) == 0;
    ESP_LOGI(TAG, "sdtest written=%u read=%u ok=%d", (unsigned)written, (unsigned)nread, (int)ok);
    if (!ok) {
        print_result("{\"cmd\":\"sdtest\",\"ok\":false,\"error\":\"mismatch\"}");
        return 1;
    }
    print_result("{\"cmd\":\"sdtest\",\"ok\":true}");
    return 0;
}

static struct {
    struct arg_int* index;
    struct arg_end* end;
} s_display_args;

static int cmd_display(int argc, char** argv)
{
    const int nerrors = arg_parse(argc, argv, (void**)&s_display_args);
    if (nerrors != 0 || s_display_args.index->count != 1) {
        print_result("{\"cmd\":\"display\",\"ok\":false,\"error\":\"bad_args\"}");
        return 2;
    }

    if (g_gallery.refresh_busy()) {
        print_result("{\"cmd\":\"display\",\"ok\":false,\"error\":\"busy\"}");
        return 1;
    }

    const int idx = s_display_args.index->ival[0];
    if (idx < 0) {
        print_result("{\"cmd\":\"display\",\"ok\":false,\"error\":\"bad_index\"}");
        return 2;
    }

    g_gallery.display_index((uint16_t)idx);

    char json[96];
    snprintf(json, sizeof(json), "{\"cmd\":\"display\",\"ok\":true,\"index\":%d}", idx);
    print_result(json);
    return 0;
}

static struct {
    struct arg_str* tag;
    struct arg_str* level;
    struct arg_end* end;
} s_loglevel_args;

static esp_log_level_t parse_level(const char* s)
{
    if (strcasecmp(s, "none") == 0) return ESP_LOG_NONE;
    if (strcasecmp(s, "error") == 0) return ESP_LOG_ERROR;
    if (strcasecmp(s, "warn") == 0) return ESP_LOG_WARN;
    if (strcasecmp(s, "info") == 0) return ESP_LOG_INFO;
    if (strcasecmp(s, "debug") == 0) return ESP_LOG_DEBUG;
    if (strcasecmp(s, "verbose") == 0) return ESP_LOG_VERBOSE;
    return ESP_LOG_INFO;
}

static int cmd_loglevel(int argc, char** argv)
{
    const int nerrors = arg_parse(argc, argv, (void**)&s_loglevel_args);
    if (nerrors != 0 || s_loglevel_args.tag->count != 1 || s_loglevel_args.level->count != 1) {
        print_result("{\"cmd\":\"loglevel\",\"ok\":false,\"error\":\"bad_args\"}");
        return 2;
    }

    const char* tag = s_loglevel_args.tag->sval[0];
    const char* level_s = s_loglevel_args.level->sval[0];
    const esp_log_level_t lvl = parse_level(level_s);
    esp_log_level_set(tag, lvl);

    char json[160];
    snprintf(
        json,
        sizeof(json),
        "{\"cmd\":\"loglevel\",\"ok\":true,\"tag\":\"%s\",\"level\":\"%s\"}",
        tag,
        level_s
    );
    print_result(json);
    return 0;
}

static int cmd_pins(int, char**)
{
    // Read-only: GPIO43/44 are the panel's DC/CS outputs. Reconfiguring them as
    // inputs breaks the display until the next reset.
    const int busy = gpio_get_level(GPIO_NUM_11);
    const int dc   = gpio_get_level(GPIO_NUM_43);
    const int cs   = gpio_get_level(GPIO_NUM_44);
    ESP_LOGI(TAG, "pins EPD busy(11)=%d dc(43)=%d cs(44)=%d", busy, dc, cs);
    char json[200];
    snprintf(json, sizeof(json), "{\"cmd\":\"pins\",\"ok\":true,\"busy_pin\":%d,\"dc_pin\":%d,\"cs_pin\":%d}",
             busy, dc, cs);
    print_result(json);
    return 0;
}

static void register_commands()
{
    esp_console_cmd_t pins_cmd = {
        .command = "pins",
        .help = "Print EPD busy/rst/cs pin levels",
        .hint = nullptr,
        .func = &cmd_pins,
        .argtable = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&pins_cmd));

    esp_console_cmd_t epdtest_cmd = {
        .command = "epdtest",
        .help = "Draw a colour-bar test pattern and time the refresh",
        .hint = nullptr,
        .func = &cmd_epdtest,
        .argtable = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&epdtest_cmd));

    esp_console_cmd_t status_cmd = {
        .command = "status",
        .help = "Print device status",
        .hint = nullptr,
        .func = &cmd_status,
        .argtable = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&status_cmd));

    esp_console_cmd_t version_cmd = {
        .command = "version",
        .help = "Print build version info",
        .hint = nullptr,
        .func = &cmd_version,
        .argtable = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&version_cmd));

    esp_console_cmd_t home_cmd = {
        .command = "home",
        .help = "Force e-ink status/home screen refresh",
        .hint = nullptr,
        .func = &cmd_home,
        .argtable = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&home_cmd));

    esp_console_cmd_t gallery_cmd = {
        .command = "gallery",
        .help = "List photos on SD",
        .hint = nullptr,
        .func = &cmd_gallery,
        .argtable = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&gallery_cmd));

    esp_console_cmd_t sdtest_cmd = {
        .command = "sdtest",
        .help = "Write/read/delete a tiny file on SD",
        .hint = nullptr,
        .func = &cmd_sdtest,
        .argtable = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&sdtest_cmd));

    s_display_args.index = arg_int1(nullptr, nullptr, "<index>", "Photo index to display");
    s_display_args.end = arg_end(2);
    static void* display_argtable[] = {s_display_args.index, s_display_args.end};
    esp_console_cmd_t display_cmd = {
        .command = "display",
        .help = "Display a photo by index",
        .hint = nullptr,
        .func = &cmd_display,
        .argtable = display_argtable,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&display_cmd));

    s_loglevel_args.tag = arg_str1(nullptr, nullptr, "<tag>", "Log tag (or '*')");
    s_loglevel_args.level = arg_str1(nullptr, nullptr, "<level>", "none|error|warn|info|debug|verbose");
    s_loglevel_args.end = arg_end(2);
    static void* loglevel_argtable[] = {s_loglevel_args.tag, s_loglevel_args.level, s_loglevel_args.end};
    esp_console_cmd_t loglevel_cmd = {
        .command = "loglevel",
        .help = "Set log level for a tag",
        .hint = nullptr,
        .func = &cmd_loglevel,
        .argtable = loglevel_argtable,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&loglevel_cmd));
}

static void console_task(void*)
{
    esp_console_register_help_command();
    register_commands();

    // USB Serial/JTAG: driver must be installed for host→device (agent cmd) RX.
    usb_serial_jtag_driver_config_t usb_cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_cfg));
    usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_LF);
    usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_LF);
    usb_serial_jtag_vfs_use_driver();

    ESP_LOGI(TAG, "REPL started (USB Serial/JTAG). Type 'help'.");

    char line[256];
    size_t len = 0;
    while (true) {
        uint8_t ch = 0;
        const int n = usb_serial_jtag_read_bytes(&ch, 1, pdMS_TO_TICKS(50));
        if (n <= 0) {
            continue;
        }
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            line[len] = '\0';
            if (len > 0) {
                int ret = 0;
                const esp_err_t err = esp_console_run(line, &ret);
                if (err == ESP_ERR_NOT_FOUND) {
                    print_result("{\"cmd\":\"unknown\",\"ok\":false,\"error\":\"unknown_command\"}");
                } else if (err == ESP_ERR_INVALID_ARG) {
                    print_result("{\"cmd\":\"invalid\",\"ok\":false,\"error\":\"invalid_args\"}");
                } else if (err != ESP_OK) {
                    char json[160];
                    snprintf(json, sizeof(json), "{\"cmd\":\"error\",\"ok\":false,\"error\":\"%s\"}", esp_err_to_name(err));
                    print_result(json);
                }
            }
            len = 0;
            continue;
        }
        if (len + 1 >= sizeof(line)) {
            len = 0;
            continue;
        }
        line[len++] = (char)ch;
    }
}

esp_err_t console_start()
{
    esp_console_config_t console_cfg = ESP_CONSOLE_CONFIG_DEFAULT();
    console_cfg.max_cmdline_length = 256;
    console_cfg.max_cmdline_args = 8;
    const esp_err_t init_err = esp_console_init(&console_cfg);
    if (init_err != ESP_OK) {
        ESP_LOGE(TAG, "console init failed: %s", esp_err_to_name(init_err));
        return init_err;
    }

    xTaskCreate(console_task, "console", 4096, nullptr, 5, nullptr);
    return ESP_OK;
}

