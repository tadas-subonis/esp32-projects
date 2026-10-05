# Architecture

## Why this shape

The handheld ships **native ESP-IDF games**, not a VM. The photo-frame repo split firmware from a shared wire contract. This repo does the same, with a portable `game/` library and a **native C++ server** so PC play (and later the handheld) never lets the client decide hits.

```text
  artillery-host
        |
   ServerProxy          ← local: Authority in this process
        |                 remote: binary UDP v2 (Link + packed messages)
        v
    Authority           ← join / submit / tick  (same code as artillery-server)
        |
     Match              ← apply() validates, tick() simulates
```

`game/` has no sockets. Parse the wire only on the UDP boundary (`shared/protocol/link.cpp` + `wire.cpp`). Local play calls `Authority::submit` in the same binary and address space.

Accepted intents append to a command log (`seq`). Join/resync send a binary `State` with full heights; impacts send crater column deltas. Aim is an unreliable absolute sample. Reliable messages are repeated in later datagrams until acked (Gaffer-style), so a lost Fire does not wait on TCP’s retransmit timer.

## Display strategy (console plan §7)

A full RGB565 frame is 307,200 bytes. SPI at 40 MHz is still under 15 fps full-screen. Artillery only needs:

- one full compose when the map or title changes
- HUD + tank rects when angle/power change
- the debug overlay (and a tiny projectile rect while the shell is in the air)

`present_frame` keeps a clean scene buffer and a display frame. SPI uploads only the dirty list. `artillery-emu` uses the same present path (SDL still uploads the whole texture). How we hit ~60 fps idle, and how to measure it: [fps.md](fps.md).

Do not replace the device path with SDL or a logical 320×240 PPA scale. That advice targets RGB/MIPI boards; see [game-stack.md](game-stack.md).

## Who simulates

| Surface | Who owns `Match` |
|---------|------------------|
| `Authority` | Always. Local proxy and UDP server both call it. |
| `artillery-host --local` | Nobody. Same process; `ServerProxy` calls `Authority` directly (default vs-bot). |
| `artillery-host --remote` | Nobody. Binary UDP intents to `artillery-server`. |
| `artillery-sim` | Direct physics CLI (no live match). Golden tests. |
| `artillery-emu` | Local `Match`, or UDP client with `--remote --want pvp` (same lobby + replica as firmware). |
| Firmware today | Local `Match` by default. Choosing **HOTSEAT** enables Wi-Fi and joins `artillery-server`; **VS BOT** stays offline. |

Modes on the UDP server: `bot` (one client = P0, server bot = P1), `hotseat` (one client acts as the active seat), `pvp` (two seats; start needs two hellos). Hello broadcasts a `State` with `players` so the first board can leave **WAITING FOR OPPONENT** when the second joins.

Default port **7420** (UDP). Contract: [`shared/protocol/artillery_protocol.h`](../shared/protocol/artillery_protocol.h).

### Join path + correlatable logs

Firmware `NetClient` opens a connected UDP socket and repeats `Hello` until `Welcome` (budget ~2.5 s). Soft reconnect backoff starts at 200 ms (cap 2 s). Hard rejects (`server_full`, `protocol_mismatch`) back off longer. Peers silent for 2 s are dropped.

Every join gets an `eid`. Hello carries `eid` + client boot `t_ms`; welcome/error echo them. UART (`I (….) net: eid=… t_ms=…`) and server stderr (`[server] eid=… t_ms=…`) line up for the same handshake.

### ESP-Hosted (P4 ↔ C6) footguns

Wi-Fi is ESP32-C6 over SDIO. Do **not** call `esp_wifi_*` from the `sys_evt` task or from the game task — hosted RPC overflows those stacks. `NetClient::disable` only sets a flag; the `net` task runs `esp_wifi_disconnect`. STA_START / DISCONNECTED handlers only set event-group bits.

## Hardware mapping

See [hardware.md](hardware.md). Prototype: Waveshare ESP32-P4-WIFI6-POE-ETH + 3.5" SPI **ILI9488**, eight tactile buttons. Firmware HAL matches that pinout.
