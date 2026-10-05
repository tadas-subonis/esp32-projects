# AGENTS.md

## Project context

> Loads on every agent invocation. Keep short.

- **Stack / versions** — Monorepo: `cores3/` = PlatformIO Arduino (espressif32@6.7.0, ESP32-S3); `papercolor/` = ESP-IDF v5.5.1 (esp32s3); wire protocol in `shared/protocol/`.
- **Host OS** — **Windows native (default):** USB boards as `COM*`; `make.cmd` + `scripts/make.ps1`. **WSL (optional):** `/dev/ttyACM*` via usbipd; repo `Makefile` + bash scripts.
- **Package manager** — PlatformIO (`pio`) for cores3; ESP-IDF via `%USERPROFILE%\esp\esp-idf\export.ps1` (Windows) or `. ~/esp/esp-idf/export.sh` (WSL) for papercolor.
- **Commands** — same target names on both hosts; port syntax differs (`COM*` vs `/dev/ttyACM*`):
  - **Windows:** `make deps` · `make build` · `make ports` · `make identify` · `make flash-cores3 PORT=COM7` · `make flash-papercolor PORT=COM8` · `make cmd-cores3 CMD=status CORES3_PORT=COM7` · `make logs-cores3 CORES3_PORT=COM7 SECONDS=8` · `make capture CORES3_PORT=COM7` · `make help` (impl: `make.cmd` → `scripts/make.ps1`; guide: `docs/windows-native.md`)
  - **WSL:** same `make …` targets via repo `Makefile` + `scripts/deps.sh` (guide: `docs/agent-tooling.md`, USB: `docs/wsl-usb-flash.md`)
  - Typecheck: `<not configured>`
  - Lint: `<not configured>`
  - Test (all): `<not configured>`
  - Test (single file): `<not configured>`
- **Non-obvious patterns** —
  - **Windows native (preferred for USB):** no usbipd; run `make` from repo root (`.\make.cmd …` if GNU `make` from Git shadows `make.cmd`). Agent loop: `docs/agent-tooling.md` + `docs/windows-native.md`.
  - **WSL on Windows:** USB serial requires [usbipd-win](https://github.com/dorssel/usbipd-win) before flash — `docs/wsl-usb-flash.md`; `make usb-status` / `make usb-help` / `make usb-attach`.
  - **Device identification:** `make identify` — match `serial_number`, not port index (`44:1B:F6:C1:85:98` = PaperColor, `30:ED:A0:D4:BC:14` = CoreS3). COM / ttyACM numbers can swap.
  - Shared contract is `shared/protocol/` (HTTP multipart), not a shared firmware library — each target builds independently.
  - UART on CoreS3 is LLM-only; JPEG is sent to PaperColor over Wi-Fi (`POST /api/v1/photo`).
  - On Windows, `make` targets source ESP-IDF via `export.ps1` internally; on WSL via `export.sh` in the Makefile.
  - `vendor/` is offline reference for porting — not auto-linked unless wired in `platformio.ini` or papercolor `CMakeLists.txt` / components.

## Recommended inner dev loop (agent-friendly)

Prefer non-interactive commands + log capture over long-running monitors. **Do not** run `make monitor-*` on the same port as `make cmd-*` / `make logs-*`.

**Windows (default):**

```powershell
make ports
make identify
make build
make flash-papercolor PORT=COM8
make flash-cores3     PORT=COM7
make cmd-papercolor CMD=status PAPERCOLOR_PORT=COM8
make logs-papercolor PAPERCOLOR_PORT=COM8 SECONDS=8
make capture        CORES3_PORT=COM7
```

**WSL:**

```bash
make ports
make identify
make build
make flash-papercolor PORT=/dev/ttyACM0
make flash-cores3     PORT=/dev/ttyACM1
make cmd-papercolor CMD="status" PAPERCOLOR_PORT=/dev/ttyACM0
make logs-papercolor PAPERCOLOR_PORT=/dev/ttyACM0 SECONDS=8
make capture CORES3_PORT=/dev/ttyACM1
```

## Agent work loop

1. **Plan** — For non-trivial work, explore the codebase, clarify ambiguities, then agree on an approach before large edits.
2. **Outline** — Before substantive changes, present a short outline and a skeleton (types, classes, functions) of what will change or be added.
3. **Implement** — Follow the pointers below; stay within scope; do not refactor unrelated code.
4. **Verify** — Run tests, lint, or typecheck for touched areas when the project provides them; fix failures you introduce.
5. **Document** — After substantive code changes, ensure `README.md` and `docs/` match current behavior when the project uses them.
6. **Changelog** — Log feature-level changes in `CHANGELOG.md` (skip trivial typo-only edits).

Whenever working, consult:

- `CODE_GUIDELINES.md` — API, HTTP, backend layering, architecture heuristics, review habits
- `WEBAPP_GUIDELINES.md` — front-end patterns (routing, client/server state, imports, Next.js defaults)
- `TESTING.md` — **Python (GivenPy) + Node/TypeScript (direct HTTP integration) + browser E2E (page objects)**
- `docs/agent-tooling.md` — serial commands, `devctl.py`, make targets, troubleshooting
- `docs/windows-native.md` — Windows `make`, COM ports, agent target matrix

Those files use **MUST** / **SHOULD** / **MAY** where strictness matters (see `CODE_GUIDELINES.md`).

## What goes where

| Topic | File |
|-------|------|
| Routes vs services vs repositories, DI, HTTP status mapping, DDD heuristics, parse-don't-validate, errors, review / DoD | `CODE_GUIDELINES.md` |
| Next.js App Router, client/server state, API client, cache keys, Zod at boundary, imports, AI from backend only | `WEBAPP_GUIDELINES.md` |
| GivenPy, Node integration tests, Playwright / page objects, E2E pyramid, test IDs | `TESTING.md` |
| Human overview, import snippets | `README.md` |
| Windows `make`, COM ports, build/flash | `docs/windows-native.md` |
| Agent serial loop, device commands, identify | `docs/agent-tooling.md` |
| WSL + usbipd USB forwarding | `docs/wsl-usb-flash.md` |
| Feature-level history | `CHANGELOG.md` |

## Guardrails (priority when rules conflict)

- **Always** — Follow MUST rules in guideline files; run safe diagnostics (config, logs, local commands) before asking the user; use tooling to isolate failures.
- **Ask first** — Destructive or system-wide actions; installing system-wide dependencies (unless explicitly instructed).
- **Never** — Install system-wide dependencies without explicit instruction; silently deviate from an approved plan; invent extra scope mid-task.

## Coding core

Simple, linear flows; composition over inheritance; immutable over mutable where practical; package by feature. Details: immutability, pipelines, and domain style in `CODE_GUIDELINES.md` (**Immutability and functional style** and **Design heuristics**).

## Learning

Record durable lessons in the appropriate guideline file or `docs/` — keep this file small.

## Collaboration stance

Act as a senior engineer with strong opinions, loosely held. Push back when a request would break existing tests, contradicts established patterns, or skips planning for non-trivial work. If overridden, comply and note the trade-off in `CHANGELOG.md`.

Announce non-trivial work before doing it. Do not silently deviate from an approved plan — stop and re-align first.

## Maintaining guidelines

When you add a **repeatable** convention (stack pitfall, test layout), put it in the right file per the table above — not duplicated across files. When you fix a production bug, add a test when practical (`TESTING.md`).
