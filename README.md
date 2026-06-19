# Smart Electronic Voting Machine (ESP32)

A secure, **fully offline** electronic voting machine built on an **ESP32 DevKit V1**.
Two-factor voter authentication (NFC card **+** fingerprint), a voter-only LCD/button
interface, and a self-hosted, modern admin web dashboard. No cloud, no internet, no SD card,
no external database — everything is stored in the ESP32's internal flash and survives reboots.

---

## 1. System overview

```
                ESP32 DevKit V1
  ┌───────────────────────────────────────────┐
  │  Core 1  voterTask                         │      Core 0  webTask
  │  ┌──────────────────────────────┐          │   ┌────────────────────┐
  │  │ VotingController (state mc.)  │          │   │  WiFi SoftAP        │
  │  │  ├ NFCReader   (PN532, I2C)   │          │   │  WebServer  :80     │
  │  │  ├ FpSensor    (AS608, UART2) │          │   │  REST API + UI      │
  │  │  ├ Display     (LCD, I2C)     │          │   └─────────┬──────────┘
  │  │  └ Buttons     (UP/DOWN/OK)   │          │             │
  │  └──────────────┬───────────────┘          │             │
  │                 │       shared mutex-guarded global       │
  │                 └──────────────  Store  ─────────────────-┘
  │                         (LittleFS + ArduinoJson)          │
  └───────────────────────────────────────────┘
```

* **One global `Store`** is the single source of truth. Both tasks talk to it and to
  nothing else — no queues, no per-feature locks, one recursive mutex.
* The **voter task** owns every piece of voter-facing hardware. The **web task** owns
  WiFi + HTTP. They never touch each other's peripherals.
* **Fingerprint enrollment** is *requested* by the dashboard and *executed* by the voter
  task (it owns the sensor); progress is published back through a mutex-guarded
  `EnrollState` that the dashboard polls. This keeps the AS608 single-owner and race-free.

---

## 2. Project structure

| File | Responsibility |
|------|----------------|
| `SmartVotingMachine.ino` | Boot, peripheral init, launches the two FreeRTOS tasks |
| `config.h` | All pins, AP/admin credentials, limits, timings (change values **here**) |
| `models.h` | `Candidate`, `Voter`, `ElectionStatus` data structures |
| `Store.h/.cpp` | RAM model + LittleFS/JSON persistence, mutex, vote-casting logic |
| `Display.h/.cpp` | 16×2 I2C LCD wrapper (voter screens) |
| `Buttons.h` | Debounced active-LOW button with edge detection |
| `FpSensor.h/.cpp` | AS608 fingerprint: 1:N search + enrollment primitives |
| `NFCReader.h/.cpp` | PN532 UID reader (I2C) |
| `Feedback.h/.cpp` | Buzzer + green/red status LEDs (voter feedback) |
| `VotingController.h/.cpp` | Voter state machine **and** enrollment workflow |
| `AdminServer.h/.cpp` | SoftAP + REST API + serves the dashboard |
| `web_assets.h` | The entire dashboard (HTML/CSS/JS) in PROGMEM |

---

## 3. Pin mapping & wiring

| ESP32 GPIO | Connects to | Notes |
|-----------:|-------------|-------|
| GPIO21 | SDA — LCD **and** PN532 | shared I2C bus |
| GPIO22 | SCL — LCD **and** PN532 | shared I2C bus |
| GPIO16 (RX2) | AS608 **TX** | fingerprint UART |
| GPIO17 (TX2) | AS608 **RX** | fingerprint UART |
| GPIO4 | PN532 IRQ | |
| GPIO5 | PN532 RST | |
| GPIO25 | Button **UP** | other side → GND |
| GPIO26 | Button **DOWN** | other side → GND |
| GPIO27 | Button **OK** | other side → GND |
| GPIO13 | Buzzer (+) | active buzzer module; (−) → GND |
| GPIO32 | Green LED (+) | via 220–330 Ω resistor → GND |
| GPIO33 | Red LED (+) | via 220–330 Ω resistor → GND |
| 3V3 | AS608 Vcc, PN532 Vcc | |
| 5V (VIN) | LCD backpack Vcc | LCD I2C is 5 V tolerant on data lines |
| GND | common ground for **all** modules | |

Wiring notes:
* Buttons use the ESP32 internal pull-ups (`INPUT_PULLUP`) — wire each button directly
  between its GPIO and GND, no external resistors needed.
* Set the PN532 DIP switches to **I2C** mode. Default I2C address `0x24`; LCD is `0x27`
  (or `0x3F` — change `LCD_ADDR` in `config.h` if your backpack differs).
* Use an **active** buzzer module (sounds on DC). Each LED needs a series resistor.
  If you must use a passive piezo, set `BUZZER_ACTIVE_HIGH` aside and drive it with
  `tone()` — but active modules are recommended and need no extra code.
* Both analog-free here, but keep the AS608 on a stable 3V3 supply; brown-outs corrupt UART.

---

## 4. Voter flow (LCD)

```
Tap NFC Card  →  Place Finger  →  Accredited / Press OK
   →  > Candidate A / Candidate B  (UP/DOWN to move, OK to pick)
   →  Confirm vote? <name>  (OK = yes)
   →  Vote Recorded / Thank You
```
Rejections: `Card Not Known`, `Finger Mismatch`, `Already Voted / Access Denied`,
`Election Closed`. Both factors (NFC UID **and** fingerprint) must resolve to the **same**
voter record, and the voted-flag is re-checked atomically under lock at the moment the
ballot is committed.

