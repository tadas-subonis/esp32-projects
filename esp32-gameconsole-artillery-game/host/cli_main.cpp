#include "artillery_protocol.h"
#include "link.hpp"
#include "tcp.hpp"
#include "udp.hpp"
#include "wire.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

using artillery::ClientIntent;
using artillery::Link;
using artillery::WireAim;
using artillery::WireError;
using artillery::WireHello;
using artillery::WireIntent;
using artillery::WireMsg;
using artillery::WireState;
using artillery::WireWelcome;

static void usage()
{
    std::fprintf(stderr,
                 "artillery-cli — send one UDP command (always time-bounded)\n"
                 "  artillery-cli [--host 127.0.0.1] [--port %d] [--timeout-ms 2000] hello|start|...\n"
                 "  artillery-cli --emu 17500 [--timeout-ms 2000] status|fire|angle 55|dump PATH|quit\n",
                 ARTILLERY_DEFAULT_PORT);
}

static uint32_t now_ms()
{
    using clock = std::chrono::steady_clock;
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch()).count());
}

static void print_msg(const Link::Message& msg)
{
    switch (static_cast<WireMsg>(msg.type)) {
        case WireMsg::Welcome: {
            WireWelcome w;
            if (artillery::decode_welcome(msg.data, msg.len, &w)) {
                std::printf("{\"type\":\"welcome\",\"ok\":true,\"seat\":%d,\"seq\":%lu,\"token\":\"%s\"}\n",
                            w.seat, static_cast<unsigned long>(w.seq), w.token);
            }
            break;
        }
        case WireMsg::Error: {
            WireError e;
            if (artillery::decode_error(msg.data, msg.len, &e)) {
                std::printf("{\"type\":\"error\",\"ok\":false,\"error\":\"%s\"}\n", e.error);
            }
            break;
        }
        case WireMsg::State: {
            WireState st;
            if (artillery::decode_state(msg.data, msg.len, &st)) {
                std::printf(
                    "{\"type\":\"state\",\"ok\":true,\"seq\":%lu,\"phase\":%u,\"angle\":%d,\"power\":%d,"
                    "\"players\":%u,\"flags\":%u}\n",
                    static_cast<unsigned long>(st.seq), st.phase, st.angle, st.power, st.players, st.flags);
            }
            break;
        }
        case WireMsg::Cmd: {
            std::printf("{\"type\":\"cmd\",\"len\":%u}\n", msg.len);
            break;
        }
        default:
            std::printf("{\"type\":%u,\"id\":%u,\"len\":%u}\n", msg.type, msg.id, msg.len);
            break;
    }
}

