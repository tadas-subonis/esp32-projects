# Shared protocol for Tank Duel.

Games are native ESP-IDF binaries. This header is the **wire contract** between:

- handheld firmware (`firmware/`) — serial `<<< {json}` for agents; **UDP v2** for online play
- `artillery-server` / `artillery-host` / `artillery-cli` — binary UDP (protocol 2)
- `artillery-sim` — golden physics CLI (not the live server)

JSON codecs in `codec.cpp` remain for logs and in-process tooling. The live wire is packed little-endian UDP (`wire.cpp` + `link.cpp`).

## Authoritative shot

The client never reports “I hit”. It reports intent (`Intent` / `Aim`). `artillery-server` runs `Match::apply` + physics and broadcasts `Cmd` / `State`. Default UDP port **7420** (`ARTILLERY_DEFAULT_PORT`).

Reliability is application-level: each datagram has a new sequence number, an ack + 32-bit ack bitfield, and unacked reliable messages are **repeated** in later packets (no TCP retransmit timer). Aim is unreliable (latest-wins absolute angle/power). `State` carries a full height baseline on join/resync, or crater column deltas after impact.

See `artillery_protocol.h` and `wire.hpp` for message layouts.

## Serial agent contract

Same as the photo-frame devices: send a newline-terminated command, read logs, then one result line:

```text
<<< {"cmd":"status","ok":true,...}
```

Device commands: `status`, `version`, `state`, `new [seed]`, `angle <n>`, `power <n>`, `fire`, `tick [frames]`, `snap`.
