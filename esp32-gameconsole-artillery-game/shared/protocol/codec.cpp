#include "codec.hpp"

#include "artillery_protocol.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace artillery {
namespace {

const char* skip_ws(const char* s)
{
    while (*s == ' ' || *s == '\t') {
        ++s;
    }
    return s;
}

const char* find_key(const char* json, const char* key)
{
    char pat[40];
    std::snprintf(pat, sizeof(pat), "\"%s\"", key);
    const size_t pat_len = std::strlen(pat);
    for (const char* p = json; (p = std::strstr(p, pat)) != nullptr; p += 1) {
        const char* after = skip_ws(p + pat_len);
        if (*after == ':') {
            return skip_ws(after + 1);
        }
    }
    return nullptr;
}

bool json_string(const char* json, const char* key, char* out, size_t cap)
{
    const char* p = find_key(json, key);
    if (p == nullptr || *p != '"') {
        return false;
    }
    ++p;
    size_t n = 0;
    while (*p && *p != '"' && n + 1 < cap) {
        out[n++] = *p++;
    }
    out[n] = 0;
    return true;
}

bool json_int(const char* json, const char* key, int* out)
{
    const char* p = find_key(json, key);
    if (p == nullptr) {
        return false;
    }
    *out = std::atoi(p);
    return true;
}

}  // namespace

bool parse_net_request(const char* line, NetRequest* out)
{
    *out = NetRequest{};
    if (!json_string(line, "cmd", out->cmd, sizeof(out->cmd))) {
        return false;
    }

    if (std::strcmp(out->cmd, "hello") == 0 || std::strcmp(out->cmd, "join") == 0) {
        out->kind = NetRequest::Hello;
        json_int(line, "protocol", &out->protocol);
        if (!json_string(line, "want", out->want, sizeof(out->want))) {
            std::snprintf(out->want, sizeof(out->want), "bot");
        }
        json_string(line, "token", out->token, sizeof(out->token));
        int eid = 0;
        if (json_int(line, "eid", &eid) && eid > 0) {
            out->eid = static_cast<uint32_t>(eid);
        }
        int seq = 0;
        if (json_int(line, "seq", &seq) && seq >= 0) {
            out->seq = static_cast<uint32_t>(seq);
        }
        return true;
    }
    if (std::strcmp(out->cmd, "sync") == 0) {
        out->kind = NetRequest::Sync;
        int seq = 0;
        if (json_int(line, "seq", &seq) && seq >= 0) {
            out->seq = static_cast<uint32_t>(seq);
        }
        return true;
    }
    if (std::strcmp(out->cmd, "status") == 0) {
        out->kind = NetRequest::Status;
        return true;
    }
    if (std::strcmp(out->cmd, "ping") == 0) {
        out->kind = NetRequest::Ping;
        return true;
    }

    out->kind = NetRequest::Intent;
    if (std::strcmp(out->cmd, "select") == 0) {
        out->intent.kind = ClientIntent::ToggleSelect;
        return true;
    }
    if (std::strcmp(out->cmd, "start") == 0) {
        out->intent.kind = ClientIntent::Start;
        char mode[16] = {0};
        json_string(line, "mode", mode, sizeof(mode));
        out->intent.value = static_cast<int>(mode_from_name(mode[0] ? mode : "bot"));
        int seed = 0;
        if (json_int(line, "seed", &seed) && seed > 0) {
            out->intent.seed = static_cast<uint32_t>(seed);
        }
        return true;
    }
    if (std::strcmp(out->cmd, "rematch") == 0) {
        out->intent.kind = ClientIntent::Rematch;
        return true;
    }
    if (std::strcmp(out->cmd, "title") == 0) {
        out->intent.kind = ClientIntent::ToTitle;
        return true;
    }
    if (std::strcmp(out->cmd, "angle") == 0) {
        out->intent.kind = ClientIntent::SetAngle;
        json_int(line, "value", &out->intent.value);
        return true;
    }
    if (std::strcmp(out->cmd, "power") == 0) {
        out->intent.kind = ClientIntent::SetPower;
        json_int(line, "value", &out->intent.value);
        return true;
    }
    if (std::strcmp(out->cmd, "nudge") == 0) {
        char axis[16] = {0};
        json_string(line, "axis", axis, sizeof(axis));
        int dir = 1;
        json_int(line, "dir", &dir);
        if (axis[0] == 'p') {
            out->intent.kind = ClientIntent::NudgePower;
        } else {
            out->intent.kind = ClientIntent::NudgeAngle;
        }
        out->intent.value = dir;
        return true;
    }
    if (std::strcmp(out->cmd, "fire") == 0) {
        out->intent.kind = ClientIntent::Fire;
        return true;
    }
    out->kind = NetRequest::Unknown;
    return false;
}

