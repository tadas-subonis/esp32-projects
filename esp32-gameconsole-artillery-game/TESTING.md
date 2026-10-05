# Testing guidelines

## Core principles

- Name tests for **behavior** (`test_world_should_be_deterministic_for_a_seed`).
- Linear tests: setup → one act → assertions.
- For new behavior, assert the public outcome (`ShotOutcome`, `Match::phase()`).
- Prefer the deepest layer that still reproduces a bug (`game/` over flashing a board).

## C++ (`game/tests/test_core.cpp`)

Host binary `artillery-tests`. No extra framework: `CHECK` macros, Given/When/Then as comments in the test name.

Covers: deterministic maps, crater lowering, ballistic termination, match phase transitions, legal bot shots, reject fire/aim from the waiting player, in-process `Authority` (local server). Live `Match::tick` is 70% wall speed; `simulate_shot` is not.

```bash
cmake -S . -B build-host && cmake --build build-host
./build-host/artillery-tests
```

Windows: `make test` (MSVC + CMake/Ninja; binaries are `build-host/*.exe`).

## Sim CLI (`tests/test_sim.py`)

`unittest` against `artillery-sim` JSON stdout — golden physics, no live server.

- `state --seed N` starts a match in `aiming`
- `fire` returns impact + flags
- same seed + shot is byte-for-byte deterministic

## UDP link (`artillery-link-tests`)

Socket-free session tests: packed bytes round-trip, reliable message survives a dropped datagram by repetition, ordered delivery under loss, unreliable aim latest-wins, full-height `State` fits one UDP packet.

## UDP server (`tests/test_server.py`)

Starts `artillery-server`, speaks binary UDP v2 (`tests/udp_wire.py`), asserts the server rejects illegal fire and simulates a legal shot.

```bash
./build-host/artillery-server --port 7420
./build-host/artillery-cli --port 7420 start --seed 42 --mode bot
./build-host/artillery-cli --port 7420 fire
```

## Device (when hardware is attached)

`make cmd CMD=status` and `make cmd CMD="new 42"` then `fire`. `make snap` writes a 120×80 PNG under `logs\`. Do not treat the LCD as the only oracle — JSON `phase` / `hp0` / `hp1` / `fps` / `frame_ms` is the assertion.

## DO / DON'T

**DO** — lock physics with a golden `artillery-sim fire` pair when you change gravity/wind.

**DON'T** — reimplement trajectory math in Python or Go without a test that compares to `simulate_shot`.
