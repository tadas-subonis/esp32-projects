# ESP32-P4 Multiplayer Handheld Console — Project Context & Build Plan

**Status:** Prototype hardware bring-up in progress  
**Primary prototype platform:** Waveshare ESP32-P4-WIFI6-POE-ETH (ESP32-P4 + ESP32-C6)  
**First game:** 1v1 tank artillery  
**Purpose of this document:** persistent project context for future conversations about controls, battery/power, game installation, networking, game SDK, matchmaking, enclosures, manufacturing, and production hardware.

---

## 1. Product vision

Build a **very cheap, deliberately simple, always-online handheld game console** inspired by the original Game Boy and by the bare, utilitarian feel of Teenage Engineering Pocket Operators.

The product should not try to compete with phones, Steam Decks, or modern retro handhelds on graphics or content volume. Its identity is:

- inexpensive enough to feel disposable / collectible;
- simple physical controls;
- instant multiplayer;
- short, replayable games;
- persistent player identity;
- games distributed as small removable-media packages or downloadable binaries;
- native ESP32 development rather than a proprietary runtime;
- hardware simple enough that outside developers can understand the entire platform.

The console should feel like a **networked embedded toy**, not a miniature Linux computer.

---

## 2. Cost target

### Original goal

The original concept targeted approximately **$10 manufacturing cost**.

### Current P4 direction

Once we moved to ESP32-P4, a larger color display, audio, Wi-Fi, and more capable graphics, a more realistic production target became:

- **target:** approximately **$14–17 BOM**
- **hard ceiling:** approximately **$20 total manufacturing BOM**
- lower is better;
- development boards and retail display modules are only for prototyping and do **not** represent final production cost.

The production product should use:

- a custom PCB;
- bare modules / SoCs rather than development boards;
- a bare TFT panel rather than an Arduino/Raspberry Pi display module;
- inexpensive power-management components;
- simple mechanical construction.

---

## 3. Core gameplay/product idea

The console is primarily for **simple multiplayer games**.

Initial game families considered:

- tank artillery;
- Stratego / micro-Stratego;
- Liar's Dice / bluffing games;
- chess variants;
- poker;
- simultaneous-turn tactical games;
- lightcycles / simple real-time games.

The design should favor games where another human is genuinely interesting to play against.

Important characteristics:

- short matches;
- low bandwidth;
- simple controls;
- high replayability;
- skill rating;
- fast rematches;
- bot fallback when matchmaking cannot find a human;
- procedural/random variation where appropriate.

The **first real game will be a 1v1 tank artillery game** because it is easy to understand and exercises almost every important subsystem without requiring a complicated renderer.

---

## 4. Player identity and online model

Each physical console should have a persistent identity.

```text
physical device
    |
    +-- device unique ID / hardware-derived identity
    |
    +-- device credential / secret
    |
    v
central server
    |
    +-- player profile
    +-- ratings
    +-- match history
    +-- matchmaking
    +-- game-specific rating
```

Desired behavior:

- device can work without typing email/password on the handheld;
- player identity starts from the device identity;
- later, a device may optionally be linked to a normal web account;
- ratings should be game-specific;
- matchmaking should use **TrueSkill or a similar skill rating system**;
- if no human opponent is found after a reasonable wait, an AI bot can fill the slot;
- bots should use the same game protocol as human clients whenever practical.

The player account should **not live on an SD card**. Losing or copying an SD card must not clone or destroy the player's identity.

---

## 5. Game/software philosophy

There should **not** be a heavy custom OS or proprietary virtual machine.

The platform should instead provide:

1. standardized ESP32 hardware;
2. a tiny boot/recovery/launcher environment;
3. a helper SDK/library;
4. a multiplayer/server protocol;
5. native ESP-IDF game binaries.

Each game should be a **normal ESP-IDF application binary** built by the developer.

The helper SDK should make common things easier but should not prevent direct hardware access.

---

## 6. Game installation model

Games should be built as complete native binaries.

```text
/game.toml
/game.bin
/icon.bin
/signature.bin
/assets/
```

Insert SD → read manifest → verify → copy game binary to internal flash → set boot partition → restart into game.

The game should execute from internal flash rather than depending on the removable SD card remaining inserted.

Recovery gesture: hold A+B during power-on → force launcher / recovery.

---

## 7. Two-SD concept

Two SD slots are desirable but not required for the first working prototype.

- SD slot A — removable game/card slot
- SD slot B — library/data slot

For the current proof of concept, **one SD slot is enough initially**.

---

## 8. Current prototype hardware

**Waveshare ESP32-P4-WIFI6-POE-ETH, Rev 2.0**

- ESP32-P4 + ESP32-C6 (Wi-Fi 6 / BLE)
- 32 MB PSRAM, 32 MB physical flash (idf image still says 16 MB; harmless for LCD, fix later)
- onboard microSD, speaker/audio, USB-C, MIPI DSI/CSI, 2×20 GPIO header
- Ethernet/PoE is **not** part of the intended handheld product

### Current working display

- ~3.5", 480×320, red PCB, SPI, marking **3.5" TFT SPI 480X320 V1.0**
- ILI9488-style; touch and display-board SD unused
- Prototype only. Production wants cheaper/faster 3.2–3.5" bare IPS, 8/16-bit I80 or RGB parallel, ~$2–5

### Known-good TFT GPIOs (right / outer header column)

