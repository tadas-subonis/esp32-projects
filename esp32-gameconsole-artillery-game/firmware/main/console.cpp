#include "console.hpp"

#include "game_app.hpp"
#include "device_hal.hpp"

#include "artillery/command.hpp"
#include "artillery/config.hpp"
#include "artillery/match.hpp"

#include "argtable3/argtable3.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "esp_app_desc.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {
const char* TAG = "console";

using tc::g_app;
using tc::g_hal;

void print_result(const char* json)
{
    printf("<<< %s\n", json);
    fflush(stdout);
}

constexpr int kSnapScale = 4;
constexpr int kSnapW = artillery::kWidth / kSnapScale;
constexpr int kSnapH = artillery::kHeight / kSnapScale;
constexpr int kSnapBytes = kSnapW * kSnapH * 3;
uint8_t s_snap_rgb[kSnapBytes];

void print_state(const char* cmd)
{
    g_app.lock();
    const artillery::ViewModel view = g_app.view();
    const artillery::MatchSnapshot s = view.snap;
    const int fps = g_app.fps();
    const unsigned compose_ms = g_app.compose_ms();
    const unsigned flush_ms = g_app.flush_ms();
    const unsigned frame_ms = g_app.frame_ms();
    const unsigned dirty_px = g_app.dirty_px();
    const int seat = g_app.seat();
    const char* net = g_app.net_status();
    const char* ip = g_app.ip();
    g_app.unlock();
    char json[640];
    snprintf(json, sizeof(json),
             "{\"cmd\":\"%s\",\"ok\":true,\"phase\":\"%s\",\"seed\":%lu,\"turn\":%d,\"active\":%d,"
             "\"angle\":%d,\"power\":%d,\"wind\":%d,\"hp0\":%d,\"hp1\":%d,\"heap_free\":%u,"
             "\"display\":%s,\"uptime_ms\":%llu,\"fps\":%d,\"compose_ms\":%u,\"flush_ms\":%u,"
             "\"frame_ms\":%u,\"dirty_px\":%u,\"net\":\"%s\",\"seat\":%d,\"ip\":\"%s\",\"players\":%d}",
             cmd, artillery::phase_name(s.phase), static_cast<unsigned long>(s.seed), s.turn,
             static_cast<int>(s.active), s.angle, s.power, s.wind, s.hp[0], s.hp[1],
             static_cast<unsigned>(esp_get_free_heap_size()), g_hal.display_ready() ? "true" : "false",
             static_cast<unsigned long long>(esp_timer_get_time() / 1000), fps, compose_ms, flush_ms,
             frame_ms, dirty_px, net ? net : "off", seat, ip ? ip : "", view.players);
    print_result(json);
}

int cmd_status(int, char**)
{
    print_state("status");
    return 0;
}

int cmd_version(int, char**)
{
    const esp_app_desc_t* desc = esp_app_get_description();
    char json[256];
    snprintf(json, sizeof(json),
             "{\"cmd\":\"version\",\"ok\":true,\"project\":\"%s\",\"version\":\"%s\",\"idf\":\"%s\"}",
             desc ? desc->project_name : "artillery", desc ? desc->version : "0",
             desc ? desc->idf_ver : "unknown");
    print_result(json);
    return 0;
}

int cmd_state(int, char**)
{
    print_state("state");
    return 0;
}

struct {
    struct arg_int* seed;
    struct arg_end* end;
} new_args;

int cmd_new(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**)&new_args);
    if (nerrors > 0) {
        print_result("{\"cmd\":\"new\",\"ok\":false,\"error\":\"invalid_args\"}");
        return 1;
    }
    g_app.lock();
    uint32_t seed = g_app.view().snap.seed;
    if (new_args.seed->count > 0) {
        seed = static_cast<uint32_t>(new_args.seed->ival[0]);
    }
    artillery::ClientIntent start;
    start.kind = artillery::ClientIntent::Start;
    start.seed = seed;
    start.value = g_app.online() ? static_cast<int>(artillery::Mode::Pvp)
                                 : static_cast<int>(artillery::Mode::VsBot);
    g_app.submit(start);
    g_app.present();
    g_app.unlock();
    print_state("new");
    return 0;
}

struct {
    struct arg_int* value;
    struct arg_end* end;
} angle_args;

int cmd_angle(int argc, char** argv)
{
    if (arg_parse(argc, argv, (void**)&angle_args) > 0 || angle_args.value->count == 0) {
        print_result("{\"cmd\":\"angle\",\"ok\":false,\"error\":\"invalid_args\"}");
        return 1;
    }
    g_app.lock();
    g_app.submit(artillery::ClientIntent{artillery::ClientIntent::SetAngle, angle_args.value->ival[0]});
    g_app.present();
    g_app.unlock();
    print_state("angle");
    return 0;
}

struct {
    struct arg_int* value;
    struct arg_end* end;
} power_args;

