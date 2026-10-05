#include "net_client.hpp"

#include "artillery_protocol.h"
#include "event_trace.hpp"
#include "secrets.h"
#include "wire.hpp"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "lwip/sockets.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace tc {
namespace {

const char* TAG = "net";
constexpr int kGotIp = BIT0;
constexpr int kDoConnect = BIT1;
EventGroupHandle_t g_wifi_events = nullptr;
char g_ip[16] = "";

void on_wifi(void*, esp_event_base_t base, int32_t id, void* data)
{
    // Do not call esp_wifi_* from this handler: ESP-Hosted RPC on sys_evt overflows the stack.
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (g_wifi_events) {
            xEventGroupSetBits(g_wifi_events, kDoConnect);
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (g_wifi_events) {
            xEventGroupClearBits(g_wifi_events, kGotIp);
            xEventGroupSetBits(g_wifi_events, kDoConnect);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t* event = static_cast<ip_event_got_ip_t*>(data);
        std::snprintf(g_ip, sizeof(g_ip), IPSTR, IP2STR(&event->ip_info.ip));
        ART_NET_LOGI(TAG, 0, "got_ip %s", g_ip);
        if (g_wifi_events) {
            xEventGroupSetBits(g_wifi_events, kGotIp);
        }
    }
}

bool ssid_configured() { return ARTILLERY_WIFI_SSID[0] != '\0'; }

bool set_nonblock(int fd, bool on)
{
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    if (on) {
        return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
    }
    return ::fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) == 0;
}

}  // namespace

uint32_t NetClient::now_ms() const
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void NetClient::start()
{
    mu_ = xSemaphoreCreateMutex();
    std::snprintf(status_, sizeof(status_), "off");
    xTaskCreate(&NetClient::task, "net", 24576, this, 5, nullptr);
}

void NetClient::enable()
{
    if (!ssid_configured()) {
        ART_NET_LOGW(TAG, 0, "enable ignored — WIFI_SSID empty");
        std::snprintf(status_, sizeof(status_), "no_ssid");
        note_error("no_ssid");
        return;
    }
    want_online_ = true;
    wifi_stop_req_ = false;
    rejected_ = false;
    last_error_[0] = 0;
    backoff_ms_ = 0;
    retry_after_ = 0;
    std::snprintf(status_, sizeof(status_), "want");
    ART_NET_LOGI(TAG, 0, "online_enabled");
}

void NetClient::disable()
{
    // Never call esp_wifi_* here — game task; hosted C6 RPC must stay on net task.
    want_online_ = false;
    wifi_stop_req_ = true;
    rejected_ = false;
    last_error_[0] = 0;
    backoff_ms_ = 0;
    retry_after_ = 0;
    close_socket();
    xSemaphoreTake(mu_, portMAX_DELAY);
    seat_ = -1;
    seq_ = 0;
    join_eid_ = 0;
    snapshot_ready_ = false;
    cmd_n_ = 0;
    ip_[0] = 0;
    std::snprintf(status_, sizeof(status_), "off");
    xSemaphoreGive(mu_);
    ART_NET_LOGI(TAG, 0, "online_disabled");
}

void NetClient::note_error(const char* err, uint32_t eid)
{
    if (err == nullptr || err[0] == 0) {
        return;
    }
    std::snprintf(last_error_, sizeof(last_error_), "%s", err);
    std::snprintf(status_, sizeof(status_), "%.23s", err);
    if (std::strcmp(err, "server_full") == 0 || std::strcmp(err, "protocol_mismatch") == 0) {
        rejected_ = true;
        if (backoff_ms_ < 1000) {
            backoff_ms_ = 3000;
        } else if (backoff_ms_ < 20000) {
            backoff_ms_ = backoff_ms_ * 2;
        }
        retry_after_ = xTaskGetTickCount() + pdMS_TO_TICKS(backoff_ms_);
        ART_NET_LOGW(TAG, eid ? eid : join_eid_, "reject %s — backoff %lu ms", err,
                     static_cast<unsigned long>(backoff_ms_));
    }
}

void NetClient::flush_out(bool force)
{
    if (fd_ < 0) {
        return;
    }
    uint8_t buf[ARTILLERY_UDP_MAX_PACKET];
    for (int i = 0; i < 4; ++i) {
        xSemaphoreTake(mu_, portMAX_DELAY);
        const int n = link_.compose_out(buf, sizeof(buf), now_ms(), force && i == 0);
        const int fd = fd_;
        xSemaphoreGive(mu_);
        if (n <= 0 || fd < 0) {
            break;
        }
        const int sent = ::send(fd, buf, static_cast<size_t>(n), 0);
        if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            ART_NET_LOGW(TAG, join_eid_, "udp_send errno=%d", errno);
            close_socket();
            std::snprintf(status_, sizeof(status_), "drop");
            return;
        }
        force = false;
    }
}

