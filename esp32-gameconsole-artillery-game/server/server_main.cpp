#include "authority.hpp"
#include "artillery_protocol.h"
#include "event_trace.hpp"
#include "link.hpp"
#include "tcp.hpp"
#include "udp.hpp"
#include "wire.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using artillery::ApplyResult;
using artillery::Authority;
using artillery::ClientIntent;
using artillery::ClientSeat;
using artillery::Link;
using artillery::LoggedCommand;
using artillery::Phase;
using artillery::PlayerId;
using artillery::ViewModel;
using artillery::WireAim;
using artillery::WireCmd;
using artillery::WireError;
using artillery::WireHello;
using artillery::WireIntent;
using artillery::WireMsg;
using artillery::WireState;
using artillery::WireSync;
using artillery::WireWelcome;

namespace {

constexpr int kMaxClients = 4;
const char* TAG = "server";

struct Client {
    net::UdpAddr addr{};
    ClientSeat seat;
    Link link;
    uint32_t last_ms = 0;
    uint32_t eid = 0;
    bool bound = false;
};

uint32_t now_ms()
{
    return artillery_trace_t_ms();
}

bool send_raw(int fd, const net::UdpAddr& addr, const uint8_t* data, int len)
{
    return net::udp_send_to(fd, data, len, addr) == len;
}

void flush_link(int fd, Client& c, uint32_t t, bool force)
{
    uint8_t buf[ARTILLERY_UDP_MAX_PACKET];
    // Keep composing while reliable messages remain (baseline may not fit with welcome).
    for (int i = 0; i < 8; ++i) {
        if (!force && c.link.pending_reliable() <= 0) {
            break;
        }
        const int n = c.link.compose_out(buf, sizeof(buf), t, true);
        if (n <= 0) {
            break;
        }
        send_raw(fd, c.addr, buf, n);
        force = false;
        if (c.link.pending_reliable() <= 0) {
            break;
        }
    }
}

bool queue_msg(Client& c, WireMsg type, const uint8_t* payload, int len)
{
    return c.link.send_reliable(static_cast<uint8_t>(type), payload, static_cast<uint16_t>(len));
}

void send_error_msg(Client& c, uint8_t cmd, const char* err, uint32_t eid)
{
    WireError e;
    e.cmd = cmd;
    std::snprintf(e.error, sizeof(e.error), "%s", err ? err : "error");
    e.eid = eid;
    e.t_ms = now_ms();
    uint8_t buf[64];
    const int n = artillery::encode_error(buf, sizeof(buf), e);
    if (n > 0) {
        queue_msg(c, WireMsg::Error, buf, n);
    }
}

void send_state_to(Client& c, Authority& auth, bool full_heights, const WireState* delta_src = nullptr)
{
    WireState st = delta_src ? *delta_src : artillery::state_from_view(auth.view(), full_heights);
    st.seq = auth.seq();
    uint8_t buf[ARTILLERY_UDP_MAX_PACKET];
    const int n = artillery::encode_state(buf, sizeof(buf), st);
    if (n > 0) {
        queue_msg(c, WireMsg::State, buf, n);
    }
}

void broadcast_state(Client* clients, Authority& auth, bool full_heights, const WireState* with_deltas)
{
    for (int i = 0; i < kMaxClients; ++i) {
        if (clients[i].bound && clients[i].seat.joined) {
            send_state_to(clients[i], auth, full_heights, with_deltas);
        }
    }
    auth.clear_scene_dirty();
    auth.clear_keyframe();
}

void broadcast_cmd(Client* clients, uint32_t seq, PlayerId who, const ClientIntent& intent)
{
    WireCmd cmd;
    cmd.seq = seq;
    cmd.who = who;
    cmd.intent = intent;
    uint8_t buf[32];
    const int n = artillery::encode_cmd(buf, sizeof(buf), cmd);
    if (n <= 0) {
        return;
    }
    for (int i = 0; i < kMaxClients; ++i) {
        if (clients[i].bound && clients[i].seat.joined) {
            queue_msg(clients[i], WireMsg::Cmd, buf, n);
        }
    }
}

void broadcast_aim(Client* clients, PlayerId who, int angle, int power)
{
    WireAim aim;
    aim.who = static_cast<uint8_t>(who);
    aim.angle = static_cast<uint8_t>(angle);
    aim.power = static_cast<uint8_t>(power);
    uint8_t buf[8];
    const int n = artillery::encode_aim(buf, sizeof(buf), aim);
    if (n <= 0) {
        return;
    }
    for (int i = 0; i < kMaxClients; ++i) {
        if (clients[i].bound && clients[i].seat.joined) {
            clients[i].link.send_unreliable(static_cast<uint8_t>(WireMsg::Aim), buf,
                                             static_cast<uint16_t>(n));
        }
    }
}

int find_by_addr(Client* clients, const net::UdpAddr& addr)
{
    for (int i = 0; i < kMaxClients; ++i) {
        if (clients[i].bound && net::udp_addr_equal(clients[i].addr, addr)) {
            return i;
        }
    }
    return -1;
}

int next_slot(Client* clients)
{
    for (int i = 0; i < kMaxClients; ++i) {
        if (!clients[i].bound) {
            return i;
        }
    }
    return -1;
}

void drop_client(Client* c, Client* all, Authority* auth, const char* why)
{
    ART_NET_LOGI(TAG, c->eid, "disconnect why=%s joined=%d", why, (int)c->seat.joined);
    if (auth) {
        auth->disconnect(&c->seat);
    }
    *c = Client{};
    if (auth && all) {
        broadcast_state(all, *auth, false, nullptr);
    }
}

void kick_token(Client* all, Client* keep, Authority& auth, const char* token)
{
    if (token == nullptr || token[0] == 0) {
        return;
    }
    for (int i = 0; i < kMaxClients; ++i) {
        Client& o = all[i];
        if (&o == keep || !o.bound || !o.seat.joined) {
            continue;
        }
        if (std::strcmp(o.seat.token, token) == 0) {
            drop_client(&o, all, &auth, "replaced");
        }
    }
}

void handle_hello(Client* c, const WireHello& hello, Authority& auth, Client* all)
{
    if (hello.eid != 0) {
        c->eid = hello.eid;
    } else if (c->eid == 0) {
        c->eid = artillery::trace::next_eid();
    }
    char want[16];
    artillery::want_from_u8(hello.want, want, sizeof(want));
    ART_NET_LOGI(TAG, c->eid, "hello_rx want=%s token=%s protocol=%d", want,
                 hello.token[0] ? hello.token : "-", hello.protocol);

    if (hello.token[0]) {
        std::snprintf(c->seat.token, sizeof(c->seat.token), "%s", hello.token);
        kick_token(all, c, auth, hello.token);
    }
    const ApplyResult joined = auth.join(&c->seat, hello.protocol, want);
    if (!joined.accepted) {
        ART_NET_LOGW(TAG, c->eid, "hello_reject err=%s", joined.error);
        send_error_msg(*c, static_cast<uint8_t>(WireMsg::Hello), joined.error, c->eid);
        return;
    }

    WireWelcome w;
    w.seat = static_cast<int8_t>(c->seat.both ? -1 : c->seat.seat);
    w.want = hello.want;
    w.seq = auth.seq();
    std::snprintf(w.token, sizeof(w.token), "%s", c->seat.token);
    w.eid = c->eid;
    w.t_ms = now_ms();
    uint8_t buf[48];
    const int n = artillery::encode_welcome(buf, sizeof(buf), w);
    if (n > 0) {
        queue_msg(*c, WireMsg::Welcome, buf, n);
    }
    ART_NET_LOGI(TAG, c->eid, "hello_ok seat=%d both=%d want=%s token=%s seq=%lu", c->seat.seat,
                 (int)c->seat.both, want, c->seat.token, static_cast<unsigned long>(auth.seq()));
    broadcast_state(all, auth, true, nullptr);
}

void handle_sync(Client* c, const WireSync& sync, Authority& auth)
{
    LoggedCommand cmds[Authority::kLogCap];
    const int n = auth.commands_after(sync.seq, cmds, Authority::kLogCap);
    ART_NET_LOGI(TAG, c->eid, "sync from_seq=%lu n=%d", static_cast<unsigned long>(sync.seq), n);
    if (n < 0) {
        send_state_to(*c, auth, true, nullptr);
        return;
    }
    for (int i = 0; i < n; ++i) {
        WireCmd cmd;
        cmd.seq = cmds[i].seq;
        cmd.who = cmds[i].who;
        cmd.intent = cmds[i].intent;
        uint8_t buf[32];
        const int m = artillery::encode_cmd(buf, sizeof(buf), cmd);
        if (m > 0) {
            queue_msg(*c, WireMsg::Cmd, buf, m);
        }
    }
}

void handle_intent(Client* c, const WireIntent& wi, Authority& auth, Client* all)
{
    const ApplyResult result = auth.submit(c->seat, wi.intent);
    ART_NET_LOGI(TAG, c->eid, "cmd=%d seat=%d who=%d ok=%d err=%s seq=%lu",
                 static_cast<int>(wi.intent.kind), c->seat.seat, static_cast<int>(auth.actor(c->seat)),
                 (int)result.accepted, result.error, static_cast<unsigned long>(auth.seq()));
    if (!result.accepted) {
        send_error_msg(*c, static_cast<uint8_t>(WireMsg::Intent), result.error, c->eid);
        return;
    }
    broadcast_cmd(all, auth.seq(), auth.actor(c->seat), wi.intent);
    if (auth.keyframe() || auth.scene_dirty()) {
        const bool full = wi.intent.kind == ClientIntent::Start || wi.intent.kind == ClientIntent::Rematch ||
                          wi.intent.kind == ClientIntent::ToTitle;
        broadcast_state(all, auth, full, nullptr);
    }
}

void handle_aim(Client* c, const WireAim& aim, Authority& auth, Client* all)
{
    const ApplyResult result = auth.set_aim(c->seat, aim.angle, aim.power);
    if (!result.accepted) {
        return;
    }
    broadcast_aim(all, auth.actor(c->seat), aim.angle, aim.power);
}

void expire_stale(Client* clients, Authority& auth, uint32_t t)
{
    for (int i = 0; i < kMaxClients; ++i) {
        Client& c = clients[i];
        if (!c.bound) {
            continue;
        }
        if (t - c.last_ms > Link::kPeerTimeoutMs) {
            drop_client(&c, clients, &auth, "timeout");
        }
    }
}

void process_messages(int /*fd*/, Client& c, Authority& auth, Client* all)
{
    Link::Message msg;
    while (c.link.pop_message(&msg)) {
        switch (static_cast<WireMsg>(msg.type)) {
            case WireMsg::Hello: {
                WireHello hello;
                if (artillery::decode_hello(msg.data, msg.len, &hello)) {
                    handle_hello(&c, hello, auth, all);
                }
                break;
            }
            case WireMsg::Sync: {
                if (!c.seat.joined) {
                    send_error_msg(c, static_cast<uint8_t>(WireMsg::Sync), "need_hello", c.eid);
                    break;
                }
                WireSync sync;
                if (artillery::decode_sync(msg.data, msg.len, &sync)) {
                    handle_sync(&c, sync, auth);
                }
                break;
            }
            case WireMsg::Intent: {
                if (!c.seat.joined) {
                    send_error_msg(c, static_cast<uint8_t>(WireMsg::Intent), "need_hello", c.eid);
                    break;
                }
                WireIntent wi;
                if (artillery::decode_intent(msg.data, msg.len, &wi)) {
                    handle_intent(&c, wi, auth, all);
                }
                break;
            }
            case WireMsg::Aim: {
                if (!c.seat.joined) {
                    break;
                }
                WireAim aim;
                if (artillery::decode_aim(msg.data, msg.len, &aim)) {
                    handle_aim(&c, aim, auth, all);
                }
                break;
            }
            default:
                break;
        }
    }
}

}  // namespace