int write_welcome_json(char* buf, size_t cap, int seat, const char* want, uint32_t seq, const char* token,
                       uint32_t eid, uint32_t t_ms)
{
    return std::snprintf(
        buf, cap,
        "{\"type\":\"welcome\",\"ok\":true,\"protocol\":%d,\"seat\":%d,\"want\":\"%s\",\"seq\":%lu,"
        "\"token\":\"%s\",\"eid\":%lu,\"t_ms\":%lu}\n",
        ARTILLERY_PROTOCOL_VERSION, seat, want ? want : "bot", static_cast<unsigned long>(seq),
        token ? token : "", static_cast<unsigned long>(eid), static_cast<unsigned long>(t_ms));
}

int write_error_json(char* buf, size_t cap, const char* cmd, const char* error, uint32_t eid,
                     uint32_t t_ms)
{
    return std::snprintf(
        buf, cap,
        "{\"type\":\"error\",\"ok\":false,\"cmd\":\"%s\",\"error\":\"%s\",\"eid\":%lu,\"t_ms\":%lu}\n",
        cmd ? cmd : "", error ? error : "error", static_cast<unsigned long>(eid),
        static_cast<unsigned long>(t_ms));
}

int write_state_json(char* buf, size_t cap, const ViewModel& view, bool include_heights)
{
    const MatchSnapshot& s = view.snap;
    int n = std::snprintf(
        buf, cap,
        "{\"type\":\"state\",\"ok\":true,\"seq\":%lu,\"phase\":\"%s\",\"mode\":\"%s\",\"seed\":%lu,\"turn\":%d,"
        "\"active\":%d,\"winner\":%d,\"have_winner\":%s,\"angle\":%d,\"angle0\":%d,\"angle1\":%d,"
        "\"power\":%d,\"wind\":%d,"
        "\"hp0\":%d,\"hp1\":%d,\"tank0\":[%d,%d],\"tank1\":[%d,%d],\"alive0\":%s,\"alive1\":%s,"
        "\"firing\":%s,\"proj_x\":%.2f,\"proj_y\":%.2f,\"title_sel\":%d,\"players\":%d",
        static_cast<unsigned long>(view.snap.seq), phase_name(s.phase), mode_name(s.mode),
        static_cast<unsigned long>(s.seed), s.turn,
        static_cast<int>(s.active), static_cast<int>(s.winner), s.have_winner ? "true" : "false", s.angle,
        view.tank_angle[0], view.tank_angle[1], s.power, s.wind, s.hp[0], s.hp[1], s.tank_x[0], s.tank_y[0],
        s.tank_x[1], s.tank_y[1],
        view.tanks[0].alive ? "true" : "false", view.tanks[1].alive ? "true" : "false",
        s.firing ? "true" : "false", static_cast<double>(s.proj_x), static_cast<double>(s.proj_y),
        view.title_sel, view.players);
    if (n < 0 || static_cast<size_t>(n) >= cap) {
        return n;
    }
    if (include_heights) {
        int m = std::snprintf(buf + n, cap - static_cast<size_t>(n), ",\"heights\":[");
        if (m < 0) {
            return m;
        }
        n += m;
        for (int x = 0; x < kWidth; ++x) {
            m = std::snprintf(buf + n, cap - static_cast<size_t>(n), "%s%u", x ? "," : "",
                              static_cast<unsigned>(view.heights[static_cast<size_t>(x)]));
            if (m < 0) {
                return m;
            }
            n += m;
            if (static_cast<size_t>(n) + 8 >= cap) {
                break;
            }
        }
        m = std::snprintf(buf + n, cap - static_cast<size_t>(n), "]");
        if (m < 0) {
            return m;
        }
        n += m;
    }
    int m = std::snprintf(buf + n, cap - static_cast<size_t>(n), "}\n");
    if (m < 0) {
        return m;
    }
    return n + m;
}