static int send_and_print_udp(int fd, Link& link, int max_msgs, int timeout_ms)
{
    int printed = 0;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms < 1 ? 2000 : timeout_ms);
    auto quiet_deadline = deadline;
    bool saw = false;
    while (printed < max_msgs && std::chrono::steady_clock::now() < deadline) {
        if (saw && std::chrono::steady_clock::now() >= quiet_deadline) {
            break;
        }
        uint8_t out[ARTILLERY_UDP_MAX_PACKET];
        const int n = link.compose_out(out, sizeof(out), now_ms(), true);
        if (n > 0) {
            net::udp_send(fd, out, n);
        }
        uint8_t packet[ARTILLERY_UDP_MAX_PACKET];
        const int r = net::udp_recv(fd, packet, sizeof(packet), nullptr);
        if (r > 0) {
            link.pump_in(packet, r);
            Link::Message msg;
            while (link.pop_message(&msg)) {
                print_msg(msg);
                ++printed;
                saw = true;
                quiet_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(80);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    if (printed == 0) {
        std::fprintf(stderr, "cli: timeout waiting for reply (%d ms)\n", timeout_ms);
        return 1;
    }
    return 0;
}

static int send_and_print_tcp(int fd, const char* json, int max_lines, int timeout_ms)
{
    if (net::send_all(fd, json, static_cast<int>(std::strlen(json))) < 0) {
        std::fprintf(stderr, "cli: send failed\n");
        return 1;
    }
    net::LineBuf lines;
    char tmp[2048];
    int printed = 0;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms < 1 ? 2000 : timeout_ms);
    auto quiet_deadline = deadline;
    bool saw_line = false;
    while (printed < max_lines && std::chrono::steady_clock::now() < deadline) {
        if (saw_line && std::chrono::steady_clock::now() >= quiet_deadline) {
            break;
        }
        const int n = net::recv_some(fd, tmp, sizeof(tmp));
        if (n < 0) {
            break;
        }
        if (n == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        if (!lines.feed(tmp, n)) {
            std::fprintf(stderr, "cli: line overflow\n");
            return 1;
        }
        char msg[8192];
        while (lines.pop_line(msg, sizeof(msg))) {
            std::puts(msg);
            ++printed;
            saw_line = true;
            quiet_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(40);
        }
    }
    if (printed == 0) {
        std::fprintf(stderr, "cli: timeout waiting for reply (%d ms)\n", timeout_ms);
        return 1;
    }
    return 0;
}

int main(int argc, char** argv)
{
    const char* host = "127.0.0.1";
    int port = ARTILLERY_DEFAULT_PORT;
    int emu_port = 0;
    int timeout_ms = 2000;
    const char* mode = "bot";
    int seed = 0;
    int value = 0;
    const char* cmd = nullptr;
    const char* path = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            host = argv[++i];
        } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--emu") == 0 && i + 1 < argc) {
            emu_port = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc) {
            timeout_ms = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            mode = argv[++i];
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = std::atoi(argv[++i]);
        } else if (argv[i][0] != '-') {
            if (cmd == nullptr) {
                cmd = argv[i];
            } else if ((std::strcmp(cmd, "dump") == 0 || std::strcmp(cmd, "path") == 0) && path == nullptr) {
                path = argv[i];
            } else if (std::strcmp(cmd, "nudge") == 0 && path == nullptr) {
                path = argv[i];
            } else {
                value = std::atoi(argv[i]);
            }
        } else {
            usage();
            return 2;
        }
    }
    if (cmd == nullptr) {
        usage();
        return 2;
    }
    if (timeout_ms < 100) {
        timeout_ms = 100;
    }
    if (timeout_ms > 30000) {
        timeout_ms = 30000;
    }

    // Emulator control plane stays JSON-over-TCP.
    if (emu_port > 0) {
        const auto connect_deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        int fd = -1;
        while (std::chrono::steady_clock::now() < connect_deadline) {
            fd = net::connect_tcp(host, emu_port);
            if (fd >= 0) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        if (fd < 0) {
            std::fprintf(stderr, "cli: cannot connect to emu control %s:%d within %d ms\n", host, emu_port,
                         timeout_ms);
            return 1;
        }
        char json[320];
        if (std::strcmp(cmd, "status") == 0) {
            std::snprintf(json, sizeof(json), "{\"cmd\":\"status\"}\n");
        } else if (std::strcmp(cmd, "quit") == 0) {
            std::snprintf(json, sizeof(json), "{\"cmd\":\"quit\"}\n");
        } else if (std::strcmp(cmd, "dump") == 0) {
            if (path == nullptr) {
                std::fprintf(stderr, "cli: dump needs a path\n");
                net::close_fd(fd);
                return 2;
            }
            std::snprintf(json, sizeof(json), "{\"cmd\":\"dump\",\"path\":\"%s\"}\n", path);
        } else if (std::strcmp(cmd, "start") == 0) {
            std::snprintf(json, sizeof(json), "{\"cmd\":\"start\",\"mode\":\"%s\"}\n",
                          mode[0] ? mode : "pvp");
        } else if (std::strcmp(cmd, "angle") == 0) {
            std::snprintf(json, sizeof(json), "{\"cmd\":\"angle\",\"value\":%d}\n", value);
        } else if (std::strcmp(cmd, "power") == 0) {
            std::snprintf(json, sizeof(json), "{\"cmd\":\"power\",\"value\":%d}\n", value);
        } else if (std::strcmp(cmd, "nudge") == 0) {
            const char* axis = path ? path : "a";
            const int dir = value == 0 ? 1 : value;
            std::snprintf(json, sizeof(json), "{\"cmd\":\"nudge\",\"axis\":\"%s\",\"dir\":%d}\n", axis, dir);
        } else if (std::strcmp(cmd, "fire") == 0) {
            std::snprintf(json, sizeof(json), "{\"cmd\":\"fire\"}\n");
        } else if (std::strcmp(cmd, "select") == 0) {
            std::snprintf(json, sizeof(json), "{\"cmd\":\"select\"}\n");
        } else {
            std::snprintf(json, sizeof(json), "{\"cmd\":\"%s\"}\n", cmd);
        }
        const int rc = send_and_print_tcp(fd, json, 2, timeout_ms);
        net::close_fd(fd);
        return rc;
    }

    const int fd = net::connect_udp(host, port);
    if (fd < 0) {
        std::fprintf(stderr, "cli: cannot open udp to %s:%d\n", host, port);
        return 1;
    }

    Link link;
    WireHello hello;
    hello.protocol = ARTILLERY_PROTOCOL_VERSION;
    hello.want = artillery::want_to_u8(mode);
    hello.t_ms = now_ms();
    uint8_t hbuf[48];
    const int hn = artillery::encode_hello(hbuf, sizeof(hbuf), hello);
    if (hn > 0) {
        link.send_reliable(static_cast<uint8_t>(WireMsg::Hello), hbuf, static_cast<uint16_t>(hn));
    }

    if (std::strcmp(cmd, "hello") != 0) {
        // Wait for welcome first.
        send_and_print_udp(fd, link, 4, timeout_ms);

        if (std::strcmp(cmd, "start") == 0) {
            ClientIntent in;
            in.kind = ClientIntent::Start;
            in.value = static_cast<int>(artillery::mode_from_name(mode));
            in.seed = static_cast<uint32_t>(seed);
            WireIntent wi{in};
            uint8_t buf[16];
            const int n = artillery::encode_intent(buf, sizeof(buf), wi);
            if (n > 0) {
                link.send_reliable(static_cast<uint8_t>(WireMsg::Intent), buf, static_cast<uint16_t>(n));
            }
        } else if (std::strcmp(cmd, "angle") == 0) {
            WireAim aim{0, static_cast<uint8_t>(value), 55};
            uint8_t buf[8];
            const int n = artillery::encode_aim(buf, sizeof(buf), aim);
            if (n > 0) {
                link.send_unreliable(static_cast<uint8_t>(WireMsg::Aim), buf, static_cast<uint16_t>(n));
            }
        } else if (std::strcmp(cmd, "power") == 0) {
            WireAim aim{0, 45, static_cast<uint8_t>(value)};
            uint8_t buf[8];
            const int n = artillery::encode_aim(buf, sizeof(buf), aim);
            if (n > 0) {
                link.send_unreliable(static_cast<uint8_t>(WireMsg::Aim), buf, static_cast<uint16_t>(n));
            }
        } else if (std::strcmp(cmd, "fire") == 0) {
            WireIntent wi;
            wi.intent.kind = ClientIntent::Fire;
            uint8_t buf[16];
            const int n = artillery::encode_intent(buf, sizeof(buf), wi);
            if (n > 0) {
                link.send_reliable(static_cast<uint8_t>(WireMsg::Intent), buf, static_cast<uint16_t>(n));
            }
        } else if (std::strcmp(cmd, "status") == 0) {
            // Hello already queued a state broadcast; just wait.
        } else {
            WireIntent wi;
            wi.intent.kind = ClientIntent::Fire;
            if (std::strcmp(cmd, "select") == 0) {
                wi.intent.kind = ClientIntent::ToggleSelect;
            } else if (std::strcmp(cmd, "rematch") == 0) {
                wi.intent.kind = ClientIntent::Rematch;
            } else if (std::strcmp(cmd, "title") == 0) {
                wi.intent.kind = ClientIntent::ToTitle;
            }
            uint8_t buf[16];
            const int n = artillery::encode_intent(buf, sizeof(buf), wi);
            if (n > 0) {
                link.send_reliable(static_cast<uint8_t>(WireMsg::Intent), buf, static_cast<uint16_t>(n));
            }
        }
    }

    const int rc = send_and_print_udp(fd, link, 8, timeout_ms);
    net::close_fd(fd);
    return rc;
}
