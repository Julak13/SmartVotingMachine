# Smart Electronic Voting Machine (ESP32) &nbsp;·&nbsp; v1.1.1

A secure electronic voting machine built on an **ESP32 DevKit V1**. Two-factor voter
authentication (NFC card **+** fingerprint), a voter-only LCD/button interface, and a
self-hosted admin web dashboard. **Voting is fully offline**: no cloud, no SD card, no external
database; everything is stored in the ESP32's internal flash and survives reboots.

Optionally the machine can join an internet Wi-Fi **only to send results to Telegram**.
Voting never depends on that link.

**Update the firmware from a browser:** <https://julak13.github.io/SmartVotingMachine/>
(see §8).

---

## 1. System overview

```
                ESP32 DevKit V1
  ┌───────────────────────────────────────────┐
  │  Core 1  voterTask                         │      Core 0  webTask + uplink
  │  ┌──────────────────────────────┐          │   ┌──────────────────────────┐
  │  │ VotingController (state mc.)  │          │   │ WiFi: own AP (always)    │
  │  │  ├ NFCReader   (PN532, I2C)   │          │   │   + router STA (optional)│
  │  │  ├ FpSensor    (AS608, UART2) │          │   │ WebServer :80  REST + UI │
  │  │  ├ Display     (LCD, I2C)     │          │   │ Uplink worker: Telegram  │
  │  │  └ Buttons / Feedback         │          │   └────────────┬─────────────┘
  │  └──────────────┬───────────────┘          │                │
  │                 │       shared mutex-guarded global          │
  │                 └──────────────  Store  ────────────────────-┘
  │                         (LittleFS + ArduinoJson)             │
  └───────────────────────────────────────────┘
```

* **One global `Store`** is the single source of truth, guarded by one recursive mutex.
* The **voter task** owns every piece of voter-facing hardware. The web side never touches
  the LCD, buttons, NFC reader or fingerprint sensor.
* **Fingerprint enrollment** is *requested* by the dashboard and *executed* by the voter task
  (it owns the sensor); progress is published back through a mutex-guarded `EnrollState`
  that the dashboard polls. Deleting a voter queues their fingerprint slot the same way.
* **Telegram** sends run on their own worker task on core 0, so a slow TLS handshake can never
  stall the dashboard or the voting loop.

---

## 2. Project structure

| File | Responsibility |
|------|----------------|
| `SmartVotingMachine.ino` | Boot, peripheral init, launches the FreeRTOS tasks |
| `config.h` | All pins, AP/admin credentials, limits, timings (change values **here**) |
| `models.h` | `Candidate`, `Voter`, `ElectionStatus` data structures |
| `Store.h/.cpp` | RAM model + LittleFS/JSON persistence, mutex, vote-casting logic |
| `Display.h/.cpp` | 16×2 I2C LCD wrapper (only rewrites lines that changed) |
| `Buttons.h` | Debounced active-LOW button with edge detection |
| `FpSensor.h/.cpp` | AS608 fingerprint: 1:N search + enrollment primitives |
| `NFCReader.h/.cpp` | PN532 UID reader (I2C), configured once at boot |
| `Feedback.h/.cpp` | Buzzer + green/red status LEDs (voter feedback) |
| `VotingController.h/.cpp` | Voter state machine **and** enrollment workflow |
| `AdminServer.h/.cpp` | REST API + serves the dashboard |
| `Uplink.h/.cpp` | Optional router connection + Telegram results |
| `web_assets.h` | The entire dashboard (HTML/CSS/JS) in PROGMEM |
| `web_icon.h` | Home-screen icon (PNG) for "Add to Home Screen" |
| `docs/` | Browser firmware updater (GitHub Pages), diagrams, Chapter 4 figures |

---

## 3. Pin mapping & wiring

| ESP32 GPIO | Connects to | Notes |
|-----------:|-------------|-------|
| GPIO21 | SDA — LCD **and** PN532 | shared I2C bus |
| GPIO22 | SCL — LCD **and** PN532 | shared I2C bus |
| GPIO16 (RX2) | AS608 **TX** | fingerprint UART (TX/RX cross over) |
| GPIO17 (TX2) | AS608 **RX** | fingerprint UART |
| GPIO4 | PN532 IRQ | |
| GPIO5 | PN532 RST | |
| GPIO25 | Button **UP** | other leg → GND |
| GPIO26 | Button **DOWN** | other leg → GND |
| GPIO27 | Button **OK** | other leg → GND |
| GPIO23 | Buzzer **IO** | 3-pin low-level-trigger module: VCC → 3.3 V, GND → GND |
| GPIO32 | Green LED (+) | via 220–330 Ω resistor → GND |
| GPIO33 | Red LED (+) | via 220–330 Ω resistor → GND |
| VIN | 5 V supply (≥ 1.5 A) | powers the ESP32 |
| 3.3 V rail | PN532, AS608, LCD, buzzer | from a separate AMS1117-3.3 fed by the 5 V supply |
| GND | common ground for **all** modules | |

