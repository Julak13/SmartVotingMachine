# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A single Arduino sketch (`SmartVotingMachine/`) — ESP32 DevKit V1 firmware for a fully **offline** electronic voting machine with two-factor voter authentication (PN532 NFC card **+** AS608 fingerprint), a voter-only 16×2 LCD/3-button interface, and a self-hosted WiFi admin dashboard. No cloud, no internet, no SD card, no external DB — all state lives in the ESP32's internal flash (LittleFS).

## Build / compile / upload

There is **no CLI build, test suite, or linter** — compilation happens through the Arduino IDE (or `arduino-cli` if configured). All `.h/.cpp/.ino` files sit in one folder and compile together as one sketch.

- **Board**: `esp32:esp32:esp32` (ESP32 Dev Module), **ESP32 board package 3.x** (the FreeRTOS/`WebServer`/`LittleFS`/`WiFi` APIs assume core 3.x).
- **Partition scheme** must include a filesystem (e.g. *"Default 4MB with spiffs"*). `LittleFS.begin(true)` auto-formats it on first boot, so there is **no separate filesystem upload step** — flashing firmware deploys everything, including the dashboard (it lives in PROGMEM in `web_assets.h`, served via `send_P`).
- **Required libraries** (Arduino Library Manager):
  - Adafruit Fingerprint Sensor Library, Adafruit PN532, Adafruit BusIO (dependency)
  - ArduinoJson **v7** (the code uses the v7 `JsonDocument` / `.to<JsonArray>()` / `.add<JsonObject>()` API — v6 will not compile)
  - `LiquidCrystal_I2C` — the installed fork exposes `.begin()` (not `.init()`). `Display.cpp` calls `_lcd.begin()`; do not "fix" it to `.init()` — that won't compile against this fork.
  - `WiFi`, `WebServer`, `Wire`, `LittleFS`, `FreeRTOS` are **core-bundled** — never install standalone copies (a stray standalone `WebServer` in `libraries/` will shadow the core one).
- Before flashing, set `AP_SSID` / `AP_PASSWORD` / `ADMIN_PASSWORD` in `config.h`. Serial Monitor @ **115200** prints the SoftAP IP (`192.168.4.1`).

## Architecture

### Two tasks, one shared state object, no other coupling

Two FreeRTOS tasks (created in `SmartVotingMachine.ino`) communicate **exclusively** through the global `store` object (`Store.h/.cpp`), guarded by one **recursive** mutex. There are no queues and no per-feature locks.

| Task | Core | Owns |
|------|------|------|
| `voterTask` | 1 | LCD, buttons, PN532, AS608, buzzer/LEDs, the voting state machine **and** enrollment execution |
| `webTask` | 0 | WiFi SoftAP + HTTP server (admin only) |

WiFi/HTTP is pinned to **core 0** (with the radio stack) so HTTP jitter never delays the deterministic ballot flow on **core 1**. **The web task never touches voter-facing hardware directly** — this is a hard rule, because the LCD/PN532 share one I2C bus and the AS608 is a single UART owner.

### The enrollment handshake (the central design constraint)

The AS608 fingerprint sensor is owned solely by the voter task, but enrollment is *initiated from the dashboard*. So enrollment is **requested** by the web task and **executed** by the voter task:

- `AdminServer` → `VotingController::enrollRequest(name)` sets a flag in an `EnrollState` struct (guarded by a *separate* `_enMtx` mutex, distinct from the store mutex).
- `VotingController::run()` checks that flag at the top of its loop and runs `runEnrollment()` itself, publishing step-by-step progress back into `EnrollState`.
- The dashboard polls `GET /api/voters/enroll/status` (which reads `enrollStatusJson()`) to drive the UI.

The LCD shows only a neutral "Enrolling — use dashboard" message: **admins never use the LCD**; all enrollment prompts ("place finger", "tap card") go to the dashboard. When adding any sensor-driven admin action, follow this request/execute/poll pattern — do not call the sensor from the web task.

### Store: in-RAM vectors mirrored to LittleFS

`Store` holds `std::vector<Voter>` and `std::vector<Candidate>` in RAM and mirrors them to JSON files (`/voters.json`, `/candidates.json`, `/state.json`) after **every** mutation. Reads use the **snapshot pattern**: `snapshotVoters()` / `snapshotCandidates()` return copies by value so callers don't hold the lock while iterating. Persisted writes are immediate, so a power loss never loses more than the in-flight operation.

### Vote integrity

Two-factor: the NFC UID and the fingerprint id must resolve to the **same** voter record before a ballot is issued (`VotingController::handleVoter` looks up by UID, then `runFingerprintCheck(expectedFid)`). `Store::castVote()` is the only writer of vote counts; it re-validates the `voted` flag **atomically under lock** at commit time, and persists the voted flag **before** the count — so the worst-case power-loss outcome is a counted vote the voter can retry, never a double count. Voting is refused unless `status == ELECTION_RUNNING`.

### Web layer

`WiFi.softAP()` + `WebServer` on port 80 (`AdminServer.cpp`). Auth is password → random session token: `POST /api/login` checks `ADMIN_PASSWORD` and returns a token; every other `/api/*` route requires `?t=<token>` (checked by `authed()`). The token is a single global that resets on reboot. The full route list is in `README.md` § "Web API quick reference" — keep both in sync when adding endpoints.

## Conventions & gotchas

- **All** pins, credentials, capacity limits and timings live in `config.h` — change values there, never inline in `.cpp` files. `MAX_VOTERS`/`FP_MAX_ID` are bounded to the AS608's ~127-template capacity; don't raise them blindly.
- Shared **I2C** bus: LCD `0x27` (or `0x3F`), PN532 `0x24` on GPIO21/22. AS608 is on **UART2** (Serial2, GPIO16/17). PN532 DIP switches must be set to **I2C** mode.
- The buzzer code (`Feedback.cpp`) assumes an **active** buzzer driven by plain `digitalWrite` — no `tone()`/LEDC. `BUZZER_ACTIVE_HIGH` in `config.h` flips polarity.
- **Windows / case-insensitive filesystem gotcha**: the sketch folder is the first `-I` include path, so a sketch-local filename that case-insensitively matches an ESP32 core/library header (e.g. `Network.h`, `WiFi.h`, `Server.h`) will silently shadow it and produce a cascade of unrelated "not declared" errors deep inside core headers. Never name a project file that way.
