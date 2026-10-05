#pragma once

#include "artillery/command.hpp"
#include "artillery/match.hpp"

#include <cstddef>
#include <cstdint>

namespace artillery {

struct NetRequest {
    enum Kind : uint8_t { Hello, Intent, Status, Ping, Sync, Unknown };

    Kind kind = Unknown;
    ClientIntent intent{};
    int protocol = 0;
    uint32_t seq = 0;
    char want[16]{};
    char cmd[24]{};
    char token[12]{};
    uint32_t eid = 0;
};

bool parse_net_request(const char* line, NetRequest* out);

/** Write a state/error/welcome JSON line (includes trailing newline). Returns bytes written. */
int write_welcome_json(char* buf, size_t cap, int seat, const char* want, uint32_t seq,
                       const char* token, uint32_t eid = 0, uint32_t t_ms = 0);
int write_error_json(char* buf, size_t cap, const char* cmd, const char* error, uint32_t eid = 0,
                     uint32_t t_ms = 0);
int write_state_json(char* buf, size_t cap, const ViewModel& view, bool include_heights);
int write_intent_json(char* buf, size_t cap, const ClientIntent& intent);
int write_cmd_json(char* buf, size_t cap, uint32_t seq, PlayerId who, const ClientIntent& intent);
bool parse_state_json(const char* line, ViewModel* view);
bool parse_welcome_json(const char* line, int* seat, uint32_t* seq, char* token, size_t token_cap);
bool parse_error_json(const char* line, char* cmd, size_t cmd_cap, char* error, size_t error_cap);
bool parse_cmd_json(const char* line, uint32_t* seq, PlayerId* who, ClientIntent* intent);

}  // namespace artillery