Wiring notes:
* Buttons use the ESP32 internal pull-ups (`INPUT_PULLUP`); wire each button between its GPIO
  and GND. No resistors, never to 3.3 V or 5 V.
* Set the PN532 DIP switches to **I2C** mode. Default I2C address `0x24`; LCD is `0x27`
  (or `0x3F`; change `LCD_ADDR` in `config.h` if yours differs).
* **Buzzer:** the fitted module is *low-level trigger* (`BUZZER_ACTIVE_HIGH 0`). Power it from
  **3.3 V, not 5 V** — at 5 V the ESP32's 3.3 V "off" level is not high enough and it beeps
  constantly. GPIO13/14 misbehaved on the delivered board, hence GPIO23.
* **AS608 is a 3.3 V part** (an R307 would want 5 V on VCC). Keep its supply stable;
  brown-outs corrupt the UART. A weak 5 V supply shows up as *Brownout* under
  Dashboard → Settings → Device.

---

## 4. Voter flow (LCD)

```
Tap NFC Card  →  Place Finger  →  Accredited / Press OK
   →  > Candidate A / Candidate B  (UP/DOWN to move, OK to pick)
   →  <name> / OK=Yes UP/DN=No
   →  Vote Recorded / Thank You
```
Rejections: `Card Not Known`, `Already Voted`, `Finger Mismatch`, `No finger read`,
`Session Timeout`, `Election Closed`. Both factors (NFC UID **and** fingerprint) must resolve
to the **same** voter record, and the voted flag is re-checked atomically under lock when the
ballot is committed.

Robustness (v1.1.x):
* No button press for 30 s on the candidate list cancels the session, so an abandoned booth
  can never freeze the machine.
* A smudged fingerprint is re-imaged rather than rejected; 3 clean non-matching reads reject.
* A card left on the reader shows *Remove card* instead of starting a new session.
* NFC card reading is exactly as in v1.0 (see §10 — the v1.1.0 change was rolled back).

---

## 5. Admin dashboard

1. Connect a phone/laptop to Wi-Fi **`VotingMachine`** (password `vote12345`).
2. Open **http://192.168.4.1** in a browser.
3. Sign in with the admin password (`admin123`).

Tabs: **Overview**, **Voters** (enroll, delete, voted status), **Candidates**, **Results**
(live bars + "Send to Telegram"), **Control** (start / stop / reset) and **Settings**
(Telegram, internet Wi-Fi, device health). It can be added to a phone's home screen.

### Telegram results (optional)
Settings → *Internet Wi-Fi*: pick the router and save. Then *Telegram results*: create a bot with
**@BotFather**, paste the token, press **START** in the bot, tap **Detect** (fills the chat ID),
then **Send test message**. With *Send results automatically* on, the final results are
snapshotted the moment the election is stopped and retried every minute until delivered.
The machine's own `VotingMachine` network stays up the whole time.

### Web API quick reference
All `/api/*` routes except `/api/login` require `?t=<token>` (obtained from login).

| Method | Route | Purpose |
|--------|-------|---------|
| GET | `/` | dashboard HTML |
| GET | `/manifest.json`, `/icon.png` | home-screen support (public) |
| POST | `/api/login` | `{password}` → `{ok, token}` |
| GET | `/api/overview` | counts + status |
| GET / POST | `/api/candidates` | list / add `{name}` |
| POST | `/api/candidates/edit` | `{id, name}` |
| POST | `/api/candidates/delete` | `{id}` |
| GET | `/api/voters` | list voters |
| POST | `/api/voters/delete` | `{fid}` (also erases the fingerprint) |
| POST | `/api/voters/enroll` | `{name}` → starts device enrollment |
| GET | `/api/voters/enroll/status` | enrollment progress (polled) |
| POST | `/api/voters/enroll/cancel` | abort enrollment |
| GET | `/api/results` | totals + per-candidate votes |
| POST | `/api/election/start\|stop\|reset` | election control (start/stop trigger Telegram) |
| GET | `/api/device` | firmware, uptime, last reset reason, sensor health, memory |
| GET / POST | `/api/settings` | Telegram + router settings (secrets masked on GET) |
| GET | `/api/uplink/status` | router + Telegram state, scan results, detect result |
| POST | `/api/telegram/test` | send a test message |
| POST | `/api/telegram/send` | send current results |
| POST | `/api/telegram/detect` | read the chat ID from the bot's latest message |
| POST | `/api/wifi/scan` | scan for router networks |

---

## 6. Libraries (install via Arduino Library Manager)

