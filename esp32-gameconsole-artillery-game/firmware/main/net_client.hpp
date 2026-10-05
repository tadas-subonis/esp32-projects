#pragma once

#include "artillery/command.hpp"
#include "artillery/match.hpp"
#include "link.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace tc {

class NetClient {
public:
    void start();
    /** Begin Wi-Fi + UDP session (call when the player opts into online). */
    void enable();
    /** Drop the server link and stop reconnecting (local play). */
    void disable();
    void pump();
    bool submit(const artillery::ClientIntent& intent);
    void request_sync(uint32_t seq);

    bool configured() const;
    bool enabled() const;
    bool alive() const;
    bool wifi_up() const;
    /** Hard reject (e.g. server_full) — lobby should show it; reconnect is paused. */
    bool rejected() const;
    const char* last_error() const;
    bool take_snapshot();
    bool pop_cmd(uint32_t* seq, artillery::PlayerId* who, artillery::ClientIntent* intent);
    int seat() const;
    uint32_t seq() const;
    const char* ip() const;
    const char* status() const;
    artillery::ViewModel view() const;

private:
    static constexpr int kCmdCap = 16;

    struct CmdEvent {
        uint32_t seq = 0;
        artillery::PlayerId who = artillery::PlayerId::P0;
        artillery::ClientIntent intent{};
    };

    static void task(void* arg);
    bool init_wifi();
    bool try_connect();
    void close_socket();
    void note_error(const char* err, uint32_t eid = 0);
    void flush_out(bool force);
    void handle_message(const artillery::Link::Message& msg);
    bool queue_hello();
    bool queue_intent(const artillery::ClientIntent& intent);
    void queue_aim(int angle, int power);
    uint32_t now_ms() const;

    SemaphoreHandle_t mu_ = nullptr;
    int fd_ = -1;
    artillery::Link link_;
    int seat_ = -1;
    uint32_t seq_ = 0;
    uint32_t join_eid_ = 0;
    bool want_online_ = false;
    bool wifi_up_ = false;
    bool wifi_stop_req_ = false;
    bool handshaking_ = false;
    bool snapshot_ready_ = false;
    bool rejected_ = false;
    uint32_t backoff_ms_ = 0;
    TickType_t retry_after_ = 0;
    char ip_[16] = "";
    char status_[24] = "off";
    char last_error_[32] = "";
    char token_[12] = "";
    artillery::ViewModel view_{};
    CmdEvent cmds_[kCmdCap]{};
    int cmd_n_ = 0;
};

}  // namespace tc
