#pragma once

#include "artillery_protocol.h"

#include <cstdint>

namespace artillery {

/**
 * Socket-free UDP session: packet seq + ack bitfield + reliable message
 * repetition (Gaffer-style). Compose never resends a packet seq; unacked
 * messages are packed into later datagrams.
 */
class Link {
public:
    static constexpr int kMaxPayload = 1024;
    static constexpr int kSendQueue = 48;
    static constexpr int kRecvQueue = 32;
    static constexpr int kDeliverQueue = 32;
    static constexpr int kSeqBuf = 1024;
    static constexpr uint32_t kHeartbeatMs = 50;
    static constexpr uint32_t kPeerTimeoutMs = 2000;

    struct Message {
        uint8_t type = 0;
        uint16_t id = 0;
        uint16_t len = 0;
        uint8_t data[kMaxPayload]{};
    };

    void reset();

    /** Queue a reliable message. Returns false if the send queue is full. */
    bool send_reliable(uint8_t type, const uint8_t* data, uint16_t len);

    /** Latest-wins unreliable sample for this type (id=0 on the wire). */
    void send_unreliable(uint8_t type, const uint8_t* data, uint16_t len);

    /** Ingest one datagram from the peer. */
    void pump_in(const uint8_t* data, int len);

    /**
     * Build the next outbound datagram. Returns bytes written, or 0 if idle.
     * force=true always emits at least a header (ack carrier / heartbeat).
     */
    int compose_out(uint8_t* buf, int cap, uint32_t now_ms, bool force = false);

    bool pop_message(Message* out);

    uint16_t local_seq() const { return local_seq_; }
    uint16_t remote_seq() const { return remote_seq_; }
    bool peer_alive(uint32_t now_ms) const;
    uint32_t last_recv_ms() const { return last_recv_ms_; }
    int pending_reliable() const;

private:
    struct Pending {
        uint16_t id = 0;
        uint8_t type = 0;
        uint16_t len = 0;
        uint8_t data[kMaxPayload]{};
        bool acked = false;
    };

    struct PacketRecord {
        uint16_t seq = 0;
        uint16_t msg_ids[16]{};
        uint8_t msg_n = 0;
        bool used = false;
    };

    struct RecvSlot {
        bool present = false;
        uint16_t id = 0;
        uint8_t type = 0;
        uint16_t len = 0;
        uint8_t data[kMaxPayload]{};
    };

    static bool seq_greater(uint16_t a, uint16_t b);
    static int seq_diff(uint16_t a, uint16_t b);
    void note_recv_seq(uint16_t seq);
    uint32_t build_ack_bits() const;
    void process_acks(uint16_t ack, uint32_t ack_bits);
    void deliver_ready();
    bool enqueue_deliver(const Message& m);

    uint16_t local_seq_ = 0;
    uint16_t remote_seq_ = 0;
    uint16_t next_msg_id_ = 1;
    uint16_t next_deliver_id_ = 1;
    uint32_t last_recv_ms_ = 0;
    uint32_t last_send_ms_ = 0;
    bool have_remote_ = false;

    Pending send_q_[kSendQueue]{};
    int send_n_ = 0;

    bool have_unreliable_ = false;
    uint8_t unreliable_type_ = 0;
    uint16_t unreliable_len_ = 0;
    uint8_t unreliable_data_[kMaxPayload]{};

    PacketRecord sent_[kSeqBuf]{};
    bool recv_seen_[kSeqBuf]{};
    uint16_t recv_seq_at_[kSeqBuf]{};

    RecvSlot recv_buf_[kRecvQueue]{};
    Message deliver_[kDeliverQueue]{};
    int deliver_n_ = 0;
};

}  // namespace artillery
