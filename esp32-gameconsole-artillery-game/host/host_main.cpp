#include "artillery/input.hpp"
#include "artillery/render.hpp"
#include "artillery_protocol.h"
#include "proxy.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

using namespace artillery;

static uint16_t g_frame[kPixelCount];

#ifdef _WIN32
static void enable_vt()
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}

static int read_key()
{
    if (!_kbhit()) {
        return 0;
    }
    const int ch = _getch();
    if (ch == 0 || ch == 224) {
        if (!_kbhit()) {
            return 0;
        }
        const int x = _getch();
        if (x == 72) {
            return 1000;
        }
        if (x == 80) {
            return 1001;
        }
        if (x == 77) {
            return 1002;
        }
        if (x == 75) {
            return 1003;
        }
        return 0;
    }
    return ch;
}
#else
static termios g_old_term{};
static bool g_raw = false;

static void restore_term()
{
    if (g_raw) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_old_term);
        std::fputs("\x1b[0m\x1b[?25h\x1b[2J\x1b[H", stdout);
        std::fflush(stdout);
        g_raw = false;
    }
}

static void set_raw()
{
    tcgetattr(STDIN_FILENO, &g_old_term);
    g_raw = true;
    std::atexit(restore_term);
    termios t = g_old_term;
    t.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    std::fputs("\x1b[?25l\x1b[2J", stdout);
}

static int read_key()
{
    unsigned char ch = 0;
    const ssize_t n = ::read(STDIN_FILENO, &ch, 1);
    if (n <= 0) {
        return 0;
    }
    if (ch == 0x1b) {
        unsigned char seq[2] = {0, 0};
        if (::read(STDIN_FILENO, &seq[0], 1) == 1 && ::read(STDIN_FILENO, &seq[1], 1) == 1 && seq[0] == '[') {
            if (seq[1] == 'A') {
                return 1000;
            }
            if (seq[1] == 'B') {
                return 1001;
            }
            if (seq[1] == 'C') {
                return 1002;
            }
            if (seq[1] == 'D') {
                return 1003;
            }
        }
        return 0;
    }
    return static_cast<int>(ch);
}
#endif

static void apply_key(Buttons& b, int key)
{
    switch (key) {
        case 'w':
        case 'W':
        case 1000:
            b.up = true;
            break;
        case 's':
        case 'S':
        case 1001:
            b.down = true;
            break;
        case 'd':
        case 'D':
        case 1002:
            b.right = true;
            break;
        case 'a':
        case 'A':
        case 1003:
            b.left = true;
            break;
        case ' ':
        case 'z':
        case 'Z':
            b.a = true;
            break;
        case 'x':
        case 'X':
            b.b = true;
            break;
        default:
            break;
    }
}

static void rgb565_rgb(uint16_t c, int& r, int& g, int& b)
{
    r = ((c >> 11) & 0x1F) * 255 / 31;
    g = ((c >> 5) & 0x3F) * 255 / 63;
    b = (c & 0x1F) * 255 / 31;
}

static void sample(const uint16_t* fb, int sx, int sy, int scale, int& r, int& g, int& b)
{
    int rs = 0;
    int gs = 0;
    int bs = 0;
    int n = 0;
    for (int dy = 0; dy < scale; ++dy) {
        for (int dx = 0; dx < scale; ++dx) {
            int cr, cg, cb;
            rgb565_rgb(fb[(sy + dy) * kWidth + (sx + dx)], cr, cg, cb);
            rs += cr;
            gs += cg;
            bs += cb;
            ++n;
        }
    }
    r = rs / n;
    g = gs / n;
    b = bs / n;
}

static void present_terminal(const uint16_t* fb, bool local)
{
    constexpr int kScale = 4;
    constexpr int kCols = kWidth / kScale;
    constexpr int kRows = kHeight / kScale;
    std::fputs("\x1b[H", stdout);
    for (int cy = 0; cy < kRows; cy += 2) {
        for (int cx = 0; cx < kCols; ++cx) {
            int r1, g1, b1, r2, g2, b2;
            sample(fb, cx * kScale, cy * kScale, kScale, r1, g1, b1);
            if (cy + 1 < kRows) {
                sample(fb, cx * kScale, (cy + 1) * kScale, kScale, r2, g2, b2);
            } else {
                r2 = g2 = b2 = 0;
            }
            std::printf("\x1b[38;2;%d;%d;%dm\x1b[48;2;%d;%d;%dm▀", r1, g1, b1, r2, g2, b2);
        }
        std::fputs("\x1b[0m\n", stdout);
    }
    std::fputs(local ? "LOCAL bot — arrows/WASD aim  Space fire  X back  Q quit  (in-process server)\n"
                     : "REMOTE — arrows/WASD aim  Space fire  X back  Q quit  (TCP server)\n",
                stdout);
    std::fflush(stdout);
}

