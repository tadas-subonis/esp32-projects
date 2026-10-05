#include "proxy.hpp"

#include "artillery_protocol.h"
#include "tcp.hpp"
#include "wire.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

namespace artillery {

uint32_t ServerProxy::now_ms() const
{
    using clock = std::chrono::steady_clock;
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch()).count());
}

void ServerProxy::refresh_local()
{
    view_ = auth_.view();
}

void ServerProxy::flush_out(bool force)
{
    if (fd_ < 0) {
        return;
    }
    uint8_t buf[ARTILLERY_UDP_MAX_PACKET];
    for (int i = 0; i < 4; ++i) {
        const int n = link_.compose_out(buf, sizeof(buf), now_ms(), force && i == 0);
        if (n <= 0) {
            break;
        }
        if (net::udp_send(fd_, buf, n) < 0) {
            last_ = apply_reject("disconnected");
            close();
            return;
        }
        force = false;
    }
}

bool ServerProxy::queue_intent(const ClientIntent& intent)
{
    WireIntent wi;
    wi.intent = intent;
    uint8_t buf[16];
    const int n = encode_intent(buf, sizeof(buf), wi);
    if (n <= 0) {
        return false;
    }
    return link_.send_reliable(static_cast<uint8_t>(WireMsg::Intent), buf, static_cast<uint16_t>(n));
}

bool ServerProxy::queue_aim(int angle, int power)
{
    WireAim aim;
    aim.who = 0;
    aim.angle = static_cast<uint8_t>(angle);
    aim.power = static_cast<uint8_t>(power);
    uint8_t buf[8];
    const int n = encode_aim(buf, sizeof(buf), aim);
    if (n <= 0) {
        return false;
    }
    link_.send_unreliable(static_cast<uint8_t>(WireMsg::Aim), buf, static_cast<uint16_t>(n));
    return true;
}

bool ServerProxy::open_local(const char* want)
{
    close();
    kind_ = ProxyKind::Local;
    last_ = auth_.join(&seat_, ARTILLERY_PROTOCOL_VERSION, want);
    if (!last_.accepted) {
        return false;
    }
    refresh_local();
    return true;
}

