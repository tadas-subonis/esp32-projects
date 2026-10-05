#include "wire.hpp"

#include <cstdio>
#include <cstring>

namespace artillery {

uint8_t want_to_u8(const char* want)
{
    if (want == nullptr) {
        return 0;
    }
    if (want[0] == 'h') {
        return 1;
    }
    if (want[0] == 'p') {
        return 2;
    }
    return 0;
}

void want_from_u8(uint8_t v, char* out, size_t cap)
{
    const char* s = "bot";
    if (v == 1) {
        s = "hotseat";
    } else if (v == 2) {
        s = "pvp";
    }
    if (out == nullptr || cap == 0) {
        return;
    }
    std::snprintf(out, cap, "%s", s);
}

int encode_hello(uint8_t* buf, size_t cap, const WireHello& m)
{
    ByteWriter w(buf, cap);
    w.u8(m.protocol);
    w.u8(m.want);
    w.cstr(m.token, ARTILLERY_TOKEN_BYTES);
    w.u32(m.seq);
    w.u32(m.eid);
    w.u32(m.t_ms);
    return w.ok() ? static_cast<int>(w.size()) : -1;
}

int encode_welcome(uint8_t* buf, size_t cap, const WireWelcome& m)
{
    ByteWriter w(buf, cap);
    w.u8(static_cast<uint8_t>(m.seat));
    w.u8(m.want);
    w.u32(m.seq);
    w.cstr(m.token, ARTILLERY_TOKEN_BYTES);
    w.u32(m.eid);
    w.u32(m.t_ms);
    return w.ok() ? static_cast<int>(w.size()) : -1;
}

int encode_error(uint8_t* buf, size_t cap, const WireError& m)
{
    ByteWriter w(buf, cap);
    w.u8(m.cmd);
    w.cstr(m.error, ARTILLERY_ERROR_BYTES);
    w.u32(m.eid);
    w.u32(m.t_ms);
    return w.ok() ? static_cast<int>(w.size()) : -1;
}

int encode_intent(uint8_t* buf, size_t cap, const WireIntent& m)
{
    ByteWriter w(buf, cap);
    w.u8(static_cast<uint8_t>(m.intent.kind));
    w.i16(static_cast<int16_t>(m.intent.value));
    w.u32(m.intent.seed);
    return w.ok() ? static_cast<int>(w.size()) : -1;
}

int encode_cmd(uint8_t* buf, size_t cap, const WireCmd& m)
{
    ByteWriter w(buf, cap);
    w.u32(m.seq);
    w.u8(static_cast<uint8_t>(m.who));
    w.u8(static_cast<uint8_t>(m.intent.kind));
    w.i16(static_cast<int16_t>(m.intent.value));
    w.u32(m.intent.seed);
    return w.ok() ? static_cast<int>(w.size()) : -1;
}

int encode_aim(uint8_t* buf, size_t cap, const WireAim& m)
{
    ByteWriter w(buf, cap);
    w.u8(m.who);
    w.u8(m.angle);
    w.u8(m.power);
    return w.ok() ? static_cast<int>(w.size()) : -1;
}

int encode_state(uint8_t* buf, size_t cap, const WireState& m)
{
    ByteWriter w(buf, cap);
    w.u32(m.seq);
    w.u8(m.phase);
    w.u8(m.mode);
    w.u32(m.seed);
    w.u8(m.turn);
    w.u8(m.active);
    w.u8(m.winner);
    w.u8(m.flags);
    w.i16(m.angle);
    w.i16(m.power);
    w.i16(m.wind);
    w.i16(m.hp0);
    w.i16(m.hp1);
    w.i16(m.tank_x0);
    w.i16(m.tank_y0);
    w.i16(m.tank_x1);
    w.i16(m.tank_y1);
    w.u8(m.angle0);
    w.u8(m.angle1);
    w.u8(m.players);
    w.u8(m.title_sel);
    w.i32(float_to_milli(m.proj_x));
    w.i32(float_to_milli(m.proj_y));
    if (m.flags & WireStateFullHeights) {
        for (int i = 0; i < kWidth; ++i) {
            w.u16(m.heights[static_cast<size_t>(i)]);
        }
    } else if (m.flags & WireStateDeltas) {
        w.u8(m.delta_count);
        for (uint8_t i = 0; i < m.delta_count; ++i) {
            w.u16(m.deltas[i].x);
            w.u16(m.deltas[i].height);
        }
    }
    return w.ok() ? static_cast<int>(w.size()) : -1;
}

int encode_sync(uint8_t* buf, size_t cap, const WireSync& m)
{
    ByteWriter w(buf, cap);
    w.u32(m.seq);
    return w.ok() ? static_cast<int>(w.size()) : -1;
}

bool decode_hello(const uint8_t* buf, size_t len, WireHello* out)
{
    *out = WireHello{};
    ByteReader r(buf, len);
    r.u8(&out->protocol);
    r.u8(&out->want);
    r.cstr(out->token, ARTILLERY_TOKEN_BYTES);
    r.u32(&out->seq);
    r.u32(&out->eid);
    r.u32(&out->t_ms);
    return r.ok();
}

bool decode_welcome(const uint8_t* buf, size_t len, WireWelcome* out)
{
    *out = WireWelcome{};
    ByteReader r(buf, len);
    uint8_t seat = 0;
    r.u8(&seat);
    out->seat = static_cast<int8_t>(seat);
    r.u8(&out->want);
    r.u32(&out->seq);
    r.cstr(out->token, ARTILLERY_TOKEN_BYTES);
    r.u32(&out->eid);
    r.u32(&out->t_ms);
    return r.ok();
}

bool decode_error(const uint8_t* buf, size_t len, WireError* out)
{
    *out = WireError{};
    ByteReader r(buf, len);
    r.u8(&out->cmd);
    r.cstr(out->error, ARTILLERY_ERROR_BYTES);
    r.u32(&out->eid);
    r.u32(&out->t_ms);
    return r.ok();
}

bool decode_intent(const uint8_t* buf, size_t len, WireIntent* out)
{
    *out = WireIntent{};
    ByteReader r(buf, len);
    uint8_t kind = 0;
    int16_t value = 0;
    r.u8(&kind);
    r.i16(&value);
    r.u32(&out->intent.seed);
    out->intent.kind = static_cast<ClientIntent::Kind>(kind);
    out->intent.value = value;
    return r.ok();
}

bool decode_cmd(const uint8_t* buf, size_t len, WireCmd* out)
{
    *out = WireCmd{};
    ByteReader r(buf, len);
    uint8_t who = 0;
    uint8_t kind = 0;
    int16_t value = 0;
    r.u32(&out->seq);
    r.u8(&who);
    r.u8(&kind);
    r.i16(&value);
    r.u32(&out->intent.seed);
    out->who = static_cast<PlayerId>(who);
    out->intent.kind = static_cast<ClientIntent::Kind>(kind);
    out->intent.value = value;
    return r.ok();
}

bool decode_aim(const uint8_t* buf, size_t len, WireAim* out)
{
    *out = WireAim{};
    ByteReader r(buf, len);
    r.u8(&out->who);
    r.u8(&out->angle);
    r.u8(&out->power);
    return r.ok();
}

bool decode_state(const uint8_t* buf, size_t len, WireState* out)
{
    *out = WireState{};
    ByteReader r(buf, len);
    r.u32(&out->seq);
    r.u8(&out->phase);
    r.u8(&out->mode);
    r.u32(&out->seed);
    r.u8(&out->turn);
    r.u8(&out->active);
    r.u8(&out->winner);
    r.u8(&out->flags);
    r.i16(&out->angle);
    r.i16(&out->power);
    r.i16(&out->wind);
    r.i16(&out->hp0);
    r.i16(&out->hp1);
    r.i16(&out->tank_x0);
    r.i16(&out->tank_y0);
    r.i16(&out->tank_x1);
    r.i16(&out->tank_y1);
    r.u8(&out->angle0);
    r.u8(&out->angle1);
    r.u8(&out->players);
    r.u8(&out->title_sel);
    int32_t px = 0;
    int32_t py = 0;
    r.i32(&px);
    r.i32(&py);
    out->proj_x = milli_to_float(px);
    out->proj_y = milli_to_float(py);
    if (out->flags & WireStateFullHeights) {
        for (int i = 0; i < kWidth; ++i) {
            r.u16(&out->heights[static_cast<size_t>(i)]);
        }
        out->have_heights = true;
    } else if (out->flags & WireStateDeltas) {
        r.u8(&out->delta_count);
        if (out->delta_count > 64) {
            return false;
        }
        for (uint8_t i = 0; i < out->delta_count; ++i) {
            r.u16(&out->deltas[i].x);
            r.u16(&out->deltas[i].height);
        }
    }
    return r.ok();
}

bool decode_sync(const uint8_t* buf, size_t len, WireSync* out)
{
    *out = WireSync{};
    ByteReader r(buf, len);
    r.u32(&out->seq);
    return r.ok();
}

WireState state_from_view(const ViewModel& view, bool full_heights)
{
    WireState st;
    const MatchSnapshot& s = view.snap;
    st.seq = s.seq;
    st.phase = static_cast<uint8_t>(s.phase);
    st.mode = static_cast<uint8_t>(s.mode);
    st.seed = s.seed;
    st.turn = static_cast<uint8_t>(s.turn);
    st.active = static_cast<uint8_t>(s.active);
    st.winner = static_cast<uint8_t>(s.winner);
    st.flags = 0;
    if (s.have_winner) {
        st.flags |= WireStateHaveWinner;
    }
    if (view.tanks[0].alive) {
        st.flags |= WireStateAlive0;
    }
    if (view.tanks[1].alive) {
        st.flags |= WireStateAlive1;
    }
    if (s.firing) {
        st.flags |= WireStateFiring;
    }
    st.angle = static_cast<int16_t>(s.angle);
    st.power = static_cast<int16_t>(s.power);
    st.wind = static_cast<int16_t>(s.wind);
    st.hp0 = static_cast<int16_t>(s.hp[0]);
    st.hp1 = static_cast<int16_t>(s.hp[1]);
    st.tank_x0 = static_cast<int16_t>(s.tank_x[0]);
    st.tank_y0 = static_cast<int16_t>(s.tank_y[0]);
    st.tank_x1 = static_cast<int16_t>(s.tank_x[1]);
    st.tank_y1 = static_cast<int16_t>(s.tank_y[1]);
    st.angle0 = static_cast<uint8_t>(view.tank_angle[0]);
    st.angle1 = static_cast<uint8_t>(view.tank_angle[1]);
    st.players = static_cast<uint8_t>(view.players);
    st.title_sel = static_cast<uint8_t>(view.title_sel);
    st.proj_x = s.proj_x;
    st.proj_y = s.proj_y;
    if (full_heights && view.have_heights) {
        st.flags |= WireStateFullHeights;
        st.heights = view.heights;
        st.have_heights = true;
    }
    return st;
}

void apply_state_to_view(const WireState& st, ViewModel* view)
{
    if (view == nullptr) {
        return;
    }
    view->snap.seq = st.seq;
    view->snap.phase = static_cast<Phase>(st.phase);
    view->snap.mode = static_cast<Mode>(st.mode);
    view->snap.seed = st.seed == 0 ? 1 : st.seed;
    view->snap.turn = st.turn;
    view->snap.active = static_cast<PlayerId>(st.active);
    view->snap.winner = static_cast<PlayerId>(st.winner);
    view->snap.have_winner = (st.flags & WireStateHaveWinner) != 0;
    view->snap.angle = st.angle;
    view->snap.power = st.power;
    view->snap.wind = st.wind;
    view->snap.hp[0] = st.hp0;
    view->snap.hp[1] = st.hp1;
    view->snap.tank_x[0] = st.tank_x0;
    view->snap.tank_y[0] = st.tank_y0;
    view->snap.tank_x[1] = st.tank_x1;
    view->snap.tank_y[1] = st.tank_y1;
    view->snap.firing = (st.flags & WireStateFiring) != 0;
    view->snap.proj_x = st.proj_x;
    view->snap.proj_y = st.proj_y;
    view->tank_angle[0] = st.angle0;
    view->tank_angle[1] = st.angle1;
    view->players = st.players;
    view->title_sel = st.title_sel;
    view->tanks[0].alive = (st.flags & WireStateAlive0) != 0;
    view->tanks[1].alive = (st.flags & WireStateAlive1) != 0;
    view->tanks[0].hp = st.hp0;
    view->tanks[1].hp = st.hp1;
    view->tanks[0].x = static_cast<float>(st.tank_x0);
    view->tanks[0].y = static_cast<float>(st.tank_y0);
    view->tanks[1].x = static_cast<float>(st.tank_x1);
    view->tanks[1].y = static_cast<float>(st.tank_y1);
    view->proj.alive = view->snap.firing;
    view->proj.pos.x = st.proj_x;
    view->proj.pos.y = st.proj_y;
    if (st.have_heights || (st.flags & WireStateFullHeights)) {
        view->heights = st.heights;
        view->have_heights = true;
    } else if ((st.flags & WireStateDeltas) && st.delta_count > 0) {
        if (!view->have_heights) {
            view->heights.fill(0);
            view->have_heights = true;
        }
        for (uint8_t i = 0; i < st.delta_count; ++i) {
            const uint16_t x = st.deltas[i].x;
            if (x < kWidth) {
                view->heights[x] = st.deltas[i].height;
            }
        }
    }
}

void collect_height_deltas(const std::array<uint16_t, kWidth>& before,
                           const std::array<uint16_t, kWidth>& after, WireState* st)
{
    if (st == nullptr) {
        return;
    }
    st->delta_count = 0;
    for (int x = 0; x < kWidth && st->delta_count < 64; ++x) {
        if (before[static_cast<size_t>(x)] != after[static_cast<size_t>(x)]) {
            st->deltas[st->delta_count].x = static_cast<uint16_t>(x);
            st->deltas[st->delta_count].height = after[static_cast<size_t>(x)];
            ++st->delta_count;
        }
    }
    if (st->delta_count > 0) {
        st->flags |= WireStateDeltas;
    }
}

}  // namespace artillery