| Library | Why |
|---------|-----|
| **ESP32 board package 3.x** (Espressif) — verified on 3.3.11 | Core, WiFi, WebServer, LittleFS, FreeRTOS, NetworkClientSecure |
| **Adafruit Fingerprint Sensor Library** | AS608/R307 driver (search + enroll) |
| **Adafruit PN532** | PN532 driver; I2C mode, raw UID |
| **Adafruit BusIO** | Dependency of the two Adafruit libs |
| **LiquidCrystal_I2C** | PCF8574 LCD driver — the installed fork uses `begin()` |
| **ArduinoJson v7** | (De)serialization for the store + API |

`WiFi`, `WebServer`, `Wire`, `LittleFS`, `FreeRTOS`, `NetworkClientSecure` are part of the
ESP32 core — do **not** install standalone copies. Telegram uses a hand-written HTTPS request
rather than `HTTPClient`, because a stale core-2.x `HTTPClient` in a sketchbook can shadow the
core one and break the build.

---

## 7. Build & upload (Arduino IDE)

1. **Tools → Board → ESP32 Arduino → "ESP32 Dev Module"**.
2. **Partition Scheme: "Default 4MB with spiffs (1.2MB APP / 1.5MB SPIFFS)"**. Keep it the
   same on every upload — a different scheme moves the data partition and wipes the voters.
3. Install the libraries from §6.
4. (Optional) edit `config.h`: `AP_SSID`, `AP_PASSWORD`, `ADMIN_PASSWORD`.
5. **Upload** (hold **BOOT** if it sticks at "Connecting…"). Serial Monitor @ **115200**.

First run: open the dashboard → add candidates → enroll voters → **Start** the election.

---

## 8. Updating from a browser (no Arduino IDE)

<https://julak13.github.io/SmartVotingMachine/> uses ESP Web Tools (Web Serial). Plug the board
into a laptop, open the page in **Chrome or Edge**, click *Connect & install*.
**Leave "Erase device" unticked** — erasing deletes all voters and candidates. The update
writes only the bootloader, partition table and app (offsets `0x1000`, `0x8000`, `0xE000`,
`0x10000`); the LittleFS data partition is never touched.

To publish a new version: build with the settings in §7, copy
`*.ino.bootloader.bin`, `*.ino.partitions.bin`, `*.ino.bin` and the core's
`tools/partitions/boot_app0.bin` into `docs/firmware/` as `bootloader.bin`, `partitions.bin`,
`firmware.bin`, `boot_app0.bin`, bump `version` in `docs/manifest.json` and `FW_VERSION` in
`config.h`, then push.

---

## 9. Memory & performance

* **Flash**: firmware + PROGMEM dashboard fit the 1.2 MB app partition. Voter/candidate JSON
  lives in the ~1.5 MB filesystem partition — 100 voters is a few KB.
* **RAM**: data in `std::vector`s (≈ 60 B/voter). Web responses build transient
  `JsonDocument`s. A Telegram send needs ~40 KB of heap for TLS while it runs.
* **AS608** stores up to 127 templates — `MAX_VOTERS`/`FP_MAX_ID` are bounded to match.
* WiFi/HTTP/Telegram run on **core 0** with the radio stack; voter hardware on **core 1**.
* NFC timing is v1.0's (250 ms polls, chip default search). See §10 for the rolled-back change.

## 10. Error-handling strategy

* **Sensors**: failures show on the LCD and in Settings → Device. A reader missing at boot is
  retried every 5 s; a working reader is never re-initialised.
* **NFC rollback (v1.1.1):** v1.1.0 bounded the PN532 card search (`setPassiveActivationRetries`)
  and ran a `getFirmwareVersion` health check every 5 s that hardware-reset the reader on failure.
  On the deployed unit the reader then reported "not responding", so v1.1.1 restores v1.0's NFC
  behaviour exactly. Suspected cause (unconfirmed on hardware): the library's
  `setPassiveActivationRetries()` never reads its reply, leaving host and chip out of step. Don't
  reintroduce either change without testing on the real reader with the Serial Monitor open.
* **Vote commit** is atomic under the store mutex and re-validates the voted flag. The voted flag
  is persisted before the count.
* **Enrollment** is fully reversible (the template is deleted again if a later step fails), refuses
  a finger or card that already belongs to someone (and names them), and a deleted voter's
  fingerprint is erased. Leftover templates from v1.0 are cleared once at boot.
* **Auth**: every `/api/*` route (except login) needs a per-session token; the WPA2 key is the
  outer boundary. Telegram secrets are never returned in full.
* **Telegram TLS** uses `setInsecure()`: someone controlling the router could capture the bot
  token and post as the bot. It cannot read or change votes. Pinning Telegram's CA would close
  that but fail silently when Telegram rotates its certificates.
* **Persistence**: writes are flushed to LittleFS immediately after each mutation.