int main(int argc, char** argv)
{
    int port = ARTILLERY_DEFAULT_PORT;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        }
    }

    const int fd = net::listen_udp(port);
    if (fd < 0) {
        std::fprintf(stderr, "[server] udp listen failed on %d\n", port);
        return 1;
    }
    ART_NET_LOGI(TAG, 0, "udp listen 0.0.0.0:%d", port);
    ART_NET_LOGI(TAG, 0, "wire v%d binary hello/intent/aim/state — logs use eid + t_ms",
                 ARTILLERY_PROTOCOL_VERSION);

    Authority auth;
    Client clients[kMaxClients]{};
    std::array<uint16_t, artillery::kWidth> heights_before{};
    Phase phase_before = Phase::Title;

    for (;;) {
        const uint32_t t = now_ms();
        expire_stale(clients, auth, t);

        for (;;) {
            uint8_t packet[ARTILLERY_UDP_MAX_PACKET];
            net::UdpAddr from{};
            const int n = net::udp_recv(fd, packet, sizeof(packet), &from);
            if (n == 0) {
                break;
            }
            if (n < 0) {
                break;
            }
            int slot = find_by_addr(clients, from);
            if (slot < 0) {
                slot = next_slot(clients);
                if (slot < 0) {
                    // No slot — ignore (cannot send Error without a Link/addr binding easily)
                    continue;
                }
                clients[slot] = Client{};
                clients[slot].addr = from;
                clients[slot].bound = true;
                clients[slot].eid = artillery::trace::next_eid();
                ART_NET_LOGI(TAG, clients[slot].eid, "bind slot=%d", slot);
            }
            Client& c = clients[slot];
            c.last_ms = t;
            c.link.pump_in(packet, n);
            process_messages(fd, c, auth, clients);
            flush_link(fd, c, t, true);
        }

        phase_before = auth.phase();
        const ViewModel before = auth.view();
        heights_before = before.heights;
        auth.tick(16);

        if (auth.keyframe() || auth.scene_dirty() || auth.phase() != phase_before) {
            const bool impact = phase_before == Phase::Firing && auth.phase() != Phase::Firing;
            if (impact) {
                WireState st = artillery::state_from_view(auth.view(), false);
                artillery::collect_height_deltas(heights_before, auth.view().heights, &st);
                broadcast_state(clients, auth, false, &st);
            } else {
                const bool full = auth.phase() == Phase::Aiming && phase_before == Phase::Title;
                broadcast_state(clients, auth, full, nullptr);
            }
        }

        for (int i = 0; i < kMaxClients; ++i) {
            if (clients[i].bound) {
                flush_link(fd, clients[i], t, false);
            }
        }

        timeval tv{};
        tv.tv_usec = 16000;
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        ::select(fd + 1, &rfds, nullptr, nullptr, &tv);
    }
}
