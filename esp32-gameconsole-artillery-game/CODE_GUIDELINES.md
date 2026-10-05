# Code guidelines

Summarized from internal conventions and the photo-frame monorepo. Rule language: **MUST** / **SHOULD** / **MAY**.

## Project-defining rules

- **`game/` is the domain.** Physics, terrain, match phases, and bot aiming MUST live here. Firmware, host, and server only adapt I/O.
- **Parse, don’t validate** — at the serial/network boundary, parse raw JSON into a `ClientIntent` / `Shot`. Failing the parse *is* validation. `Match::apply` assumes a constructed intent and returns accept/reject.
- **Authoritative shots** — a client MUST send intent (`angle`, `power`, `fire`), never “I hit / I won”. Only `Authority` (in-process or behind UDP) runs `simulate_shot`.
- **HAL stays thin** — `DeviceHal` reads buttons and flushes dirty rects. It MUST NOT decide whose turn it is.
- **No sockets / exceptions / iostream in `game/`** — P4 firmware and the host share this code. UDP wire + JSON serial live in `shared/` and `server/`.

## Complexity

Watch for change amplification (a HUD tweak touching physics), high cognitive load, and unknown unknowns. Prefer one module per concept: `World`, `Match`, `Renderer`.

## Layering

```text
UDP/serial handler  →  Authority::submit →  Match::apply →  World + physics
ServerProxy local   →  Authority::submit (same process)
display HAL         →  Renderer          →  RGB565 buffer
```

Handlers print JSON; they do not apply craters themselves. The server broadcasts accepted **commands** (`state = reduce(state, command)`) and a **snapshot** after keyframes (start, fire, crater, reconnect). Clients restore from a snapshot, then replay any `cmd` lines after that `seq`. The shell is presentation-only (`Match::step_fx`); the next snapshot is the settled world. Disconnects keep the PvP seat for the same `token` so a board can hello again without jumping to a local match.

## Immutability

Prefer transform-and-return for `ShotOutcome`. `Match` is mutable because it owns the live projectile on a small MCU — keep mutations inside `Match` methods, not scattered in the HAL.

## Design heuristics

- Behavior on the type that owns the state (`World::apply_crater`, not `TerrainUtils`).
- Composition over inheritance.
- Deep modules: small public surface (`Match::apply`, `fire_shot`, `tick`, `view`).
- Guard clauses; named predicates (`is_human_turn`).
- Avoid `Manager`/`Helper` dumping grounds.

## Embedded / C++

- C++17, no RTTI required, no exceptions in `game/`.
- Fixed display size 480×320 RGB565 (307,200 bytes). Allocate the compose buffer from PSRAM; SPI uses a DMA line buffer (`480 * 3` RGB666 bytes).
- Dirty rectangles for SPI; full-screen fallback when the scene changes (terrain crater, turn HUD).
- Deterministic `Rng` from a map seed so the same seed is the same battlefield on every device.

## Definition of done

- Happy path + important edges (out-of-map shot, crater under a tank, bot legal shot)
- `make test` passing
- Firmware still builds if you touched `firmware/` or `game/`
- No unrelated refactors