int cmd_power(int argc, char** argv)
{
    if (arg_parse(argc, argv, (void**)&power_args) > 0 || power_args.value->count == 0) {
        print_result("{\"cmd\":\"power\",\"ok\":false,\"error\":\"invalid_args\"}");
        return 1;
    }
    g_app.lock();
    g_app.submit(artillery::ClientIntent{artillery::ClientIntent::SetPower, power_args.value->ival[0]});
    g_app.present();
    g_app.unlock();
    print_state("power");
    return 0;
}

int cmd_fire(int, char**)
{
    g_app.lock();
    if (g_app.online()) {
        g_app.submit(artillery::ClientIntent{artillery::ClientIntent::Fire});
        g_app.present();
        g_app.unlock();
        print_state("fire");
        return 0;
    }
    if (!g_app.match().fire_current()) {
        g_app.unlock();
        print_result("{\"cmd\":\"fire\",\"ok\":false,\"error\":\"not_aiming\"}");
        return 1;
    }
    for (int i = 0; i < 4000 && g_app.match().phase() == artillery::Phase::Firing; ++i) {
        g_app.match().tick(16);
    }
    g_app.present();
    g_app.unlock();
    print_state("fire");
    return 0;
}

struct {
    struct arg_int* n;
    struct arg_end* end;
} tick_args;

int cmd_tick(int argc, char** argv)
{
    int frames = 1;
    if (arg_parse(argc, argv, (void**)&tick_args) == 0 && tick_args.n->count > 0) {
        frames = tick_args.n->ival[0];
    }
    for (int i = 0; i < frames; ++i) {
        g_app.tick(16);
    }
    print_state("tick");
    return 0;
}

int cmd_snap(int, char**)
{
    g_app.lock();
    g_app.present();
    const uint16_t* fb = g_app.frame();
    if (fb == nullptr) {
        g_app.unlock();
        print_result("{\"cmd\":\"snap\",\"ok\":false,\"error\":\"no_framebuffer\"}");
        return 1;
    }
    uint8_t* dst = s_snap_rgb;
    for (int y = 0; y < kSnapH; ++y) {
        const uint16_t* row = fb + (y * kSnapScale) * artillery::kWidth;
        for (int x = 0; x < kSnapW; ++x) {
            const uint16_t p = row[x * kSnapScale];
            dst[0] = static_cast<uint8_t>(((p >> 11) & 0x1F) << 3);
            dst[1] = static_cast<uint8_t>(((p >> 5) & 0x3F) << 2);
            dst[2] = static_cast<uint8_t>((p & 0x1F) << 3);
            dst += 3;
        }
    }
    const artillery::MatchSnapshot s = g_app.view().snap;
    const int fps = g_app.fps();
    const unsigned compose_ms = g_app.compose_ms();
    const unsigned flush_ms = g_app.flush_ms();
    const unsigned frame_ms = g_app.frame_ms();
    const unsigned dirty_px = g_app.dirty_px();
    char json[512];
    snprintf(json, sizeof(json),
             "{\"cmd\":\"snap\",\"ok\":true,\"w\":%d,\"h\":%d,\"scale\":%d,\"bytes\":%d,"
             "\"phase\":\"%s\",\"fps\":%d,\"compose_ms\":%u,\"flush_ms\":%u,\"frame_ms\":%u,"
             "\"dirty_px\":%u,\"heap_free\":%u}",
             kSnapW, kSnapH, kSnapScale, kSnapBytes, artillery::phase_name(s.phase), fps, compose_ms,
             flush_ms, frame_ms, dirty_px, static_cast<unsigned>(esp_get_free_heap_size()));
    print_result(json);
    printf(">>>SNAP\n");
    fflush(stdout);
    fsync(fileno(stdout));

    const uart_port_t uart = static_cast<uart_port_t>(CONFIG_ESP_CONSOLE_UART_NUM);
    const uint8_t* p = s_snap_rgb;
    int left = kSnapBytes;
    while (left > 0) {
        const int n = uart_write_bytes(uart, p, static_cast<size_t>(left));
        if (n <= 0) {
            break;
        }
        p += n;
        left -= n;
    }
    uart_wait_tx_done(uart, pdMS_TO_TICKS(8000));
    printf("\n<<<SNAP_END\n");
    fflush(stdout);
    g_app.unlock();
    return left == 0 ? 0 : 1;
}

