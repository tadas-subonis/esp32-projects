#pragma once

#include "artillery/command.hpp"
#include "artillery/match.hpp"

namespace artillery {

/** One client identity as seen by the authoritative sim (UDP or in-process). */
struct ClientSeat {
    int seat = -1;
    bool both = false;
    bool joined = false;
    char token[9] = {};
};

struct LoggedCommand {
    uint32_t seq = 0;
    PlayerId who = PlayerId::P0;
    ClientIntent intent{};
};

/**
 * In-process server: owns Match, validates intents, ticks physics/bot.
 * UDP `artillery-server` and the host's local proxy call this directly.
 *
 * Match is a reduce: accepted intents (plus bot fire) append to a command log.
 * Reconnect/sync either replays log entries after a client's seq or sends a snapshot.
 */
class Authority {
public:
    static constexpr int kLogCap = 48;

    Authority();

    ApplyResult join(ClientSeat* client, int protocol, const char* want);
    void leave(ClientSeat* client);
    void disconnect(ClientSeat* client);

    PlayerId actor(const ClientSeat& client) const;
    ApplyResult submit(const ClientSeat& client, ClientIntent intent);
    /** Absolute aim sample — updates Match without growing the command log. */
    ApplyResult set_aim(const ClientSeat& client, int angle, int power);
    void tick(uint32_t dt_ms);

    ViewModel view() const;
    Phase phase() const { return match_.phase(); }
    bool scene_dirty() const { return match_.scene_dirty(); }
    void clear_scene_dirty() { match_.clear_scene_dirty(); }
    int joined_count() const { return online_n_; }
    uint32_t seq() const { return seq_; }
    bool keyframe() const { return keyframe_; }
    void clear_keyframe() { keyframe_ = false; }

    int commands_after(uint32_t after_seq, LoggedCommand* out, int cap) const;

private:
    bool seat_busy(int seat) const;
    void assign_token(ClientSeat* client);
    void append_log(PlayerId who, const ClientIntent& intent, bool keyframe);
    void note_bot_fire();

    Match match_{};
    bool taken_[2] = {false, false};
    bool present_[2] = {false, false};
    char tokens_[2][9] = {};
    int online_n_ = 0;
    uint32_t seq_ = 0;
    uint32_t next_token_ = 1;
    bool keyframe_ = true;
    LoggedCommand log_[kLogCap]{};
    int log_len_ = 0;
};

}  // namespace artillery
