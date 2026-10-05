#include "link.hpp"

#include "bytes.hpp"

#include <cstring>

namespace artillery {

void Link::reset()
{
    *this = Link{};
}

bool Link::seq_greater(uint16_t a, uint16_t b)
{
    return static_cast<uint16_t>(a - b) < 32768 && a != b;
}

int Link::seq_diff(uint16_t a, uint16_t b)
{
    return static_cast<int16_t>(a - b);
}

bool Link::send_reliable(uint8_t type, const uint8_t* data, uint16_t len)
{
    if (len > kMaxPayload || send_n_ >= kSendQueue) {
        return false;
    }
    Pending& p = send_q_[send_n_++];
    p.id = next_msg_id_++;
    if (next_msg_id_ == 0) {
        next_msg_id_ = 1;
    }
    p.type = type;
    p.len = len;
    p.acked = false;
    if (len > 0 && data != nullptr) {
        std::memcpy(p.data, data, len);
    }
    return true;
}

void Link::send_unreliable(uint8_t type, const uint8_t* data, uint16_t len)
{
    if (len > kMaxPayload) {
        return;
    }
    have_unreliable_ = true;
    unreliable_type_ = type;
    unreliable_len_ = len;
    if (len > 0 && data != nullptr) {
        std::memcpy(unreliable_data_, data, len);
    }
}

int Link::pending_reliable() const
{
    int n = 0;
    for (int i = 0; i < send_n_; ++i) {
        if (!send_q_[i].acked) {
            ++n;
        }
    }
    return n;
}

bool Link::peer_alive(uint32_t now_ms) const
{
    if (!have_remote_ || last_recv_ms_ == 0) {
        return false;
    }
    return now_ms - last_recv_ms_ <= kPeerTimeoutMs;
}

void Link::note_recv_seq(uint16_t seq)
{
    const int idx = seq % kSeqBuf;
    recv_seen_[idx] = true;
    recv_seq_at_[idx] = seq;
    if (!have_remote_ || seq_greater(seq, remote_seq_)) {
        remote_seq_ = seq;
        have_remote_ = true;
    }
}

uint32_t Link::build_ack_bits() const
{
    if (!have_remote_) {
        return 0;
    }
    uint32_t bits = 0;
    for (int i = 0; i < 32; ++i) {
        const uint16_t s = static_cast<uint16_t>(remote_seq_ - 1 - i);
        const int idx = s % kSeqBuf;
        if (recv_seen_[idx] && recv_seq_at_[idx] == s) {
            bits |= (1u << i);
        }
    }
    return bits;
}

void Link::process_acks(uint16_t ack, uint32_t ack_bits)
{
    auto ack_one = [&](uint16_t pkt) {
        const int idx = pkt % kSeqBuf;
        PacketRecord& rec = sent_[idx];
        if (!rec.used || rec.seq != pkt) {
            return;
        }
        for (uint8_t i = 0; i < rec.msg_n; ++i) {
            const uint16_t mid = rec.msg_ids[i];
            for (int j = 0; j < send_n_; ++j) {
                if (send_q_[j].id == mid) {
                    send_q_[j].acked = true;
                }
            }
        }
        rec.used = false;
    };

    ack_one(ack);
    for (int i = 0; i < 32; ++i) {
        if (ack_bits & (1u << i)) {
            ack_one(static_cast<uint16_t>(ack - 1 - i));
        }
    }

    int w = 0;
    for (int i = 0; i < send_n_; ++i) {
        if (!send_q_[i].acked) {
            if (w != i) {
                send_q_[w] = send_q_[i];
            }
            ++w;
        }
    }
    send_n_ = w;
}

bool Link::enqueue_deliver(const Message& m)
{
    if (deliver_n_ >= kDeliverQueue) {
        return false;
    }
    deliver_[deliver_n_++] = m;
    return true;
}

void Link::deliver_ready()
{
    for (;;) {
        bool found = false;
        for (int i = 0; i < kRecvQueue; ++i) {
            RecvSlot& s = recv_buf_[i];
            if (!s.present || s.id != next_deliver_id_) {
                continue;
            }
            Message m;
            m.type = s.type;
            m.id = s.id;
            m.len = s.len;
            if (s.len > 0) {
                std::memcpy(m.data, s.data, s.len);
            }
            if (!enqueue_deliver(m)) {
                return;
            }
            s.present = false;
            ++next_deliver_id_;
            if (next_deliver_id_ == 0) {
                next_deliver_id_ = 1;
            }
            found = true;
            break;
        }
        if (!found) {
            break;
        }
    }
}

void Link::pump_in(const uint8_t* data, int len)
{
    if (data == nullptr || len < ARTILLERY_UDP_HEADER_SIZE) {
        return;
    }
    ByteReader r(data, static_cast<size_t>(len));
    uint8_t magic = 0;
    uint8_t version = 0;
    uint16_t seq = 0;
    uint16_t ack = 0;
    uint32_t ack_bits = 0;
    uint8_t msg_count = 0;
    if (!r.u8(&magic) || magic != ARTILLERY_UDP_MAGIC) {
        return;
    }
    if (!r.u8(&version) || version != ARTILLERY_PROTOCOL_VERSION) {
        return;
    }
    if (!r.u16(&seq) || !r.u16(&ack) || !r.u32(&ack_bits) || !r.u8(&msg_count)) {
        return;
    }

    note_recv_seq(seq);
    process_acks(ack, ack_bits);

    for (uint8_t i = 0; i < msg_count; ++i) {
        uint8_t type = 0;
        uint16_t id = 0;
        uint16_t mlen = 0;
        if (!r.u8(&type) || !r.u16(&id) || !r.u16(&mlen)) {
            return;
        }
        if (mlen > kMaxPayload || r.remaining() < mlen) {
            return;
        }
        const uint8_t* payload = r.ptr();
        r.skip(mlen);

        if (id == 0) {
            Message m;
            m.type = type;
            m.id = 0;
            m.len = mlen;
            if (mlen > 0) {
                std::memcpy(m.data, payload, mlen);
            }
            enqueue_deliver(m);
            continue;
        }

        // Already delivered when id is behind next_deliver_id_ (wrap-aware).
        if (id != next_deliver_id_ && !seq_greater(id, next_deliver_id_)) {
            continue;
        }

        int slot = -1;
        for (int j = 0; j < kRecvQueue; ++j) {
            if (recv_buf_[j].present && recv_buf_[j].id == id) {
                slot = -2;  // duplicate
                break;
            }
            if (!recv_buf_[j].present && slot < 0) {
                slot = j;
            }
        }
        if (slot == -2) {
            continue;
        }
        if (slot < 0) {
            continue;  // buffer full
        }
        RecvSlot& s = recv_buf_[slot];
        s.present = true;
        s.id = id;
        s.type = type;
        s.len = mlen;
        if (mlen > 0) {
            std::memcpy(s.data, payload, mlen);
        }
    }

    deliver_ready();
}

int Link::compose_out(uint8_t* buf, int cap, uint32_t now_ms, bool force)
{
    if (buf == nullptr || cap < ARTILLERY_UDP_HEADER_SIZE) {
        return 0;
    }

    const bool due = last_send_ms_ == 0 || now_ms - last_send_ms_ >= kHeartbeatMs;
    const bool need = force || due || send_n_ > 0 || have_unreliable_;
    if (!need) {
        return 0;
    }

    const uint16_t pkt_seq = ++local_seq_;
    ByteWriter w(buf, static_cast<size_t>(cap));
    w.u8(ARTILLERY_UDP_MAGIC);
    w.u8(ARTILLERY_PROTOCOL_VERSION);
    w.u16(pkt_seq);
    w.u16(have_remote_ ? remote_seq_ : 0);
    w.u32(build_ack_bits());

    // placeholder for msg_count
    const size_t count_at = w.size();
    w.u8(0);
    if (!w.ok()) {
        return 0;
    }

    uint8_t count = 0;
    PacketRecord rec{};
    rec.seq = pkt_seq;
    rec.used = true;

    auto pack = [&](uint8_t type, uint16_t id, const uint8_t* data, uint16_t len) -> bool {
        if (w.remaining() < static_cast<size_t>(5 + len)) {
            return false;
        }
        const size_t before = w.size();
        w.u8(type);
        w.u16(id);
        w.u16(len);
        w.bytes(data, len);
        if (!w.ok()) {
            return false;
        }
        (void)before;
        ++count;
        if (id != 0 && rec.msg_n < 16) {
            rec.msg_ids[rec.msg_n++] = id;
        }
        return true;
    };

    for (int i = 0; i < send_n_; ++i) {
        const Pending& p = send_q_[i];
        if (p.acked) {
            continue;
        }
        if (!pack(p.type, p.id, p.data, p.len)) {
            break;
        }
    }

    if (have_unreliable_) {
        if (pack(unreliable_type_, 0, unreliable_data_, unreliable_len_)) {
            have_unreliable_ = false;
        }
    }

    buf[count_at] = count;
    sent_[pkt_seq % kSeqBuf] = rec;
    last_send_ms_ = now_ms;
    return static_cast<int>(w.size());
}

bool Link::pop_message(Message* out)
{
    if (out == nullptr || deliver_n_ <= 0) {
        return false;
    }
    *out = deliver_[0];
    for (int i = 1; i < deliver_n_; ++i) {
        deliver_[i - 1] = deliver_[i];
    }
    --deliver_n_;
    return true;
}

}  // namespace artillery