bool NetClient::queue_hello()
{
    artillery::WireHello hello;
    hello.protocol = ARTILLERY_PROTOCOL_VERSION;
    hello.want = artillery::want_to_u8(ARTILLERY_WANT);
    if (token_[0]) {
        std::memcpy(hello.token, token_, ARTILLERY_TOKEN_BYTES);
        hello.token[ARTILLERY_TOKEN_BYTES] = 0;
        hello.seq = seq_;
    }
    hello.eid = join_eid_;
    hello.t_ms = now_ms();
    uint8_t buf[48];
    const int n = artillery::encode_hello(buf, sizeof(buf), hello);
    if (n <= 0) {
        return false;
    }
    return link_.send_reliable(static_cast<uint8_t>(artillery::WireMsg::Hello), buf,
                               static_cast<uint16_t>(n));
}

bool NetClient::queue_intent(const artillery::ClientIntent& intent)
{
    artillery::WireIntent wi;
    wi.intent = intent;
    uint8_t buf[16];
    const int n = artillery::encode_intent(buf, sizeof(buf), wi);
    if (n <= 0) {
        return false;
    }
    return link_.send_reliable(static_cast<uint8_t>(artillery::WireMsg::Intent), buf,
                               static_cast<uint16_t>(n));
}

void NetClient::queue_aim(int angle, int power)
{
    artillery::WireAim aim;
    aim.who = 0;
    aim.angle = static_cast<uint8_t>(angle);
    aim.power = static_cast<uint8_t>(power);
    uint8_t buf[8];
    const int n = artillery::encode_aim(buf, sizeof(buf), aim);
    if (n > 0) {
        link_.send_unreliable(static_cast<uint8_t>(artillery::WireMsg::Aim), buf,
                              static_cast<uint16_t>(n));
    }
}

