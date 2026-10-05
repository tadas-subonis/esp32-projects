#include "artillery/input.hpp"
#include "artillery/log.hpp"
#include "artillery/match.hpp"
#include "artillery/net_input.hpp"
#include "artillery/render.hpp"
#include "artillery_protocol.h"
#include "codec.hpp"
#include "proxy.hpp"
#include "tcp.hpp"

#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace artillery;

namespace {

constexpr int kCtrlClients = 4;
constexpr int kDefaultCtrlBase = 17500;

struct CtrlClient {
    int fd = -1;
    net::LineBuf lines;
};

struct CtrlHub {
    int listen_fd = -1;
    int port = 0;
    CtrlClient clients[kCtrlClients]{};
    char pending_dump[260]{};
    int pending_dump_fd = -1;
    bool want_quit = false;
};

void emu_log(const char* line)
{
    std::fprintf(stderr, "[match] %s\n", line);
    std::fflush(stderr);
}

Buttons read_buttons()
{
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    Buttons b;
    b.up = k[SDL_SCANCODE_UP] || k[SDL_SCANCODE_W];
    b.down = k[SDL_SCANCODE_DOWN] || k[SDL_SCANCODE_S];
    b.left = k[SDL_SCANCODE_LEFT] || k[SDL_SCANCODE_A];
    b.right = k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D];
    b.a = k[SDL_SCANCODE_SPACE] || k[SDL_SCANCODE_Z];
    b.b = k[SDL_SCANCODE_X];
    return b;
}

void present(SDL_Renderer* renderer, SDL_Texture* tex, const uint16_t* fb)
{
    SDL_UpdateTexture(tex, nullptr, fb, kWidth * static_cast<int>(sizeof(uint16_t)));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, tex, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}

