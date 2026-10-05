#pragma once

#include "authority.hpp"
#include "link.hpp"
#include "udp.hpp"

namespace artillery {

enum class ProxyKind { Local, Remote };

/**
 * Client-side proxy: same send/pump/view API.
 * Local calls Authority in this process; Remote talks binary UDP via Link.
 */
class ServerProxy {
public:
    ~ServerProxy() { close(); }

    bool open_local(const char* want);
    bool open_remote(const char* host, int port, const char* want);
    void close();

    ApplyResult submit(const ClientIntent& intent);
    /** Remote only: wait up to timeout_ms for seq/error. Use from control I/O, not the frame loop. */
    ApplyResult submit_wait(const ClientIntent& intent, uint32_t timeout_ms = 150);
    void pump(uint32_t dt_ms);

    const ViewModel& view() const { return view_; }
    bool is_local() const { return kind_ == ProxyKind::Local; }
    bool alive() const { return kind_ == ProxyKind::Local || fd_ >= 0; }
    int seat() const { return seat_.seat; }
    const ApplyResult& last() const { return last_; }

private:
    void refresh_local();
    void flush_out(bool force);
    void handle_message(const Link::Message& msg);
    bool queue_intent(const ClientIntent& intent);
    bool queue_aim(int angle, int power);
    uint32_t now_ms() const;

    ProxyKind kind_ = ProxyKind::Local;
    Authority auth_;
    Match replica_{};
    ClientSeat seat_{};
    ViewModel view_{};
    ApplyResult last_{true, ""};
    int fd_ = -1;
    Link link_;
    uint32_t seq_ = 0;
    bool have_replica_ = false;
    char want_[16] = "bot";
};

}  // namespace artillery
