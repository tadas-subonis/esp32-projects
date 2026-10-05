#include "link.hpp"
#include "wire.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int g_fails = 0;

#define CHECK(cond)                                                                                  \
    do {                                                                                             \
        if (!(cond)) {                                                                               \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                      \
            ++g_fails;                                                                               \
        }                                                                                            \
    } while (0)

void exchange(artillery::Link& a, artillery::Link& b, uint32_t t, bool drop_a_to_b = false)
{
    uint8_t buf[ARTILLERY_UDP_MAX_PACKET];
    const int n = a.compose_out(buf, sizeof(buf), t, true);
    if (n > 0 && !drop_a_to_b) {
        b.pump_in(buf, n);
    }
    const int m = b.compose_out(buf, sizeof(buf), t, true);
    if (m > 0) {
        a.pump_in(buf, m);
    }
}

void test_bytes_roundtrip()
{
    uint8_t buf[64];
    artillery::ByteWriter w(buf, sizeof(buf));
    w.u8(0xA7);
    w.u16(0x1234);
    w.u32(0x89ABCDEF);
    w.i16(-7);
    w.cstr("pvp", 8);
    w.cstr("00000001", 8);
    CHECK(w.ok());

    artillery::ByteReader r(buf, w.size());
    uint8_t magic = 0;
    uint16_t u16 = 0;
    uint32_t u32 = 0;
    int16_t i16 = 0;
    char s[9] = {};
    char tok[9] = {};
    CHECK(r.u8(&magic) && magic == 0xA7);
    CHECK(r.u16(&u16) && u16 == 0x1234);
    CHECK(r.u32(&u32) && u32 == 0x89ABCDEF);
    CHECK(r.i16(&i16) && i16 == -7);
    CHECK(r.cstr(s, 8) && std::strcmp(s, "pvp") == 0);
    CHECK(r.cstr(tok, 8) && std::strcmp(tok, "00000001") == 0);
}

void test_wire_aim_roundtrip()
{
    artillery::WireAim in{1, 55, 80};
    uint8_t buf[16];
    const int n = artillery::encode_aim(buf, sizeof(buf), in);
    CHECK(n > 0);
    artillery::WireAim out{};
    CHECK(artillery::decode_aim(buf, static_cast<size_t>(n), &out));
    CHECK(out.who == 1 && out.angle == 55 && out.power == 80);
}

void test_reliable_survives_drop()
{
    artillery::Link a;
    artillery::Link b;
    uint8_t fire[] = {9};
    CHECK(a.send_reliable(static_cast<uint8_t>(artillery::WireMsg::Intent), fire, 1));

    uint8_t buf[ARTILLERY_UDP_MAX_PACKET];
    const int n1 = a.compose_out(buf, sizeof(buf), 100, true);
    CHECK(n1 > 0);
    // Drop first datagram that carried Fire.
    const int n2 = a.compose_out(buf, sizeof(buf), 150, true);
    CHECK(n2 > 0);
    b.pump_in(buf, n2);

    artillery::Link::Message m;
    CHECK(b.pop_message(&m));
    CHECK(m.type == static_cast<uint8_t>(artillery::WireMsg::Intent));
    CHECK(m.len == 1 && m.data[0] == 9);

    // Ack path clears sender queue.
    exchange(b, a, 200, false);
    CHECK(a.pending_reliable() == 0);
}

void test_ordered_delivery_under_loss()
{
    artillery::Link a;
    artillery::Link b;
    for (uint8_t i = 1; i <= 5; ++i) {
        uint8_t payload[] = {i};
        CHECK(a.send_reliable(10, payload, 1));
    }

    // Drop the first two composes entirely.
    uint8_t buf[ARTILLERY_UDP_MAX_PACKET];
    a.compose_out(buf, sizeof(buf), 10, true);
    a.compose_out(buf, sizeof(buf), 20, true);
    const int n = a.compose_out(buf, sizeof(buf), 30, true);
    CHECK(n > 0);
    b.pump_in(buf, n);

    std::vector<uint8_t> got;
    artillery::Link::Message m;
    while (b.pop_message(&m)) {
        CHECK(m.len == 1);
        got.push_back(m.data[0]);
    }
    CHECK(got.size() == 5);
    for (size_t i = 0; i < got.size(); ++i) {
        CHECK(got[i] == static_cast<uint8_t>(i + 1));
    }
}

void test_unreliable_latest_wins()
{
    artillery::Link a;
    artillery::Link b;
    uint8_t a1[] = {10};
    uint8_t a2[] = {99};
    a.send_unreliable(6, a1, 1);
    a.send_unreliable(6, a2, 1);
    uint8_t buf[ARTILLERY_UDP_MAX_PACKET];
    const int n = a.compose_out(buf, sizeof(buf), 1, true);
    CHECK(n > 0);
    b.pump_in(buf, n);
    artillery::Link::Message m;
    CHECK(b.pop_message(&m));
    CHECK(m.type == 6 && m.id == 0 && m.data[0] == 99);
    CHECK(!b.pop_message(&m));
}

void test_state_full_heights_fits()
{
    artillery::ViewModel v;
    v.have_heights = true;
    for (int i = 0; i < artillery::kWidth; ++i) {
        v.heights[static_cast<size_t>(i)] = static_cast<uint16_t>(100 + (i % 50));
    }
    v.snap.seq = 3;
    v.snap.phase = artillery::Phase::Aiming;
    artillery::WireState st = artillery::state_from_view(v, true);
    uint8_t payload[1100];
    const int plen = artillery::encode_state(payload, sizeof(payload), st);
    CHECK(plen > 0);
    CHECK(plen + ARTILLERY_UDP_HEADER_SIZE + 5 < ARTILLERY_UDP_MAX_PACKET);

    artillery::WireState out{};
    CHECK(artillery::decode_state(payload, static_cast<size_t>(plen), &out));
    CHECK(out.have_heights);
    CHECK(out.heights[0] == 100);
    CHECK(out.heights[479] == static_cast<uint16_t>(100 + (479 % 50)));
}

}  // namespace

int main()
{
    test_bytes_roundtrip();
    test_wire_aim_roundtrip();
    test_reliable_survives_drop();
    test_ordered_delivery_under_loss();
    test_unreliable_latest_wins();
    test_state_full_heights_fits();
    if (g_fails) {
        std::fprintf(stderr, "%d link checks failed\n", g_fails);
        return 1;
    }
    std::puts("link ok");
    return 0;
}