void write_ppm(const char* path, const uint16_t* fb)
{
    if (path == nullptr || path[0] == 0 || fb == nullptr) {
        return;
    }
    FILE* f = std::fopen(path, "wb");
    if (f == nullptr) {
        std::fprintf(stderr, "emu: cannot write %s\n", path);
        return;
    }
    std::fprintf(f, "P6\n%d %d\n255\n", kWidth, kHeight);
    for (int i = 0; i < kPixelCount; ++i) {
        const uint16_t c = fb[i];
        const unsigned char rgb[3] = {
            static_cast<unsigned char>(((c >> 11) & 0x1F) * 255 / 31),
            static_cast<unsigned char>(((c >> 5) & 0x3F) * 255 / 63),
            static_cast<unsigned char>((c & 0x1F) * 255 / 31),
        };
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    std::fprintf(stderr, "emu: dumped %s\n", path);
}

const char* title_ui_name(TitleUi ui)
{
    switch (ui) {
        case TitleUi::Connecting:
            return "connecting";
        case TitleUi::Waiting:
            return "waiting";
        case TitleUi::Starting:
            return "starting";
        case TitleUi::Rejected:
            return "rejected";
        default:
            return "local";
    }
}

void usage()
{
    std::fprintf(stderr,
                 "artillery-emu — desktop window for the same 480x320 RGB565 compose as firmware\n"
                 "  artillery-emu [--seed N] [--start]\n"
                 "  artillery-emu --remote [--host 127.0.0.1] [--port 7420] [--want pvp]\n"
                 "  --control-port N   localhost JSON control (default remote: %d+seat)\n"
                 "  --dump FILE --dump-after MS\n"
                 "Control: status | dump <path> | quit | angle/power/fire/start/nudge...\n"
                 "Keys: arrows/WASD aim  Space/Z fire  X back  Esc/Q quit\n",
                 kDefaultCtrlBase);
}

bool scene_changed(const ViewModel& a, const ViewModel& b)
{
    if (a.snap.phase != b.snap.phase || a.title_ui != b.title_ui || a.players != b.players ||
        a.title_sel != b.title_sel || a.snap.seed != b.snap.seed) {
        return true;
    }
    return std::memcmp(a.heights.data(), b.heights.data(), sizeof(a.heights)) != 0;
}

bool hud_changed(const ViewModel& a, const ViewModel& b)
{
    return a.snap.angle != b.snap.angle || a.snap.power != b.snap.power || a.snap.wind != b.snap.wind ||
           a.snap.active != b.snap.active || a.snap.hp[0] != b.snap.hp[0] || a.snap.hp[1] != b.snap.hp[1] ||
           a.tank_angle[0] != b.tank_angle[0] || a.tank_angle[1] != b.tank_angle[1];
}

void submit_remote(ServerProxy& proxy, const ButtonEdges& edges, const ViewModel& view, bool remote_pvp,
                   uint32_t dt_ms, AimSendClock* aim_clock)
{
    if (view.snap.phase == Phase::Title) {
        aim_clock->reset();
        if (remote_pvp) {
            if (edges.pressed.a && view.players >= 2) {
                ClientIntent start;
                start.kind = ClientIntent::Start;
                start.value = static_cast<int>(Mode::Pvp);
                proxy.submit(start);
            }
            return;
        }
        if (edges.pressed.left || edges.pressed.right || edges.pressed.up || edges.pressed.down ||
            edges.pressed.b) {
            proxy.submit(ClientIntent{ClientIntent::ToggleSelect});
        }
        if (edges.pressed.a) {
            ClientIntent start;
            start.kind = ClientIntent::Start;
            start.value = view.title_sel == 0 ? static_cast<int>(Mode::VsBot)
                                              : static_cast<int>(Mode::Hotseat);
            proxy.submit(start);
        }
        return;
    }
    if (view.snap.phase == Phase::GameOver) {
        aim_clock->reset();
        if (edges.pressed.a) {
            proxy.submit(ClientIntent{ClientIntent::Rematch});
        }
        if (edges.pressed.b) {
            proxy.submit(ClientIntent{ClientIntent::ToTitle});
        }
        return;
    }
    if (view.snap.phase != Phase::Aiming) {
        aim_clock->reset();
        return;
    }
    if (remote_pvp) {
        const int seat = proxy.seat();
        if (seat < 0 || static_cast<int>(view.snap.active) != seat) {
            aim_clock->reset();
            return;
        }
    }
    const bool holding =
        edges.down.left || edges.down.right || edges.down.up || edges.down.down;
    const bool edge =
        edges.pressed.left || edges.pressed.right || edges.pressed.up || edges.pressed.down;
    const int step = aim_clock->poll(holding, edge, dt_ms);
    if (step > 0) {
        const int dir_left = view.snap.active == PlayerId::P0 ? step : -step;
        if (edges.down.left) {
            proxy.submit(ClientIntent{ClientIntent::NudgeAngle, dir_left});
        }
        if (edges.down.right) {
            proxy.submit(ClientIntent{ClientIntent::NudgeAngle, -dir_left});
        }
        if (edges.down.up) {
            proxy.submit(ClientIntent{ClientIntent::NudgePower, step});
        }
        if (edges.down.down) {
            proxy.submit(ClientIntent{ClientIntent::NudgePower, -step});
        }
    }
    if (edges.pressed.a) {
        proxy.submit(ClientIntent{ClientIntent::Fire});
    }
}

bool open_control(CtrlHub* hub, int port)
{
    if (hub == nullptr || port <= 0) {
        return false;
    }
    hub->listen_fd = net::listen_tcp(port);
    if (hub->listen_fd < 0) {
        std::fprintf(stderr, "emu: control listen failed on %d\n", port);
        return false;
    }
    hub->port = port;
    std::fprintf(stderr, "emu: control on 127.0.0.1:%d\n", port);
    return true;
}

void close_control(CtrlHub* hub)
{
    if (hub == nullptr) {
        return;
    }
    for (int i = 0; i < kCtrlClients; ++i) {
        net::close_fd(hub->clients[i].fd);
        hub->clients[i] = CtrlClient{};
    }
    net::close_fd(hub->listen_fd);
    hub->listen_fd = -1;
}

void write_status_line(char* buf, size_t cap, const ViewModel& view, int seat)
{
    const bool ended = view.snap.phase == Phase::GameOver || view.snap.have_winner;
    const int winner = ended ? static_cast<int>(view.snap.winner) : -1;
    std::snprintf(buf, cap,
                  "{\"ok\":true,\"cmd\":\"status\",\"seat\":%d,\"phase\":\"%s\",\"mode\":\"%s\","
                  "\"title_ui\":\"%s\",\"players\":%d,\"angle\":%d,\"power\":%d,\"wind\":%d,"
                  "\"active\":%d,\"hp0\":%d,\"hp1\":%d,\"turn\":%d,\"winner\":%d,"
                  "\"firing\":%s,\"proj_x\":%.1f,\"proj_y\":%.1f,\"seq\":%lu}\n",
                  seat, phase_name(view.snap.phase), mode_name(view.snap.mode),
                  title_ui_name(view.title_ui), view.players, view.snap.angle, view.snap.power,
                  view.snap.wind, static_cast<int>(view.snap.active), view.snap.hp[0], view.snap.hp[1],
                  view.snap.turn, winner, view.snap.firing ? "true" : "false", view.snap.proj_x,
                  view.snap.proj_y, static_cast<unsigned long>(view.snap.seq));
}

bool extract_path(const char* line, char* out, size_t cap)
{
    if (out == nullptr || cap == 0) {
        return false;
    }
    out[0] = 0;
    const char* key = std::strstr(line, "\"path\"");
    if (key == nullptr) {
        return false;
    }
    const char* colon = std::strchr(key, ':');
    if (colon == nullptr) {
        return false;
    }
    const char* q0 = std::strchr(colon, '"');
    if (q0 == nullptr) {
        return false;
    }
    ++q0;
    const char* q1 = std::strchr(q0, '"');
    if (q1 == nullptr || static_cast<size_t>(q1 - q0) >= cap) {
        return false;
    }
    std::memcpy(out, q0, static_cast<size_t>(q1 - q0));
    out[q1 - q0] = 0;
    return out[0] != 0;
}

void handle_control_line(CtrlHub* hub, int client_fd, const char* line, ServerProxy* proxy, Match* local,
                         bool remote, const ViewModel& view, int seat)
{
    if (std::strncmp(line, "{\"cmd\":\"quit\"", 13) == 0 || std::strcmp(line, "quit") == 0) {
        hub->want_quit = true;
        net::send_all(client_fd, "{\"ok\":true,\"cmd\":\"quit\"}\n", 25);
        return;
    }
    if (std::strstr(line, "\"cmd\":\"dump\"") != nullptr || std::strncmp(line, "dump ", 5) == 0) {
        char path[260] = {};
        if (!extract_path(line, path, sizeof(path))) {
            if (std::strncmp(line, "dump ", 5) == 0) {
                std::snprintf(path, sizeof(path), "%s", line + 5);
            }
        }
        if (path[0] == 0) {
            net::send_all(client_fd, "{\"ok\":false,\"error\":\"need_path\"}\n", 33);
            return;
        }
        if (hub->pending_dump[0] && hub->pending_dump_fd >= 0) {
            net::send_all(client_fd, "{\"ok\":false,\"error\":\"dump_busy\"}\n", 33);
            return;
        }
        std::snprintf(hub->pending_dump, sizeof(hub->pending_dump), "%s", path);
        hub->pending_dump_fd = client_fd;
        // Reply after the next frame write so the file exists before CLI returns.
        return;
    }
    if (std::strstr(line, "\"cmd\":\"status\"") != nullptr || std::strcmp(line, "status") == 0) {
        char reply[384];
        write_status_line(reply, sizeof(reply), view, seat);
        net::send_all(client_fd, reply, static_cast<int>(std::strlen(reply)));
        return;
    }

    NetRequest req;
    char json[256];
    if (line[0] == '{') {
        std::snprintf(json, sizeof(json), "%s", line);
    } else {
        // Plain: fire | angle 55 | power 80 | start | nudge a 1
        char cmd[32] = {};
        int value = 0;
        char axis[8] = {};
        int dir = 1;
        if (std::sscanf(line, "%31s %d", cmd, &value) >= 1) {
            if (std::strcmp(cmd, "nudge") == 0) {
                std::sscanf(line, "%*s %7s %d", axis, &dir);
                std::snprintf(json, sizeof(json), "{\"cmd\":\"nudge\",\"axis\":\"%s\",\"dir\":%d}",
                              axis[0] ? axis : "a", dir);
            } else if (std::strcmp(cmd, "start") == 0) {
                std::snprintf(json, sizeof(json), "{\"cmd\":\"start\",\"mode\":\"pvp\"}");
            } else if (value != 0 || std::strcmp(cmd, "angle") == 0 || std::strcmp(cmd, "power") == 0) {
                std::snprintf(json, sizeof(json), "{\"cmd\":\"%s\",\"value\":%d}", cmd, value);
            } else {
                std::snprintf(json, sizeof(json), "{\"cmd\":\"%s\"}", cmd);
            }
        } else {
            net::send_all(client_fd, "{\"ok\":false,\"error\":\"bad_cmd\"}\n", 31);
            return;
        }
    }
    if (!parse_net_request(json, &req) || req.kind != NetRequest::Intent) {
        net::send_all(client_fd, "{\"ok\":false,\"error\":\"bad_cmd\"}\n", 31);
        return;
    }
    bool ok = false;
    const char* err = "";
    if (remote && proxy != nullptr) {
        const ApplyResult r = proxy->submit_wait(req.intent, 150);
        ok = r.accepted;
        err = r.error;
    } else if (local != nullptr) {
        const PlayerId who =
            req.intent.kind == ClientIntent::Start || req.intent.kind == ClientIntent::ToggleSelect ||
                    req.intent.kind == ClientIntent::Rematch || req.intent.kind == ClientIntent::ToTitle
                ? PlayerId::P0
                : local->active();
        const ApplyResult r = local->apply(who, req.intent);
        ok = r.accepted;
        err = r.error;
    }
    char reply[160];
    if (ok) {
        std::snprintf(reply, sizeof(reply), "{\"ok\":true,\"cmd\":\"%s\"}\n", req.cmd);
    } else {
        std::snprintf(reply, sizeof(reply), "{\"ok\":false,\"cmd\":\"%s\",\"error\":\"%s\"}\n", req.cmd,
                      err && err[0] ? err : "rejected");
    }
    net::send_all(client_fd, reply, static_cast<int>(std::strlen(reply)));
}

void poll_control(CtrlHub* hub, ServerProxy* proxy, Match* local, bool remote, const ViewModel& view,
                  int seat)
{
    if (hub == nullptr || hub->listen_fd < 0) {
        return;
    }
    for (;;) {
        const int fd = net::accept_tcp(hub->listen_fd);
        if (fd < 0) {
            break;
        }
        int slot = -1;
        for (int i = 0; i < kCtrlClients; ++i) {
            if (hub->clients[i].fd < 0) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            net::close_fd(fd);
        } else {
            hub->clients[slot].fd = fd;
            hub->clients[slot].lines = net::LineBuf{};
        }
    }

    char tmp[1024];
    char line[512];
    for (int i = 0; i < kCtrlClients; ++i) {
        CtrlClient& c = hub->clients[i];
        if (c.fd < 0) {
            continue;
        }
        const int n = net::recv_some(c.fd, tmp, sizeof(tmp));
        if (n < 0) {
            net::close_fd(c.fd);
            c = CtrlClient{};
            continue;
        }
        if (n == 0) {
            continue;
        }
        if (!c.lines.feed(tmp, n)) {
            net::close_fd(c.fd);
            c = CtrlClient{};
            continue;
        }
        while (c.lines.pop_line(line, sizeof(line))) {
            handle_control_line(hub, c.fd, line, proxy, local, remote, view, seat);
        }
    }
}

}  // namespace