```text
TFT_CS    = GPIO22
TFT_RST   = GPIO5
TFT_DC    = GPIO4
TFT_MOSI  = GPIO36
TFT_SCLK  = GPIO32
TFT_MISO  = not connected
TFT VCC   = 5V
TFT LED   = 3.3V
TFT GND   = GND
```

Treat the working debug source as the source of truth if it differs. Screen logic is **3.3 V**. Never feed 5 V into a P4 GPIO.

Stage 0 LCD is **DONE**. Git tag: `hw-lcd-working`.

---

## 9. Controls (Stage 1)

Eight buttons: D-pad, A, B, SELECT, START. GPIO to GND, internal pull-ups, pressed = LOW, debounce in software.

Prototype button GPIOs (left / inner header column — LCD stays on the right):

```text
UP      GPIO20
DOWN    GPIO6
LEFT    GPIO3
RIGHT   GPIO2
A       GPIO33
B       GPIO26
SELECT  GPIO48
START   GPIO47
```

Avoid GPIO7/8 (onboard I2C / ES8311), GPIO53 (amp enable), GPIO37/38 (Type-C UART), GPIO0 (boot).

---

## 10. Audio / power / networking (later)

- Audio later: press A → beep. Speakers on hand are 8 Ω / 2 W. Onboard ES8311 + NS4150B.
- Power is **not** the first prototype milestone. USB-C first. Dev-board current is not representative of production.
- First version uses a **central server**, not peer-to-peer.
- Device identity must not live on SD.

---

## 11. First game: 1v1 tank artillery

Simplest game that proves the platform. Local sandbox before Wi-Fi.

Controls: LEFT/RIGHT angle, UP/DOWN power, A fire, B cancel later.

Send `FIRE { angle, power, weapon, turn_number }`. Both clients simulate. Fixed timestep / deterministic.

Bot fallback **after** human-vs-human works.

---

## 12. Development roadmap

| Stage | Exit criterion | Status |
|---|---|---|
| 0 Display | test patterns after power cycles | **DONE** (`hw-lcd-working`) |
| 1 Controls | eight buttons reliable on screen | **DONE** (`hw-buttons-working`) |
| 2 Local artillery | two people share one device, finish a match | **next** — `docs/stage2-local-artillery.md` |
| 3 Audio | fire/explode/win sounds, no crashes | |
| 4 Onboard SD | FAT32 read/write + one asset | |
| 5 Wi-Fi via C6 | stable IP, message to server | |
| 6 Device identity | reboot = same player | |
| 7 Two-device square test | low-latency state sync | |
| 8 Multiplayer artillery | two consoles complete matches | |
| 9 Matchmaking | Play → find each other | |
| 10 Rating | TrueSkill persists | |
| 11 Bot fallback | one device still starts a match | |
| 12 Launcher / install | SD → native game → recovery | |
| 13 Battery | untethered + voltage report | |
| 14 Second SD | cartridge slot | |
| 15 Custom PCB | after interfaces stabilize | |

Do not add later-stage features until the current stage works.

Suggested tags: `hw-lcd-working` `hw-buttons-working` `hw-sd-working` `hw-audio-working` `hw-wifi-working` `net-two-devices-working` `game-artillery-local` `game-artillery-online` `launcher-install-working` `battery-working`

---

## 13. First real success

> Two ugly USB-powered breadboard consoles, each with a P4, screen and six buttons, automatically connect through the server and play a complete tank-artillery match against each other.

When there is a choice between adding infrastructure and getting two people playing: **get two people playing.**

---

## 14. Debugging principles

1. Keep a known-good reference firmware for every subsystem.
2. Change only one subsystem at a time.
3. Never trust generated diagrams over the official board pinout.
4. Record exact GPIO mappings in source and this document.
5. Test power with USB before battery.
6. Start buses at slow frequency, then increase.
7. Never change wiring and driver code simultaneously when debugging.
8. Use serial logs heavily.
9. Use multimeter/continuity checks for ambiguous wiring.
10. Once something works, commit/tag it before changing it.

---

## 15. Open questions

- Production: P4+C6 vs P4+C5
- Production panel: 3.2 vs 3.5, 240×320 vs 320×480, 8-bit vs 16-bit I80 vs RGB
- Battery: 2×AAA / 3×AAA / LiPo / both
- Dedicated Start/Menu later
- One vs two SD slots in production

---

## 16. Short paste for new conversations

> I am building an ultra-cheap multiplayer handheld console based on ESP32-P4. Current prototype uses a Waveshare ESP32-P4-WIFI6-POE-ETH Rev2 board (P4 + C6), 32 MB PSRAM, a working 3.5" 480×320 SPI ILI9488-style TFT, eight buttons, onboard microSD and an 8Ω/2W speaker. Display and buttons are confirmed working. Current TFT wiring is CS GPIO22, RESET GPIO5, DC GPIO4, MOSI GPIO36, SCK GPIO32, VCC 5V, LED 3.3V, MISO disconnected. Buttons: UP GPIO20, DOWN GPIO6, LEFT GPIO3, RIGHT GPIO2, A GPIO33, B GPIO26, SELECT GPIO48, START GPIO47, each to GND with internal pull-ups. Immediate next milestone is local 1v1 artillery on this hardware (HAL port in the game repo; spec in docs/stage2-local-artillery.md), then Wi-Fi, two-device sync, matchmaking, rating, bot fallback, SD install, battery, second SD, and finally custom PCB.
