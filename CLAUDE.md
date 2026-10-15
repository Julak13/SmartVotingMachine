# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A single Arduino sketch (`SmartVotingMachine/`) — ESP32 DevKit V1 firmware for an electronic voting machine with two-factor voter authentication (PN532 NFC card **+** AS608 fingerprint), a voter-only 16×2 LCD/3-button interface, and a self-hosted WiFi admin dashboard. Voting is fully **offline** — no cloud, no SD card, no external DB; all state lives in the ESP32's internal flash (LittleFS). Since v1.1.0 the machine can *optionally* join a router to send results to Telegram (`Uplink.cpp`); voting must never depend on that link.

The device is deployed with a client who updates it themselves through the browser flasher in `docs/` (GitHub Pages, ESP Web Tools) — see "Releasing" below.

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
| `webTask` | 0 | WiFi (own AP always, router STA optional) + HTTP server (admin only) |
| `uplink` | 0 | router connection state machine + every Telegram HTTPS call (started by `webTask`) |

WiFi/HTTP is pinned to **core 0** (with the radio stack) so HTTP jitter never delays the deterministic ballot flow on **core 1**. **The web task never touches voter-facing hardware directly** — this is a hard rule, because the LCD/PN532 share one I2C bus and the AS608 is a single UART owner. Web handlers also never do network I/O: they queue a job on `Uplink` and the dashboard polls `/api/uplink/status`.

Other web→voter-task requests use the same request/execute pattern as enrollment: deleting a voter calls `VotingController::queueTemplateDelete(fid)`, and `run()` erases the AS608 slot at the top of its loop (before any enrollment, which may reuse that slot).

### Voter-loop invariants (v1.1.0 — each fixes a field complaint)

- `selectCandidate()` must always be able to return: `SESSION_IDLE_MS` with no button press → `SELECT_TIMEOUT`. A loop without an exit froze the whole machine for every later voter.
- `FpSensor::search()` distinguishes `FP_BAD_IMAGE` (retry silently) from `FP_NO_MATCH` (counts toward `FP_MAX_ATTEMPTS`). Never treat a smudged read as a rejection.
- PN532: `NFC_PASSIVE_RETRIES` makes the chip end each card search on its own **inside** `NFC_READ_TIMEOUT_MS`. If the host wait expires first, the next command collides with the unfinished one and reads fail intermittently. Keep that ordering if you touch either value. `NFCReader::maintain()` self-tests every 5 s and hardware-resets via RST (GPIO5).
- A card just used is ignored until lifted for `CARD_REPEAT_IGNORE_MS`; enrollment seeds this with the new card.
- Enrollment refuses a finger already enrolled to another voter (two templates of one finger make that voter unable to vote). `sweepOrphanTemplates()` runs once in `setup()` to clear slots no voter owns; it deliberately does nothing when the voter list is empty.

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

`Uplink::beginWifi()` brings up the AP (`WIFI_AP`, or `WIFI_AP_STA` when a router is configured) and `WebServer` serves port 80 (`AdminServer.cpp`). Auth is password → random session token: `POST /api/login` checks `ADMIN_PASSWORD` and returns a token; every other `/api/*` route requires `?t=<token>` (checked by `authed()`). The token is a single global that resets on reboot. The full route list is in `README.md` § "Web API quick reference" — keep both in sync when adding endpoints.

Router link: a failed connection stops retrying for `STA_RETRY_MS` (5 min) on purpose — continuous reconnect scans hop channels and make the admin AP stutter. Final results are snapshotted in `electionStopped()` and retried by the worker, so a later reset can never change what is sent. Router/Telegram settings persist in `/uplink.json` (not NVS); secrets are masked in `GET /api/settings`.

## Conventions & gotchas

- **All** pins, credentials, capacity limits and timings live in `config.h` — change values there, never inline in `.cpp` files. `MAX_VOTERS`/`FP_MAX_ID` are bounded to the AS608's ~127-template capacity; don't raise them blindly.
- Shared **I2C** bus: LCD `0x27` (or `0x3F`), PN532 `0x24` on GPIO21/22. AS608 is on **UART2** (Serial2, GPIO16/17). PN532 DIP switches must be set to **I2C** mode.
- The buzzer code (`Feedback.cpp`) assumes an **active** buzzer driven by plain `digitalWrite` — no `tone()`/LEDC. The deployed module is a 3-pin **low-level trigger** (`BUZZER_ACTIVE_HIGH 0`) on **GPIO23**, powered from **3.3 V** (at 5 V a 3.3 V "off" level is too low and it beeps nonstop). GPIO13/14 misbehaved on the delivered board — don't move it back.
- **`NetworkClient(Secure)::setTimeout()` takes milliseconds** on core 3.3.x (it is `Stream::setTimeout`; only `NetworkServer` has a seconds version). Telegram uses a hand-rolled HTTP/1.0 POST, not `HTTPClient` — a stale core-2.x `HTTPClient` in `Documents\Arduino\libraries` can shadow the core one.
- Partition scheme must stay **Default 4MB with spiffs** across releases; changing it moves the LittleFS partition and wipes every voter on the deployed unit.

## Releasing an update to the deployed unit

1. Bump `FW_VERSION` in `config.h` and `version` in `docs/manifest.json`.
2. Build for `esp32:esp32:esp32` (default partition scheme) and copy into `docs/firmware/`: `*.ino.bootloader.bin` → `bootloader.bin`, `*.ino.partitions.bin` → `partitions.bin`, `*.ino.bin` → `firmware.bin`, core `tools/partitions/boot_app0.bin` → `boot_app0.bin`.
3. Push to `main`; GitHub Pages serves `docs/` at https://julak13.github.io/SmartVotingMachine/.

`docs/manifest.json` sets `new_install_prompt_erase: true` — ESP Web Tools **erases the whole flash by default** otherwise, which would delete every voter. The manifest's parts deliberately stop at the app (0x10000) so the LittleFS data partition is never written. The client must leave "Erase device" unticked; the page says so prominently.
- **Windows / case-insensitive filesystem gotcha**: the sketch folder is the first `-I` include path, so a sketch-local filename that case-insensitively matches an ESP32 core/library header (e.g. `Network.h`, `WiFi.h`, `Server.h`) will silently shadow it and produce a cascade of unrelated "not declared" errors deep inside core headers. Never name a project file that way.