int write_intent_json(char* buf, size_t cap, const ClientIntent& intent)
{
    switch (intent.kind) {
        case ClientIntent::ToggleSelect:
            return std::snprintf(buf, cap, "{\"cmd\":\"select\"}\n");
        case ClientIntent::Start:
            if (intent.seed > 0) {
                return std::snprintf(buf, cap, "{\"cmd\":\"start\",\"mode\":\"%s\",\"seed\":%u}\n",
                                     mode_name(static_cast<Mode>(intent.value)),
                                     static_cast<unsigned>(intent.seed));
            }
            return std::snprintf(buf, cap, "{\"cmd\":\"start\",\"mode\":\"%s\"}\n",
                                 mode_name(static_cast<Mode>(intent.value)));
        case ClientIntent::Rematch:
            return std::snprintf(buf, cap, "{\"cmd\":\"rematch\"}\n");
        case ClientIntent::ToTitle:
            return std::snprintf(buf, cap, "{\"cmd\":\"title\"}\n");
        case ClientIntent::SetAngle:
            return std::snprintf(buf, cap, "{\"cmd\":\"angle\",\"value\":%d}\n", intent.value);
        case ClientIntent::SetPower:
            return std::snprintf(buf, cap, "{\"cmd\":\"power\",\"value\":%d}\n", intent.value);
        case ClientIntent::NudgeAngle:
            return std::snprintf(buf, cap, "{\"cmd\":\"nudge\",\"axis\":\"angle\",\"dir\":%d}\n",
                                 intent.value);
        case ClientIntent::NudgePower:
            return std::snprintf(buf, cap, "{\"cmd\":\"nudge\",\"axis\":\"power\",\"dir\":%d}\n",
                                 intent.value);
        case ClientIntent::Fire:
            return std::snprintf(buf, cap, "{\"cmd\":\"fire\"}\n");
        default:
            return std::snprintf(buf, cap, "{\"cmd\":\"unknown\"}\n");
    }
}

int write_cmd_json(char* buf, size_t cap, uint32_t seq, PlayerId who, const ClientIntent& intent)
{
    char inner[192];
    write_intent_json(inner, sizeof(inner), intent);
    int n = static_cast<int>(std::strlen(inner));
    if (n > 0 && inner[n - 1] == '\n') {
        inner[n - 1] = 0;
        if (n > 1 && inner[n - 2] == '\r') {
            inner[n - 2] = 0;
        }
    }
    char fields[192];
    const char* brace = std::strchr(inner, '{');
    std::snprintf(fields, sizeof(fields), "%s", brace ? brace + 1 : inner);
    n = static_cast<int>(std::strlen(fields));
    if (n > 0 && fields[n - 1] == '}') {
        fields[n - 1] = 0;
    }
    return std::snprintf(buf, cap, "{\"type\":\"cmd\",\"seq\":%lu,\"who\":%d,%s}\n",
                         static_cast<unsigned long>(seq), static_cast<int>(who), fields);
}

Phase phase_from_name(const char* s)
{
    if (s == nullptr) {
        return Phase::Title;
    }
    if (std::strcmp(s, "aiming") == 0) {
        return Phase::Aiming;
    }
    if (std::strcmp(s, "firing") == 0) {
        return Phase::Firing;
    }
    if (std::strcmp(s, "resolving") == 0) {
        return Phase::Resolving;
    }
    if (std::strcmp(s, "gameover") == 0) {
        return Phase::GameOver;
    }
    return Phase::Title;
}