int main(int argc, char** argv)
{
    const char* host = "127.0.0.1";
    int port = ARTILLERY_DEFAULT_PORT;
    const char* want = "bot";
    bool remote = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            host = argv[++i];
            remote = true;
        } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = std::atoi(argv[++i]);
            remote = true;
        } else if (std::strcmp(argv[i], "--want") == 0 && i + 1 < argc) {
            want = argv[++i];
        } else if (std::strcmp(argv[i], "--remote") == 0) {
            remote = true;
        } else if (std::strcmp(argv[i], "--local") == 0) {
            remote = false;
        }
    }

#ifdef _WIN32
    enable_vt();
#else
    set_raw();
#endif

    ServerProxy proxy;
    if (remote) {
        if (!proxy.open_remote(host, port, want)) {
            std::fprintf(stderr, "client: cannot connect to %s:%d — start the server with: make server\n",
                         host, port);
            return 1;
        }
    } else if (!proxy.open_local(want)) {
        std::fprintf(stderr, "client: local server join failed (%s)\n", proxy.last().error);
        return 1;
    }

    Buttons prev{};
    bool running = true;

    while (running && proxy.alive()) {
        Buttons now{};
        for (;;) {
            const int key = read_key();
            if (key == 0) {
                break;
            }
            if (key == 'q' || key == 'Q') {
                running = false;
                break;
            }
            apply_key(now, key);
        }
        const auto edges = make_edges(prev, now);
        prev = now;
        const ViewModel& view = proxy.view();
        const bool remote_pvp = std::strcmp(want, "pvp") == 0;
        if (edges.pressed.left || edges.pressed.right || edges.pressed.up || edges.pressed.down ||
            edges.pressed.b) {
            if (view.snap.phase == Phase::Title && !remote_pvp) {
                proxy.submit(ClientIntent{ClientIntent::ToggleSelect});
            } else if (view.snap.phase == Phase::GameOver && edges.pressed.b) {
                proxy.submit(ClientIntent{ClientIntent::ToTitle});
            }
        }
        if (edges.down.left && view.snap.phase == Phase::Aiming) {
            proxy.submit(ClientIntent{ClientIntent::NudgeAngle,
                                      view.snap.active == PlayerId::P0 ? 1 : -1});
        }
        if (edges.down.right && view.snap.phase == Phase::Aiming) {
            proxy.submit(ClientIntent{ClientIntent::NudgeAngle,
                                      view.snap.active == PlayerId::P0 ? -1 : 1});
        }
        if (edges.down.up && view.snap.phase == Phase::Aiming) {
            proxy.submit(ClientIntent{ClientIntent::NudgePower, 1});
        }
        if (edges.down.down && view.snap.phase == Phase::Aiming) {
            proxy.submit(ClientIntent{ClientIntent::NudgePower, -1});
        }
        if (edges.pressed.a) {
            if (view.snap.phase == Phase::Title) {
                ClientIntent start;
                start.kind = ClientIntent::Start;
                if (std::strcmp(want, "pvp") == 0) {
                    start.value = static_cast<int>(Mode::Pvp);
                } else if (view.title_sel != 0 || std::strcmp(want, "hotseat") == 0) {
                    start.value = static_cast<int>(Mode::Hotseat);
                } else {
                    start.value = static_cast<int>(Mode::VsBot);
                }
                proxy.submit(start);
            } else if (view.snap.phase == Phase::GameOver) {
                proxy.submit(ClientIntent{ClientIntent::Rematch});
            } else {
                proxy.submit(ClientIntent{ClientIntent::Fire});
            }
        }

        proxy.pump(16);
        proxy.pump(16);

        ViewModel drawn = proxy.view();
        stamp_title_ui(drawn, remote_pvp, proxy.alive());
        compose_scene(g_frame, drawn);
        draw_projectile(g_frame, drawn);
        present_terminal(g_frame, proxy.is_local());
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
    proxy.close();
    return 0;
}
