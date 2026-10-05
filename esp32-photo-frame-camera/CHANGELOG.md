# Changelog

## [Unreleased]

### Added

- **Native Windows development** — `make.cmd` + `scripts/make.ps1` (Makefile-equivalent targets and `VAR=value` syntax), `scripts/deps.ps1`, `docs/windows-native.md`; build/flash/logs via `COM*` ports without WSL or usbipd.
- **Agent-friendly tooling** — `scripts/devctl.py` (pyserial) + Makefile wrappers (`make ports`, `make cmd-*`, `make logs-*`, `make capture`) for non-interactive build/flash/command/log loops; plus device-side serial command interfaces on both CoreS3 and PaperColor.
- **`docs/wsl-usb-flash.md`** — Windows + WSL2: usbipd-win USB attach, `make usb-status`, build/flash both boards.
- **`scripts/usb-wsl-status.sh`**, **`scripts/usb-wsl-help.sh`** — WSL USB helpers; Makefile targets `usb-status`, `usb-help`, `verify-photo-post`, `monitor-*`.
- **Photo frame MVP firmware** — CoreS3 capture + POST; PaperColor softAP, SD gallery, e-ink (`cores3/`, `papercolor/`).
- **`shared/protocol/photo_frame_wifi.h`** — AP/STA static IP constants for the pair.
- **`scripts/verify-photo-post.sh`** + **`shared/test_assets/colorbars_qvga.jpg`** — host-side POST smoke test.
- **`cores3/include/secrets.h.example`** — Wi-Fi template for CoreS3.
- **`make deps`** + **`scripts/deps.sh`** — one-shot setup: apt build deps, PlatformIO, ESP-IDF v5.5.1 (`esp32s3`), shallow vendor reference clones at pinned commits, and `pio pkg install` / `idf.py set-target` for project libraries.
- **`README.md`** — optional [Superpowers](https://github.com/obra/superpowers) section: how it complements this starter, Claude Code and Codex install steps, basic skill-driven workflow, and pointer to upstream install docs for other agents.
- **`INIT.md`** — one-shot bootstrap prompt the user runs once per repo (*"Follow `INIT.md`."*) so the agent discovers and inserts a populated `## Project context` block (stack/versions, package manager, commands, non-obvious patterns) into `AGENTS.md`. Scoped to evidence-backed values from manifests, lockfiles, CI, and task runners; explicitly forbids speculation, architecture overviews, and edits outside the new block. Includes a **Step 4 — Monorepo handling** that detects workspace declarations (`pnpm-workspace.yaml`, `nx.json`, Cargo `[workspace]`, uv/poetry workspaces, …) and proposes per-module sub-`AGENTS.md` files only for modules whose stack/commands/patterns diverge from the root, with each sub-file scoped to overrides only (target ≤ 30 lines) per the Codex deeper-file-wins precedence rule. References the studies that motivate the scope: Gloaguen et al. (arXiv:2602.11988) and the Augment 2026 AGENTS.md study (the latter found module-level files outperform monolithic root files for mid-size modules).

### Fixed

- **PaperColor e-ink never refreshed (root cause)** — the panel was left asleep, holding `BUSY` (GPIO11) low, so every M5GFX `_wait_busy()` burned its full 20s timeout: `M5.begin()` took **40.7s** and refreshes never visibly completed. Two causes, both fixed in `DeviceHal::bring_up_power_rails()` *before* `M5.begin()`: (1) PMIC `PWR_CFG` (reg `0x06`) auto-clears on every reset and the EPD rail is off by default — now enables charge/DCDC/LDO/**boost** (the e-paper high-voltage rail) plus `HOLD_CFG` so rails survive USB unplug; (2) `cfg.clear_display = false` makes M5Unified call `Display.init_without_reset(false)`, so M5GFX's PaperColor branch never pulses the panel's RST line — now pulsed explicitly on GPIO12. Result: `M5.begin()` **671ms**, full colour refresh **~17.4s**.
- **PaperColor photo receive vs display** — HTTP handler now only saves/queues; main loop performs e-ink refresh (matches `"queued"`). Shared SPI2 is serialized with a bus mutex, since LovyanGFX drives the panel via direct register access and does not participate in the IDF SPI driver's arbitration with `sdspi`.
- **PaperColor empty-gallery boot** — when SD is mounted but has no photos, boot now paints the status screen instead of skipping the e-ink refresh (panel looked “stuck” on old content). Added serial `home` command to force a status refresh.
- **PaperColor SD mount** — reuse SPI2 already owned by the e-ink panel (`bus_shared`), re-assert `PY_SD_PWR_EN` after PMIC init, and skip mount when CARD_DEC reports no card (was leaving `sd_mounted: false` and making gallery/nav appear broken).

### Changed

- **PaperColor gallery diagnostics** — verbose logs for SD mount, save/verify, scan, and BtnA/B/C; `status` now reports `sd_inserted`, `photo_count` and `epd_busy`; refreshes log their duration. New serial commands: `epdtest` (colour-bar pattern + refresh timing), `sdtest` (SD write/read/delete), `pins` (read-only EPD pin levels).
- **PaperColor without SD** — incoming `POST /api/v1/photo` JPEGs display on e-ink from RAM when no SD card is mounted; gallery/history still requires SD.
- **README.md** + **AGENTS.md** + **docs/agent-tooling.md** — Windows-first `make` workflow documented; WSL + usbipd as alternative.
- **`scripts/devctl.py`** — cross-platform serial reads (no `select`) so `identify` / `logs` / `cmd` work on Windows `COM*` ports.
- **`scripts/flash-cores3-windows.ps1`** — uses repo-local build artifacts (no hard-coded WSL path); usbipd detach/attach optional via `-BusId`.
- **CoreS3 USB serial** — `ARDUINO_USB_MODE=0` on `M5CoreS3` so `Serial` uses hardware USB Serial/JTAG on the same `COM*` / `/dev/ttyACM*` as agent tooling.
- **`make usb-attach`** — re-attach detached ESP32 boards from WSL via Windows PowerShell + usbipd.
- **`make flash-cores3-windows`** — build in WSL, flash CoreS3 via Windows COM (`scripts/flash-cores3-windows.ps1`).
- **CoreS3 boot** — disable loop/core0 WDT during `CoreS3.begin()`; early `PF_LOGI` for agent log capture.
- Restructured guidelines for agent-first use: **AGENTS.md** now leads with a work loop, a **what goes where** table, and explicit **Always / Ask first / Never** guardrails.
- **CODE_GUIDELINES.md**: added **Project-defining rules** at the top; compressed generic design content into **Design heuristics**; removed duplicate testing prose, meta “working with AI” / interview sections, and advanced distributed-state notes; tests fully delegated to **TESTING.md**.
- **WEBAPP_GUIDELINES.md**: added **Project-defining frontend rules** at the top; merged principles and stack notes; removed duplicate E2E and HTTP integration sections (now only in **TESTING.md**).
- **TESTING.md**: single source for core principles, Python (GivenPy), Node/TS HTTP integration, and browser E2E (page objects, pyramid, selectors) with per-stack **DO / DON'T** checklists.
- **README.md**: updated file descriptions and import guidance to match the split.
- Added `scripts/install.ps1` and `scripts/install.sh` for copying agent guideline files into an existing project after cloning, plus a shell-script LF line-ending rule.
- **`README.md`** + **install scripts**: included `INIT.md` in the file listing, the inline `cp` brace expansion, and both `install.sh` / `install.ps1` `files` arrays; expanded the **After importing** section into a three-step workflow (review → run `INIT.md` → start working) so the bootstrap path is the first thing a new user sees.