bool parse_state_json(const char* line, ViewModel* view)
{
    if (view == nullptr || line == nullptr) {
        return false;
    }
    char type[16] = {0};
    json_string(line, "type", type, sizeof(type));
    if (type[0] && std::strcmp(type, "state") != 0) {
        return false;
    }
    char phase[16] = {0};
    char mode[16] = {0};
    json_string(line, "phase", phase, sizeof(phase));
    json_string(line, "mode", mode, sizeof(mode));
    view->snap.phase = phase_from_name(phase);
    view->snap.mode = mode_from_name(mode);
    int v = 0;
    if (json_int(line, "seq", &v) && v >= 0) {
        view->snap.seq = static_cast<uint32_t>(v);
    }
    if (json_int(line, "seed", &v)) {
        view->snap.seed = static_cast<uint32_t>(v);
    }
    json_int(line, "turn", &view->snap.turn);
    if (json_int(line, "active", &v)) {
        view->snap.active = static_cast<PlayerId>(v);
    }
    if (json_int(line, "winner", &v)) {
        view->snap.winner = static_cast<PlayerId>(v);
    }
    const char* have_w = std::strstr(line, "\"have_winner\":");
    view->snap.have_winner = have_w && std::strstr(have_w, "true") == have_w + 14;
    if (view->snap.phase == Phase::GameOver) {
        view->snap.have_winner = true;
    }
    json_int(line, "angle", &view->snap.angle);
    json_int(line, "power", &view->snap.power);
    if (!json_int(line, "angle0", &view->tank_angle[0])) {
        view->tank_angle[0] =
            view->snap.active == PlayerId::P0 ? view->snap.angle : view->tank_angle[0];
    }
    if (!json_int(line, "angle1", &view->tank_angle[1])) {
        view->tank_angle[1] =
            view->snap.active == PlayerId::P1 ? view->snap.angle : view->tank_angle[1];
    }
    json_int(line, "wind", &view->snap.wind);
    json_int(line, "hp0", &view->snap.hp[0]);
    json_int(line, "hp1", &view->snap.hp[1]);
    view->tanks[0].hp = view->snap.hp[0];
    view->tanks[1].hp = view->snap.hp[1];
    json_int(line, "title_sel", &view->title_sel);
    json_int(line, "players", &view->players);

    const char* t0 = std::strstr(line, "\"tank0\":[");
    if (t0) {
        std::sscanf(t0, "\"tank0\":[%d,%d]", &view->snap.tank_x[0], &view->snap.tank_y[0]);
    }
    const char* t1 = std::strstr(line, "\"tank1\":[");
    if (t1) {
        std::sscanf(t1, "\"tank1\":[%d,%d]", &view->snap.tank_x[1], &view->snap.tank_y[1]);
    }
    view->tanks[0].x = static_cast<float>(view->snap.tank_x[0]);
    view->tanks[0].y = static_cast<float>(view->snap.tank_y[0]);
    view->tanks[1].x = static_cast<float>(view->snap.tank_x[1]);
    view->tanks[1].y = static_cast<float>(view->snap.tank_y[1]);
    view->tanks[0].alive = view->tanks[0].hp > 0;
    view->tanks[1].alive = view->tanks[1].hp > 0;

    const char* firing = std::strstr(line, "\"firing\":");
    view->snap.firing = firing && std::strstr(firing, "true") == firing + 9;
    view->proj.alive = view->snap.firing;
    const char* px = find_key(line, "proj_x");
    const char* py = find_key(line, "proj_y");
    if (px) {
        view->snap.proj_x = std::atof(px);
        view->proj.pos.x = view->snap.proj_x;
    }
    if (py) {
        view->snap.proj_y = std::atof(py);
        view->proj.pos.y = view->snap.proj_y;
    }

    const char* hs = std::strstr(line, "\"heights\":[");
    if (hs) {
        view->have_heights = true;
        hs = std::strchr(hs, '[');
        if (hs) {
            ++hs;
            for (int x = 0; x < kWidth; ++x) {
                view->heights[static_cast<size_t>(x)] = static_cast<uint16_t>(std::atoi(hs));
                const char* comma = std::strchr(hs, ',');
                const char* end = std::strchr(hs, ']');
                if (comma == nullptr || (end && end < comma)) {
                    break;
                }
                hs = comma + 1;
            }
        }
    }
    return true;
}

bool parse_welcome_json(const char* line, int* seat, uint32_t* seq, char* token, size_t token_cap)
{
    if (line == nullptr || seat == nullptr) {
        return false;
    }
    char type[16] = {0};
    json_string(line, "type", type, sizeof(type));
    if (std::strcmp(type, "welcome") != 0) {
        return false;
    }
    if (!json_int(line, "seat", seat)) {
        return false;
    }
    int s = 0;
    if (seq != nullptr && json_int(line, "seq", &s) && s >= 0) {
        *seq = static_cast<uint32_t>(s);
    }
    if (token != nullptr && token_cap > 0) {
        if (!json_string(line, "token", token, token_cap)) {
            token[0] = 0;
        }
    }
    return true;
}

bool parse_error_json(const char* line, char* cmd, size_t cmd_cap, char* error, size_t error_cap)
{
    if (line == nullptr) {
        return false;
    }
    char type[16] = {0};
    json_string(line, "type", type, sizeof(type));
    if (std::strcmp(type, "error") != 0) {
        // Also accept ok:false lines that carry an error field.
        if (std::strstr(line, "\"ok\":false") == nullptr) {
            return false;
        }
    }
    if (error == nullptr || error_cap == 0) {
        return false;
    }
    if (!json_string(line, "error", error, error_cap) || error[0] == 0) {
        return false;
    }
    if (cmd != nullptr && cmd_cap > 0) {
        if (!json_string(line, "cmd", cmd, cmd_cap)) {
            cmd[0] = 0;
        }
    }
    return true;
}

bool parse_cmd_json(const char* line, uint32_t* seq, PlayerId* who, ClientIntent* intent)
{
    if (line == nullptr || intent == nullptr) {
        return false;
    }
    char type[16] = {0};
    json_string(line, "type", type, sizeof(type));
    if (std::strcmp(type, "cmd") != 0) {
        return false;
    }
    int v = 0;
    if (seq != nullptr && json_int(line, "seq", &v) && v >= 0) {
        *seq = static_cast<uint32_t>(v);
    }
    if (who != nullptr && json_int(line, "who", &v)) {
        *who = static_cast<PlayerId>(v);
    }
    NetRequest req;
    if (!parse_net_request(line, &req) || req.kind != NetRequest::Intent) {
        return false;
    }
    *intent = req.intent;
    return true;
}

}  // namespace artillery