bool ServerProxy::open_remote(const char* host, int port, const char* want)
{
    close();
    kind_ = ProxyKind::Remote;
    std::snprintf(want_, sizeof(want_), "%s", want ? want : "bot");
    for (int i = 0; i < 25 && fd_ < 0; ++i) {
        fd_ = net::connect_udp(host, port);
        if (fd_ < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    if (fd_ < 0) {
        last_ = apply_reject("connect_failed");
        return false;
    }

    WireHello hello;
    hello.protocol = ARTILLERY_PROTOCOL_VERSION;
    hello.want = want_to_u8(want_);
    hello.t_ms = now_ms();
    uint8_t buf[48];
    const int n = encode_hello(buf, sizeof(buf), hello);
    if (n <= 0 || !link_.send_reliable(static_cast<uint8_t>(WireMsg::Hello), buf, static_cast<uint16_t>(n))) {
        close();
        last_ = apply_reject("connect_failed");
        return false;
    }
    last_ = apply_ok();
    for (int i = 0; i < 40; ++i) {
        flush_out(true);
        pump(0);
        if (!last_.accepted) {
            close();
            return false;
        }
        if (seat_.joined) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    close();
    last_ = apply_reject("no_hello");
    return false;
}

void ServerProxy::close()
{
    if (kind_ == ProxyKind::Local) {
        auth_.leave(&seat_);
    }
    if (fd_ >= 0) {
        net::close_fd(fd_);
        fd_ = -1;
    }
    link_.reset();
    seat_ = ClientSeat{};
    have_replica_ = false;
    seq_ = 0;
}

ApplyResult ServerProxy::submit(const ClientIntent& intent)
{
    if (kind_ == ProxyKind::Local) {
        last_ = auth_.submit(seat_, intent);
        refresh_local();
        return last_;
    }
    if (intent.kind == ClientIntent::SetAngle || intent.kind == ClientIntent::NudgeAngle ||
        intent.kind == ClientIntent::SetPower || intent.kind == ClientIntent::NudgePower) {
        // Convert nudges/sets into absolute aim for the wire.
        int angle = view_.snap.angle;
        int power = view_.snap.power;
        if (have_replica_) {
            angle = replica_.angle();
            power = replica_.power();
        }
        if (intent.kind == ClientIntent::SetAngle) {
            angle = intent.value;
        } else if (intent.kind == ClientIntent::SetPower) {
            power = intent.value;
        } else if (intent.kind == ClientIntent::NudgeAngle) {
            angle += intent.value;
        } else if (intent.kind == ClientIntent::NudgePower) {
            power += intent.value;
        }
        if (have_replica_) {
            replica_.apply(seat_.both ? replica_.active() : static_cast<PlayerId>(seat_.seat),
                           ClientIntent{ClientIntent::SetAngle, angle});
            replica_.apply(seat_.both ? replica_.active() : static_cast<PlayerId>(seat_.seat),
                           ClientIntent{ClientIntent::SetPower, power});
            view_ = replica_.view();
            view_.snap.seq = seq_;
        }
        queue_aim(angle, power);
        flush_out(true);
        last_ = apply_ok();
        pump(0);
        return last_;
    }
    if (!queue_intent(intent)) {
        last_ = apply_reject("disconnected");
        close();
        return last_;
    }
    last_ = apply_ok();
    flush_out(true);
    pump(0);
    return last_;
}

ApplyResult ServerProxy::submit_wait(const ClientIntent& intent, uint32_t timeout_ms)
{
    if (kind_ == ProxyKind::Local) {
        return submit(intent);
    }
    const uint32_t before = seq_;
    last_ = apply_ok();
    if (intent.kind == ClientIntent::SetAngle || intent.kind == ClientIntent::SetPower ||
        intent.kind == ClientIntent::NudgeAngle || intent.kind == ClientIntent::NudgePower) {
        submit(intent);
    } else if (!queue_intent(intent)) {
        last_ = apply_reject("disconnected");
        close();
        return last_;
    } else {
        flush_out(true);
    }
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms == 0 ? 150 : timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        pump(0);
        if (!last_.accepted) {
            return last_;
        }
        if (seq_ > before) {
            return last_;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (seq_ == before && last_.accepted) {
        last_ = apply_reject("timeout");
    }
    return last_;
}

void ServerProxy::handle_message(const Link::Message& msg)
{
    switch (static_cast<WireMsg>(msg.type)) {
        case WireMsg::Welcome: {
            WireWelcome w;
            if (!decode_welcome(msg.data, msg.len, &w)) {
                return;
            }
            seat_.joined = true;
            seat_.seat = w.seat;
            seat_.both = w.seat < 0;
            if (w.token[0]) {
                std::snprintf(seat_.token, sizeof(seat_.token), "%s", w.token);
            }
            seq_ = w.seq;
            last_ = apply_ok();
            return;
        }
        case WireMsg::Error: {
            WireError e;
            if (decode_error(msg.data, msg.len, &e)) {
                last_ = apply_reject(e.error[0] ? e.error : "rejected");
            } else {
                last_ = apply_reject("rejected");
            }
            return;
        }
        case WireMsg::Cmd: {
            WireCmd cmd;
            if (!decode_cmd(msg.data, msg.len, &cmd)) {
                return;
            }
            if (have_replica_ && cmd.seq == seq_ + 1) {
                replica_.apply(cmd.who, cmd.intent);
                seq_ = cmd.seq;
                view_ = replica_.view();
                view_.snap.seq = seq_;
                last_ = apply_ok();
            }
            return;
        }
        case WireMsg::Aim: {
            WireAim aim;
            if (!decode_aim(msg.data, msg.len, &aim) || !have_replica_) {
                return;
            }
            const PlayerId who = static_cast<PlayerId>(aim.who);
            replica_.apply(who, ClientIntent{ClientIntent::SetAngle, aim.angle});
            replica_.apply(who, ClientIntent{ClientIntent::SetPower, aim.power});
            view_ = replica_.view();
            view_.snap.seq = seq_;
            return;
        }
        case WireMsg::State: {
            WireState st;
            if (!decode_state(msg.data, msg.len, &st)) {
                return;
            }
            ViewModel next = view_;
            if (have_replica_ && (st.flags & WireStateDeltas) && !(st.flags & WireStateFullHeights)) {
                next = replica_.view();
                next.have_heights = true;
                next.heights = replica_.world().heights();
            }
            apply_state_to_view(st, &next);
            replica_.restore(next);
            have_replica_ = true;
            seq_ = st.seq;
            view_ = replica_.view();
            view_.snap.seq = seq_;
            view_.players = st.players;
            last_ = apply_ok();
            return;
        }
        default:
            return;
    }
}

void ServerProxy::pump(uint32_t dt_ms)
{
    if (kind_ == ProxyKind::Local) {
        if (dt_ms > 0) {
            auth_.tick(dt_ms);
        }
        refresh_local();
        return;
    }
    if (fd_ < 0) {
        return;
    }
    for (;;) {
        uint8_t packet[ARTILLERY_UDP_MAX_PACKET];
        const int n = net::udp_recv(fd_, packet, sizeof(packet), nullptr);
        if (n == 0) {
            break;
        }
        if (n < 0) {
            last_ = apply_reject("disconnected");
            close();
            return;
        }
        link_.pump_in(packet, n);
    }
    Link::Message msg;
    while (link_.pop_message(&msg)) {
        handle_message(msg);
    }
    flush_out(false);
    if (have_replica_ && replica_.phase() == Phase::Firing && dt_ms > 0) {
        replica_.step_fx(dt_ms);
        view_ = replica_.view();
        view_.snap.seq = seq_;
    }
}

}  // namespace artillery