void NetClient::task(void* arg)
{
    auto* self = static_cast<NetClient*>(arg);
    for (;;) {
        if (self->wifi_stop_req_) {
            self->wifi_stop_req_ = false;
            self->close_socket();
            if (self->wifi_up_) {
                ART_NET_LOGI(TAG, 0, "wifi_disconnect (net task)");
                esp_wifi_disconnect();
                self->wifi_up_ = false;
            }
        }
        if (!self->want_online_) {
            if (self->fd_ >= 0) {
                self->close_socket();
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (self->retry_after_ != 0 && xTaskGetTickCount() < self->retry_after_) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (!self->wifi_up_) {
            if (!self->init_wifi()) {
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
        }
        if (g_wifi_events) {
            const EventBits_t bits = xEventGroupGetBits(g_wifi_events);
            if ((bits & kGotIp) == 0) {
                ART_NET_LOGW(TAG, 0, "wifi_lost");
                self->wifi_up_ = false;
                self->close_socket();
                if (bits & kDoConnect) {
                    xEventGroupClearBits(g_wifi_events, kDoConnect);
                    esp_wifi_connect();
                }
                vTaskDelay(pdMS_TO_TICKS(200));
                continue;
            }
            if (bits & kDoConnect) {
                xEventGroupClearBits(g_wifi_events, kDoConnect);
            }
        }
        if (self->fd_ < 0) {
            if (!self->try_connect()) {
                if (self->rejected_) {
                    // note_error already set backoff
                } else if (self->backoff_ms_ == 0) {
                    self->backoff_ms_ = 200;
                } else if (self->backoff_ms_ < 2000) {
                    self->backoff_ms_ = self->backoff_ms_ * 2;
                }
                if (!self->rejected_) {
                    self->retry_after_ = xTaskGetTickCount() + pdMS_TO_TICKS(self->backoff_ms_);
                    ART_NET_LOGW(TAG, self->join_eid_, "connect_fail — retry in %lu ms",
                                 static_cast<unsigned long>(self->backoff_ms_));
                }
            } else {
                self->backoff_ms_ = 0;
                self->retry_after_ = 0;
                self->rejected_ = false;
            }
        } else if (!self->handshaking_) {
            // Heartbeat / ack carrier while joined.
            self->flush_out(false);
        }
        vTaskDelay(pdMS_TO_TICKS(self->fd_ >= 0 ? 50 : 50));
    }
}

bool NetClient::init_wifi()
{
    if (!ssid_configured()) {
        std::snprintf(status_, sizeof(status_), "no_ssid");
        ART_NET_LOGW(TAG, 0, "WIFI_SSID empty — staying offline");
        vTaskDelay(pdMS_TO_TICKS(60000));
        return false;
    }
    if (g_wifi_events == nullptr) {
        g_wifi_events = xEventGroupCreate();
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi,
                                                             nullptr, nullptr));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_wifi,
                                                             nullptr, nullptr));
        wifi_config_t wifi_cfg = {};
        std::snprintf(reinterpret_cast<char*>(wifi_cfg.sta.ssid), sizeof(wifi_cfg.sta.ssid), "%s",
                      ARTILLERY_WIFI_SSID);
        std::snprintf(reinterpret_cast<char*>(wifi_cfg.sta.password), sizeof(wifi_cfg.sta.password),
                      "%s", ARTILLERY_WIFI_PASS);
        wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
        ESP_ERROR_CHECK(esp_wifi_start());
        ART_NET_LOGI(TAG, 0, "wifi_start ssid=%s", ARTILLERY_WIFI_SSID);
    }

    std::snprintf(status_, sizeof(status_), "wifi");
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(15000);
    while ((xEventGroupGetBits(g_wifi_events) & kGotIp) == 0 && xTaskGetTickCount() < deadline) {
        const EventBits_t bits = xEventGroupWaitBits(g_wifi_events, kGotIp | kDoConnect, pdFALSE,
                                                     pdFALSE, pdMS_TO_TICKS(200));
        if (bits & kDoConnect) {
            xEventGroupClearBits(g_wifi_events, kDoConnect);
            ART_NET_LOGI(TAG, 0, "wifi_connect");
            esp_wifi_connect();
        }
    }
    wifi_up_ = (xEventGroupGetBits(g_wifi_events) & kGotIp) != 0;
    if (!wifi_up_) {
        std::snprintf(status_, sizeof(status_), "no_ip");
        ART_NET_LOGW(TAG, 0, "no_ip after 15s");
        return false;
    }
    xSemaphoreTake(mu_, portMAX_DELAY);
    std::snprintf(ip_, sizeof(ip_), "%s", g_ip);
    std::snprintf(status_, sizeof(status_), "ip");
    xSemaphoreGive(mu_);
    return true;
}

