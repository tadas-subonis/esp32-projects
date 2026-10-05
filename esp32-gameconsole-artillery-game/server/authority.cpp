#include "authority.hpp"

#include "artillery_protocol.h"

#include <cstdio>
#include <cstring>

namespace artillery {

Authority::Authority()
{
    match_.reset_title();
}

bool Authority::seat_busy(int seat) const
{
    return seat >= 0 && seat < 2 && taken_[seat] && present_[seat];
}

void Authority::assign_token(ClientSeat* client)
{
    if (client == nullptr) {
        return;
    }
    if (client->token[0] != '\0') {
        return;
    }
    std::snprintf(client->token, sizeof(client->token), "%08x", static_cast<unsigned>(next_token_++));
}

ApplyResult Authority::join(ClientSeat* client, int protocol, const char* want)
{
    if (client == nullptr) {
        return apply_reject("need_hello");
    }
    if (protocol != 0 && protocol != ARTILLERY_PROTOCOL_VERSION) {
        return apply_reject("protocol_mismatch");
    }
    if (client->joined) {
        disconnect(client);
    }

    const Mode want_mode = mode_from_name(want);
    ClientSeat next{};
    next.joined = true;
    if (client->token[0]) {
        std::snprintf(next.token, sizeof(next.token), "%s", client->token);
    }

    if (want_mode == Mode::Hotseat) {
        next.both = true;
        next.seat = 0;
        assign_token(&next);
        taken_[0] = true;
        if (!present_[0]) {
            present_[0] = true;
            ++online_n_;
        }
        std::snprintf(tokens_[0], sizeof(tokens_[0]), "%s", next.token);
        *client = next;
        return apply_ok();
    }

    if (want_mode == Mode::Pvp) {
        int seat = -1;
        if (next.token[0]) {
            for (int s = 0; s < 2; ++s) {
                if (taken_[s] && std::strcmp(tokens_[s], next.token) == 0) {
                    seat = s;
                    break;
                }
            }
        }
        // Reclaim held seats even when still "present" — the TCP adapter kicks the old socket.
        if (seat < 0) {
            for (int s = 0; s < 2; ++s) {
                if (!present_[s]) {
                    seat = s;
                    break;
                }
            }
        }
        if (seat < 0) {
            return apply_reject("server_full");
        }
        next.seat = seat;
        next.both = false;
        assign_token(&next);
        if (!present_[seat]) {
            ++online_n_;
        }
        taken_[seat] = true;
        present_[seat] = true;
        std::snprintf(tokens_[seat], sizeof(tokens_[seat]), "%s", next.token);
        *client = next;
        return apply_ok();
    }

    if (seat_busy(0)) {
        return apply_reject("server_full");
    }
    next.seat = 0;
    next.both = false;
    assign_token(&next);
    if (!present_[0]) {
            ++online_n_;
    }
    taken_[0] = true;
    present_[0] = true;
    std::snprintf(tokens_[0], sizeof(tokens_[0]), "%s", next.token);
    *client = next;
    return apply_ok();
}

void Authority::leave(ClientSeat* client)
{
    if (client == nullptr || !client->joined) {
        return;
    }
    if (!client->both && client->seat >= 0 && client->seat < 2) {
        if (present_[client->seat]) {
            --online_n_;
        }
        taken_[client->seat] = false;
        present_[client->seat] = false;
        tokens_[client->seat][0] = 0;
    } else if (client->both) {
        if (present_[0]) {
            --online_n_;
        }
        taken_[0] = false;
        present_[0] = false;
        tokens_[0][0] = 0;
    }
    *client = ClientSeat{};
}

void Authority::disconnect(ClientSeat* client)
{
    if (client == nullptr || !client->joined) {
        return;
    }
    const int seat = client->both ? 0 : client->seat;
    if (seat >= 0 && seat < 2 && present_[seat]) {
        present_[seat] = false;
        --online_n_;
    }
    client->joined = false;
}

PlayerId Authority::actor(const ClientSeat& client) const
{
    if (client.both) {
        return match_.active();
    }
    return static_cast<PlayerId>(client.seat);
}

void Authority::append_log(PlayerId who, const ClientIntent& intent, bool keyframe)
{
    ++seq_;
    LoggedCommand entry{seq_, who, intent};
    if (log_len_ < kLogCap) {
        log_[log_len_++] = entry;
    } else {
        for (int i = 1; i < kLogCap; ++i) {
            log_[i - 1] = log_[i];
        }
        log_[kLogCap - 1] = entry;
    }
    if (keyframe) {
        keyframe_ = true;
    }
}

ApplyResult Authority::submit(const ClientSeat& client, ClientIntent intent)
{
    if (!client.joined) {
        return apply_reject("need_hello");
    }
    if (intent.kind == ClientIntent::Start && static_cast<Mode>(intent.value) == Mode::Pvp &&
        online_n_ < 2) {
        return apply_reject("need_two_players");
    }
    const PlayerId who = actor(client);
    const ApplyResult result = match_.apply(who, intent);
    if (result.accepted) {
        const bool key = intent.kind == ClientIntent::Start || intent.kind == ClientIntent::Rematch ||
                         intent.kind == ClientIntent::ToTitle || intent.kind == ClientIntent::Fire;
        append_log(who, intent, key);
    }
    return result;
}

ApplyResult Authority::set_aim(const ClientSeat& client, int angle, int power)
{
    if (!client.joined) {
        return apply_reject("need_hello");
    }
    const PlayerId who = actor(client);
    if (!match_.can_act(who)) {
        return apply_reject("not_your_turn");
    }
    ApplyResult a = match_.apply(who, ClientIntent{ClientIntent::SetAngle, angle});
    if (!a.accepted) {
        return a;
    }
    return match_.apply(who, ClientIntent{ClientIntent::SetPower, power});
}

void Authority::note_bot_fire()
{
    Shot shot{};
    if (!match_.consume_bot_fire(&shot)) {
        return;
    }
    ClientIntent angle{ClientIntent::SetAngle, shot.angle_deg};
    ClientIntent power{ClientIntent::SetPower, shot.power};
    ClientIntent fire{};
    fire.kind = ClientIntent::Fire;
    append_log(PlayerId::P1, angle, false);
    append_log(PlayerId::P1, power, false);
    append_log(PlayerId::P1, fire, true);
}

void Authority::tick(uint32_t dt_ms)
{
    match_.tick(dt_ms);
    note_bot_fire();
    if (match_.scene_dirty()) {
        keyframe_ = true;
    }
}

ViewModel Authority::view() const
{
    ViewModel v = match_.view();
    v.snap.seq = seq_;
    v.have_heights = true;
    v.players = online_n_;
    return v;
}

int Authority::commands_after(uint32_t after_seq, LoggedCommand* out, int cap) const
{
    if (out == nullptr || cap <= 0) {
        return -1;
    }
    if (after_seq == seq_) {
        return 0;
    }
    if (log_len_ == 0) {
        return -1;
    }
    const uint32_t oldest = log_[0].seq;
    if (after_seq + 1 < oldest) {
        return -1;
    }
    int n = 0;
    for (int i = 0; i < log_len_ && n < cap; ++i) {
        if (log_[i].seq > after_seq) {
            out[n++] = log_[i];
        }
    }
    if (n == 0 || out[n - 1].seq != seq_) {
        return -1;
    }
    if (out[0].seq != after_seq + 1) {
        return -1;
    }
    return n;
}

}  // namespace artillery