void register_commands()
{
    const esp_console_cmd_t status_cmd = {
        .command = "status",
        .help = "Match + heap JSON",
        .hint = nullptr,
        .func = &cmd_status,
        .argtable = nullptr,
        .func_w_context = nullptr,
        .context = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&status_cmd));

    const esp_console_cmd_t version_cmd = {
        .command = "version",
        .help = "Firmware version",
        .hint = nullptr,
        .func = &cmd_version,
        .argtable = nullptr,
        .func_w_context = nullptr,
        .context = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&version_cmd));

    const esp_console_cmd_t state_cmd = {
        .command = "state",
        .help = "Dump match snapshot",
        .hint = nullptr,
        .func = &cmd_state,
        .argtable = nullptr,
        .func_w_context = nullptr,
        .context = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&state_cmd));

    new_args.seed = arg_int0(nullptr, nullptr, "<seed>", "map seed");
    new_args.end = arg_end(2);
    static void* new_argtable[] = {new_args.seed, new_args.end};
    const esp_console_cmd_t new_cmd = {
        .command = "new",
        .help = "Start a match vs bot",
        .hint = nullptr,
        .func = &cmd_new,
        .argtable = new_argtable,
        .func_w_context = nullptr,
        .context = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&new_cmd));

    angle_args.value = arg_int1(nullptr, nullptr, "<deg>", "aim angle");
    angle_args.end = arg_end(2);
    static void* angle_argtable[] = {angle_args.value, angle_args.end};
    const esp_console_cmd_t angle_cmd = {
        .command = "angle",
        .help = "Set aim angle",
        .hint = nullptr,
        .func = &cmd_angle,
        .argtable = angle_argtable,
        .func_w_context = nullptr,
        .context = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&angle_cmd));

    power_args.value = arg_int1(nullptr, nullptr, "<n>", "power 8-100");
    power_args.end = arg_end(2);
    static void* power_argtable[] = {power_args.value, power_args.end};
    const esp_console_cmd_t power_cmd = {
        .command = "power",
        .help = "Set shot power",
        .hint = nullptr,
        .func = &cmd_power,
        .argtable = power_argtable,
        .func_w_context = nullptr,
        .context = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&power_cmd));

    const esp_console_cmd_t fire_cmd = {
        .command = "fire",
        .help = "Fire current aim and resolve flight",
        .hint = nullptr,
        .func = &cmd_fire,
        .argtable = nullptr,
        .func_w_context = nullptr,
        .context = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&fire_cmd));

    tick_args.n = arg_int0(nullptr, nullptr, "<frames>", "frames of 16ms");
    tick_args.end = arg_end(2);
    static void* tick_argtable[] = {tick_args.n, tick_args.end};
    const esp_console_cmd_t tick_cmd = {
        .command = "tick",
        .help = "Advance the game loop",
        .hint = nullptr,
        .func = &cmd_tick,
        .argtable = tick_argtable,
        .func_w_context = nullptr,
        .context = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&tick_cmd));

    const esp_console_cmd_t snap_cmd = {
        .command = "snap",
        .help = "Dump 120x80 RGB888 screenshot over UART",
        .hint = nullptr,
        .func = &cmd_snap,
        .argtable = nullptr,
        .func_w_context = nullptr,
        .context = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&snap_cmd));
}

void console_task(void*)
{
    esp_console_register_help_command();
    register_commands();

    const uart_port_t uart = static_cast<uart_port_t>(CONFIG_ESP_CONSOLE_UART_NUM);
    fflush(stdout);
    fsync(fileno(stdout));
    setvbuf(stdin, nullptr, _IONBF, 0);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    uart_vfs_dev_port_set_rx_line_endings(uart, ESP_LINE_ENDINGS_LF);
    uart_vfs_dev_port_set_tx_line_endings(uart, ESP_LINE_ENDINGS_CRLF);

    uart_config_t uart_cfg = {};
    uart_cfg.baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE;
    uart_cfg.data_bits = UART_DATA_8_BITS;
    uart_cfg.parity = UART_PARITY_DISABLE;
    uart_cfg.stop_bits = UART_STOP_BITS_1;
    uart_cfg.source_clk = UART_SCLK_DEFAULT;
    ESP_ERROR_CHECK(uart_param_config(uart, &uart_cfg));
    ESP_ERROR_CHECK(uart_driver_install(uart, 2048, 4096, 0, nullptr, 0));
    uart_vfs_dev_use_driver(uart);

    ESP_LOGI(TAG, "REPL started (UART). Type 'help'.");

    char line[256];
    size_t len = 0;
    while (true) {
        uint8_t ch = 0;
        const int n = uart_read_bytes(uart, &ch, 1, pdMS_TO_TICKS(50));
        if (n <= 0) {
            continue;
        }
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            line[len] = '\0';
            if (len > 0) {
                ESP_LOGI(TAG, "uart> %s", line);
                int ret = 0;
                const esp_err_t err = esp_console_run(line, &ret);
                if (err == ESP_ERR_NOT_FOUND) {
                    print_result("{\"cmd\":\"unknown\",\"ok\":false,\"error\":\"unknown_command\"}");
                } else if (err == ESP_ERR_INVALID_ARG) {
                    print_result("{\"cmd\":\"invalid\",\"ok\":false,\"error\":\"invalid_args\"}");
                } else if (err != ESP_OK) {
                    char json[160];
                    snprintf(json, sizeof(json), "{\"cmd\":\"error\",\"ok\":false,\"error\":\"%s\"}",
                             esp_err_to_name(err));
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
        line[len++] = static_cast<char>(ch);
    }
}

}  // namespace

esp_err_t console_start()
{
    esp_console_config_t console_cfg = ESP_CONSOLE_CONFIG_DEFAULT();
    console_cfg.max_cmdline_length = 256;
    console_cfg.max_cmdline_args = 8;
    const esp_err_t init_err = esp_console_init(&console_cfg);
    if (init_err != ESP_OK) {
        return init_err;
    }
    xTaskCreate(console_task, "console", 8192, nullptr, 5, nullptr);
    return ESP_OK;
}