---

## 5. Admin dashboard

1. Connect a phone/laptop to WiFi **`VotingMachine`** (password `vote12345`).
2. Open **http://192.168.4.1** in a browser.
3. Sign in with the admin password (`admin123`).

Tabs: **Overview** (registered / cast / remaining / status), **Voters** (enroll, view,
delete, voted status), **Candidates** (add / edit / delete), **Results** (live bars +
percentages), **Control** (start / stop / reset). Dark, responsive, mobile-first SaaS UI.

### Web API quick reference
All `/api/*` routes except `/api/login` require `?t=<token>` (obtained from login).

| Method | Route | Purpose |
|--------|-------|---------|
| GET | `/` | dashboard HTML |
| POST | `/api/login` | `{password}` → `{ok, token}` |
| GET | `/api/overview` | counts + status |
| GET / POST | `/api/candidates` | list / add `{name}` |
| POST | `/api/candidates/edit` | `{id, name}` |
| POST | `/api/candidates/delete` | `{id}` |
| GET | `/api/voters` | list voters |
| POST | `/api/voters/delete` | `{fid}` |
| POST | `/api/voters/enroll` | `{name}` → starts device enrollment |
| GET | `/api/voters/enroll/status` | enrollment progress (polled) |
| POST | `/api/voters/enroll/cancel` | abort enrollment |
| GET | `/api/results` | totals + per-candidate votes |
| POST | `/api/election/start\|stop\|reset` | election control |

---

## 6. Libraries (install via Arduino Library Manager)

| Library | Why |
|---------|-----|
| **ESP32 board package 3.x** (Espressif) | Core, WiFi, WebServer, LittleFS, FreeRTOS all bundled |
| **Adafruit Fingerprint Sensor Library** | Mature, well-tested AS608/R307 driver (search + enroll) |
| **Adafruit PN532** | De-facto PN532 driver; clean I2C mode, returns raw UID |
| **Adafruit BusIO** | Dependency of the two Adafruit libs |
| **LiquidCrystal_I2C** | Tiny, ubiquitous PCF8574 LCD driver — this fork uses `begin()` |
| **ArduinoJson v7** (Benoît Blanchon) | Robust (de)serialization for the LittleFS store + API |

`WiFi`, `WebServer`, `Wire`, `LittleFS`, `FreeRTOS` are part of the ESP32 core — do **not**
install standalone copies (a stray standalone `WebServer` will shadow the core one).

---

## 7. Setup & upload (Arduino IDE)

1. **Tools → Board → ESP32 Arduino → "ESP32 Dev Module"**.
2. Set **Partition Scheme** to one that includes a filesystem, e.g.
   *"Default 4MB with spiffs (1.2MB APP / 1.5MB SPIFFS)"* — LittleFS uses this partition.
   `LittleFS.begin(true)` formats it automatically on first boot, so **no separate
   filesystem upload is needed**.
3. Install the libraries from §6.
4. (Optional) edit `config.h`: `AP_SSID`, `AP_PASSWORD`, `ADMIN_PASSWORD`.
5. **Upload**. Open Serial Monitor @ **115200** to see the AP IP (`192.168.4.1`).

First run: open the dashboard → add candidates → enroll voters → **Start** the election.

---

## 8. Memory considerations

* **Flash**: firmware + PROGMEM dashboard fit comfortably in the 1.2 MB app partition.
  Voter/candidate JSON lives in the ~1.5 MB filesystem partition — 100 voters is a few KB.
* **RAM**: ESP32 has ~320 KB. Data is held in `std::vector`s (≈ 60 B/voter → ~6 KB for 100).
  Web responses build transient `JsonDocument`s on the heap and free immediately.
* **Task stacks**: 8 KB each; the web task needs headroom for TLS-free HTTP + JSON.
* **AS608** stores up to 127 templates — `MAX_VOTERS`/`FP_MAX_ID` are bounded to match.

## 9. Performance considerations

* Voter loop is event-driven: it blocks up to 250 ms per NFC poll, yielding instantly when
  idle, so the deterministic voting path is never starved by HTTP traffic on the other core.
* WiFi/HTTP runs on **core 0** (with the radio stack); voter hardware on **core 1** — radio
  jitter can't delay the ballot flow.
* `vTaskDelay` (not `delay`) is used throughout so the scheduler stays cooperative.

## 10. Error-handling strategy

* **Sensor init** failures are reported on the LCD at boot and the affected feature degrades
  gracefully (enrollment refuses to start if a sensor is offline, with a dashboard message).
* **Vote commit** is atomic under the store mutex and re-validates the voted flag, so a
  card replay or a race cannot double-count. The voted flag is persisted before the count.
* **Enrollment** is fully reversible: if the NFC step fails or is cancelled after the
  template was stored, the fingerprint template is deleted again to avoid orphans.
* **Auth**: every `/api/*` route (except login) is gated by a per-session random token; the
  WiFi WPA2 key is the outer boundary.
* **Persistence**: writes are flushed to LittleFS immediately after each mutation, so an
  unexpected power loss never loses more than the in-flight operation.