int main(int argc, char** argv)
{
    uint32_t seed = 1;
    bool start = false;
    bool remote = false;
    bool want_control = false;
    int control_port = 0;
    const char* host = "127.0.0.1";
    int port = ARTILLERY_DEFAULT_PORT;
    const char* want = "bot";
    const char* dump_path = nullptr;
    uint32_t dump_after_ms = 0;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--start") == 0) {
            start = true;
        } else if (std::strcmp(argv[i], "--remote") == 0) {
            remote = true;
            want = "pvp";
            want_control = true;
        } else if (std::strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            host = argv[++i];
            remote = true;
            want_control = true;
        } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = std::atoi(argv[++i]);
            remote = true;
            want_control = true;
        } else if (std::strcmp(argv[i], "--want") == 0 && i + 1 < argc) {
            want = argv[++i];
        } else if (std::strcmp(argv[i], "--control-port") == 0 && i + 1 < argc) {
            control_port = std::atoi(argv[++i]);
            want_control = true;
        } else if (std::strcmp(argv[i], "--control") == 0) {
            want_control = true;
        } else if (std::strcmp(argv[i], "--dump") == 0 && i + 1 < argc) {
            dump_path = argv[++i];
        } else if (std::strcmp(argv[i], "--dump-after") == 0 && i + 1 < argc) {
            dump_after_ms = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        } else {
            usage();
            return 2;
        }
    }
    if (seed == 0) {
        seed = 1;
    }

    ServerProxy proxy;
    Match local;
    const bool remote_pvp = remote && std::strcmp(want, "pvp") == 0;
    int seat = 0;
    if (remote) {
        if (!proxy.open_remote(host, port, want)) {
            std::fprintf(stderr, "emu: cannot connect to %s:%d — start artillery-server\n", host, port);
            return 1;
        }
        seat = proxy.seat() < 0 ? 0 : proxy.seat();
        std::fprintf(stderr, "emu: remote %s:%d want=%s seat=%d\n", host, port, want, seat);
    } else {
        local.set_seed(seed);
        if (start) {
            local.start(seed, Mode::VsBot);
        } else {
            local.reset_title();
        }
    }

    CtrlHub ctrl{};
    if (want_control) {
        if (control_port <= 0) {
            control_port = kDefaultCtrlBase + seat;
        }
        if (!open_control(&ctrl, control_port)) {
            return 1;
        }
    }

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "emu: SDL_Init failed: %s\n", SDL_GetError());
        close_control(&ctrl);
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    char title[64];
    if (remote) {
        std::snprintf(title, sizeof(title), "Tank Duel emu P%d", seat);
    } else {
        std::snprintf(title, sizeof(title), "Tank Duel (emu)");
    }
    const int win_w = kWidth * 2;
    const int win_h = kHeight * 2;
    int win_x = SDL_WINDOWPOS_CENTERED;
    int win_y = SDL_WINDOWPOS_CENTERED;
    if (remote && seat == 1) {
        win_x = 40 + win_w + 24;
        win_y = 40;
    } else if (remote) {
        win_x = 40;
        win_y = 40;
    }

    SDL_Window* window = SDL_CreateWindow(title, win_x, win_y, win_w, win_h, SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        std::fprintf(stderr, "emu: SDL_CreateWindow failed: %s\n", SDL_GetError());
        close_control(&ctrl);
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == nullptr) {
        std::fprintf(stderr, "emu: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        close_control(&ctrl);
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(renderer, kWidth, kHeight);
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);

    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, kWidth,
                                         kHeight);
    if (tex == nullptr) {
        std::fprintf(stderr, "emu: SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        close_control(&ctrl);
        SDL_Quit();
        return 1;
    }

    static uint16_t frame[kPixelCount];
    static uint16_t scene[kPixelCount];
    set_log_sink(&emu_log);

    Buttons prev{};
    DebugOverlay debug{};
    DirtyList dirty{};
    Rect prev_proj{};
    ViewModel prev_view{};
    bool have_prev = false;
    uint32_t start_hold_ms = 0;
    AimSendClock aim_send{};
    uint32_t elapsed_ms = 0;
    bool dumped = false;
    bool running = true;
    Uint32 last = SDL_GetTicks();

    usage();

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
            }
            if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
                if (ev.key.keysym.sym == SDLK_ESCAPE || ev.key.keysym.sym == SDLK_q) {
                    running = false;
                }
            }
        }

        const Uint32 now = SDL_GetTicks();
        uint32_t dt_ms = now - last;
        if (dt_ms < 16) {
            SDL_Delay(16 - dt_ms);
            continue;
        }
        if (dt_ms > 100) {
            dt_ms = 100;
        }
        last = now;
        elapsed_ms += dt_ms;

        const bool focused = (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
        Buttons now_btns{};
        if (focused) {
            now_btns = read_buttons();
        }
        const ButtonEdges edges = make_edges(prev, now_btns);
        debug.on_frame(dt_ms, now_btns, edges.pressed);
        prev = now_btns;

        ViewModel view;
        bool scene_dirty = true;
        bool hud_dirty = true;
        if (remote) {
            proxy.pump(dt_ms);
            view = proxy.view();
            stamp_title_ui(view, remote_pvp, proxy.alive());
            if (view.title_ui == TitleUi::Starting && proxy.alive()) {
                start_hold_ms += dt_ms;
                if (start_hold_ms >= 700) {
                    start_hold_ms = 0;
                    ClientIntent start_intent;
                    start_intent.kind = ClientIntent::Start;
                    start_intent.value = static_cast<int>(Mode::Pvp);
                    proxy.submit(start_intent);
                }
            } else {
                start_hold_ms = 0;
            }
            if (focused) {
                submit_remote(proxy, edges, view, remote_pvp, dt_ms, &aim_send);
            }
            if (!proxy.alive()) {
                std::fprintf(stderr, "emu: disconnected\n");
                running = false;
            }
            scene_dirty = !have_prev || scene_changed(prev_view, view);
            hud_dirty = scene_dirty || hud_changed(prev_view, view);
            prev_view = view;
            have_prev = true;
            present_frame(frame, scene, view, scene_dirty, hud_dirty, dirty, prev_proj, debug);
        } else {
            local.handle_buttons(edges);
            local.tick(dt_ms);
            present_frame(frame, scene, local, dirty, prev_proj, debug);
            view = local.view();
            stamp_title_ui(view, false, false);
        }

        poll_control(&ctrl, remote ? &proxy : nullptr, remote ? nullptr : &local, remote, view, seat);
        if (ctrl.want_quit) {
            running = false;
        }
        if (ctrl.pending_dump[0]) {
            write_ppm(ctrl.pending_dump, frame);
            if (ctrl.pending_dump_fd >= 0) {
                char reply[320];
                std::snprintf(reply, sizeof(reply), "{\"ok\":true,\"cmd\":\"dump\",\"path\":\"%s\"}\n",
                              ctrl.pending_dump);
                net::send_all(ctrl.pending_dump_fd, reply, static_cast<int>(std::strlen(reply)));
                ctrl.pending_dump_fd = -1;
            }
            ctrl.pending_dump[0] = 0;
        }

        present(renderer, tex, frame);
        if (dump_path && !dumped && elapsed_ms >= dump_after_ms) {
            write_ppm(dump_path, frame);
            dumped = true;
        }
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close_control(&ctrl);
    if (remote) {
        proxy.close();
    }
    return 0;
}