bool NetClient::try_connect()
{
    join_eid_ = artillery::trace::next_eid();
    const uint32_t eid = join_eid_;
    const uint32_t t0 = now_ms();
    ART_NET_LOGI(TAG, eid, "join_begin host=%s:%d udp", ARTILLERY_SERVER_HOST, ARTILLERY_SERVER_PORT);

    const int fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (fd < 0) {
        std::snprintf(status_, sizeof(status_), "sock");
        ART_NET_LOGE(TAG, eid, "socket_fail errno=%d", errno);
        return false;
    }
    set_nonblock(fd, true);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(ARTILLERY_SERVER_PORT));
    if (::inet_pton(AF_INET, ARTILLERY_SERVER_HOST, &addr.sin_addr) != 1) {
        ART_NET_LOGE(TAG, eid, "bad_host %s", ARTILLERY_SERVER_HOST);
        ::close(fd);
        std::snprintf(status_, sizeof(status_), "bad_host");
        return false;
    }
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ART_NET_LOGW(TAG, eid, "udp_connect_fail errno=%d", errno);
        ::close(fd);
        std::snprintf(status_, sizeof(status_), "udp_fail");
        return false;
    }

    xSemaphoreTake(mu_, portMAX_DELAY);
    fd_ = fd;
    link_.reset();
    handshaking_ = true;
    xSemaphoreGive(mu_);

    if (!queue_hello()) {
        close_socket();
        std::snprintf(status_, sizeof(status_), "hello_fail");
        return false;
    }
    ART_NET_LOGI(TAG, eid, "hello_queued");

    constexpr int kHelloBudgetMs = 2500;
    const TickType_t hello_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(kHelloBudgetMs);
    while (handshaking_ && !rejected_ && xTaskGetTickCount() < hello_deadline) {
        flush_out(true);
        uint8_t packet[ARTILLERY_UDP_MAX_PACKET];
        const int n = ::recv(fd, packet, sizeof(packet), 0);
        if (n > 0) {
            xSemaphoreTake(mu_, portMAX_DELAY);
            link_.pump_in(packet, n);
            artillery::Link::Message msg;
            while (link_.pop_message(&msg)) {
                handle_message(msg);
            }
            xSemaphoreGive(mu_);
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

    if (rejected_) {
        ART_NET_LOGW(TAG, eid, "hello_rejected err=%s dt_ms=%lu", last_error_,
                     static_cast<unsigned long>(now_ms() - t0));
        close_socket();
        return false;
    }
    if (handshaking_) {
        ART_NET_LOGW(TAG, eid, "no_welcome dt_ms=%lu", static_cast<unsigned long>(now_ms() - t0));
        close_socket();
        std::snprintf(status_, sizeof(status_), "no_hello");
        note_error("no_hello", eid);
        return false;
    }

    std::snprintf(status_, sizeof(status_), "pvp");
    ART_NET_LOGI(TAG, eid, "joined seat=%d dt_ms=%lu", seat_,
                 static_cast<unsigned long>(now_ms() - t0));
    return true;
}

void NetClient::close_socket()
{
    xSemaphoreTake(mu_, portMAX_DELAY);
    if (fd_ >= 0) {
        ART_NET_LOGI(TAG, join_eid_, "socket_close fd=%d", fd_);
        ::close(fd_);
        fd_ = -1;
    }
    link_.reset();
    seat_ = -1;
    handshaking_ = false;
    snapshot_ready_ = false;
    cmd_n_ = 0;
    xSemaphoreGive(mu_);
}

void NetClient::handle_message(const artillery::Link::Message& msg)
{
    using artillery::WireMsg;
    switch (static_cast<WireMsg>(msg.type)) {
        case WireMsg::Welcome: {
            artillery::WireWelcome w;
            if (!artillery::decode_welcome(msg.data, msg.len, &w)) {
                return;
            }
            seat_ = w.seat;
            if (w.token[0]) {
                std::snprintf(token_, sizeof(token_), "%s", w.token);
            }
            seq_ = w.seq;
            rejected_ = false;
            last_error_[0] = 0;
            handshaking_ = false;
            std::snprintf(status_, sizeof(status_), "joined");
            ART_NET_LOGI(TAG, join_eid_, "welcome seat=%d seq=%lu token=%s", seat_,
                         static_cast<unsigned long>(seq_), token_);
            return;
        }
        case WireMsg::Error: {
            artillery::WireError e;
            if (artillery::decode_error(msg.data, msg.len, &e)) {
                ART_NET_LOGW(TAG, join_eid_, "server_error cmd=%u err=%s", e.cmd, e.error);
                note_error(e.error, join_eid_);
            }
            return;
        }
        case WireMsg::Cmd: {
            artillery::WireCmd cmd;
            if (!artillery::decode_cmd(msg.data, msg.len, &cmd)) {
                return;
            }
            if (cmd_n_ < kCmdCap) {
                cmds_[cmd_n_++] = CmdEvent{cmd.seq, cmd.who, cmd.intent};
            }
            return;
        }
        case WireMsg::Aim: {
            artillery::WireAim aim;
            if (!artillery::decode_aim(msg.data, msg.len, &aim)) {
                return;
            }
            // Reflect opponent aim into view for HUD; game_app also applies via cmds when needed.
            if (aim.who < 2) {
                view_.tank_angle[aim.who] = aim.angle;
                if (static_cast<int>(view_.snap.active) == aim.who) {
                    view_.snap.angle = aim.angle;
                    view_.snap.power = aim.power;
                }
            }
            return;
        }
        case WireMsg::State: {
            artillery::WireState st;
            if (!artillery::decode_state(msg.data, msg.len, &st)) {
                return;
            }
            artillery::ViewModel next = view_;
            if ((st.flags & artillery::WireStateDeltas) && !(st.flags & artillery::WireStateFullHeights) &&
                view_.have_heights) {
                next.heights = view_.heights;
                next.have_heights = true;
            }
            artillery::apply_state_to_view(st, &next);
            view_ = next;
            seq_ = st.seq;
            snapshot_ready_ = true;
            return;
        }
        default:
            return;
    }
}

void NetClient::pump()
{
    if (fd_ < 0 || handshaking_) {
        return;
    }
    for (;;) {
        uint8_t packet[ARTILLERY_UDP_MAX_PACKET];
        const int n = ::recv(fd_, packet, sizeof(packet), 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            ART_NET_LOGW(TAG, join_eid_, "recv_drop errno=%d", errno);
            close_socket();
            std::snprintf(status_, sizeof(status_), "drop");
            return;
        }
        if (n == 0) {
            break;
        }
        xSemaphoreTake(mu_, portMAX_DELAY);
        link_.pump_in(packet, n);
        artillery::Link::Message msg;
        while (link_.pop_message(&msg)) {
            handle_message(msg);
        }
        xSemaphoreGive(mu_);
    }
    flush_out(false);
}

bool NetClient::submit(const artillery::ClientIntent& intent)
{
    xSemaphoreTake(mu_, portMAX_DELAY);
    bool ok = false;
    if (intent.kind == artillery::ClientIntent::SetAngle ||
        intent.kind == artillery::ClientIntent::SetPower ||
        intent.kind == artillery::ClientIntent::NudgeAngle ||
        intent.kind == artillery::ClientIntent::NudgePower) {
        int angle = view_.snap.angle;
        int power = view_.snap.power;
        if (intent.kind == artillery::ClientIntent::SetAngle) {
            angle = intent.value;
        } else if (intent.kind == artillery::ClientIntent::SetPower) {
            power = intent.value;
        } else if (intent.kind == artillery::ClientIntent::NudgeAngle) {
            angle += intent.value;
        } else {
            power += intent.value;
        }
        if (angle < artillery::kMinAngle) {
            angle = artillery::kMinAngle;
        }
        if (angle > artillery::kMaxAngle) {
            angle = artillery::kMaxAngle;
        }
        if (power < artillery::kMinPower) {
            power = artillery::kMinPower;
        }
        if (power > artillery::kMaxPower) {
            power = artillery::kMaxPower;
        }
        view_.snap.angle = angle;
        view_.snap.power = power;
        queue_aim(angle, power);
        ok = true;
    } else {
        ok = queue_intent(intent);
    }
    xSemaphoreGive(mu_);
    if (ok) {
        flush_out(true);
    }
    return ok;
}

void NetClient::request_sync(uint32_t seq)
{
    artillery::WireSync sync;
    sync.seq = seq;
    uint8_t buf[8];
    const int n = artillery::encode_sync(buf, sizeof(buf), sync);
    if (n > 0) {
        xSemaphoreTake(mu_, portMAX_DELAY);
        link_.send_reliable(static_cast<uint8_t>(artillery::WireMsg::Sync), buf,
                            static_cast<uint16_t>(n));
        xSemaphoreGive(mu_);
        flush_out(true);
    }
}

bool NetClient::configured() const { return ARTILLERY_WIFI_SSID[0] != '\0'; }

bool NetClient::enabled() const { return want_online_; }

bool NetClient::alive() const { return fd_ >= 0 && !handshaking_; }

bool NetClient::wifi_up() const { return wifi_up_; }

bool NetClient::rejected() const { return rejected_; }

const char* NetClient::last_error() const { return last_error_; }

bool NetClient::take_snapshot()
{
    if (mu_ == nullptr) {
        return false;
    }
    xSemaphoreTake(mu_, portMAX_DELAY);
    const bool ready = snapshot_ready_;
    snapshot_ready_ = false;
    xSemaphoreGive(mu_);
    return ready;
}

bool NetClient::pop_cmd(uint32_t* seq, artillery::PlayerId* who, artillery::ClientIntent* intent)
{
    if (mu_ == nullptr) {
        return false;
    }
    xSemaphoreTake(mu_, portMAX_DELAY);
    if (cmd_n_ <= 0) {
        xSemaphoreGive(mu_);
        return false;
    }
    const CmdEvent e = cmds_[0];
    for (int i = 1; i < cmd_n_; ++i) {
        cmds_[i - 1] = cmds_[i];
    }
    --cmd_n_;
    xSemaphoreGive(mu_);
    if (seq) {
        *seq = e.seq;
    }
    if (who) {
        *who = e.who;
    }
    if (intent) {
        *intent = e.intent;
    }
    return true;
}

int NetClient::seat() const { return seat_; }

uint32_t NetClient::seq() const { return seq_; }

const char* NetClient::ip() const { return ip_; }

const char* NetClient::status() const { return status_; }

artillery::ViewModel NetClient::view() const
{
    artillery::ViewModel v;
    if (mu_) {
        xSemaphoreTake(mu_, portMAX_DELAY);
        v = view_;
        xSemaphoreGive(mu_);
    }
    return v;
}

}  // namespace tc
