#pragma once
// =====================================================================
//  config.h  -  Central configuration for the Smart Voting Machine
//  All pins, credentials, limits and timings live here. Change values
//  in this file only - never hard-code them inside the .cpp modules.
// =====================================================================
#include <Arduino.h>

#define FW_NAME     "SmartVotingMachine"
#define FW_VERSION  "1.1.1"

// ---------------------------------------------------------------------
//  WiFi Access Point  (the machine is its OWN network - always on)
// ---------------------------------------------------------------------
#define AP_SSID         "VotingMachine"   // network the admin connects to
#define AP_PASSWORD     "vote12345"       // WPA2 key, MUST be >= 8 chars
#define AP_CHANNEL      6
#define AP_MAX_CLIENTS  4

// Dashboard login (the AP key is the first line of defence, this is the
// second). Change before deploying.
#define ADMIN_PASSWORD  "admin123"

// ---------------------------------------------------------------------
//  I2C bus  -  shared by the 16x2 LCD and the PN532 NFC reader
// ---------------------------------------------------------------------
#define I2C_SDA   21
#define I2C_SCL   22
#define I2C_FREQ  100000UL      // 100 kHz - safe with PN532 + LCD backpack

#define LCD_ADDR  0x27          // common PCF8574 backpack address (or 0x3F)
#define LCD_COLS  16
#define LCD_ROWS  2

// ---------------------------------------------------------------------
//  PN532 NFC reader (I2C mode)
// ---------------------------------------------------------------------
#define PN532_IRQ    4          // interrupt line
#define PN532_RESET  5          // reset line

// ---------------------------------------------------------------------
//  AS608 fingerprint sensor (UART2 / Serial2)
// ---------------------------------------------------------------------
#define FP_RX_PIN   16          // ESP32 RX2  <-- sensor TX (white/green)
#define FP_TX_PIN   17          // ESP32 TX2  --> sensor RX (yellow)
#define FP_BAUD     57600UL

// ---------------------------------------------------------------------
//  Push buttons (wired to GND, using internal pull-ups, active LOW)
// ---------------------------------------------------------------------
#define BTN_UP    25
#define BTN_DOWN  26
#define BTN_OK    27
#define BTN_DEBOUNCE_MS 40

// ---------------------------------------------------------------------
//  Voter feedback  -  buzzer + two status LEDs (active task owns these)
// ---------------------------------------------------------------------
#define BUZZER_PIN   23         // active buzzer module (DC, no tone needed)
#define LED_GREEN    32         // success / accredited
#define LED_RED      33         // denied / error
// 1 = sounds when pin is HIGH (high-level trigger / bare active buzzer).
// 0 = sounds when pin is LOW  (low-level trigger module) <-- this build.
#define BUZZER_ACTIVE_HIGH 0
#define BEEP_SHORT_MS  70
#define BEEP_LONG_MS   400
#define BEEP_GAP_MS    60

// ---------------------------------------------------------------------
//  Capacity limits  (sized for ESP32 RAM / AS608 template store)
// ---------------------------------------------------------------------
#define MAX_VOTERS      100     // AS608 stores up to 127 templates
#define MAX_CANDIDATES  16
#define MAX_NAME_LEN    32
#define FP_MIN_ID       1
#define FP_MAX_ID       127     // valid AS608 template id range

// ---------------------------------------------------------------------
//  Behaviour / timing
// ---------------------------------------------------------------------
#define FP_MIN_CONFIDENCE      30    // reject weak fingerprint matches
#define SCREEN_MSG_MS          2200  // how long status screens stay up
#define ENROLL_NFC_TIMEOUT_MS  15000 // wait for card during enrollment
#define ENROLL_FP_TIMEOUT_MS   15000 // wait for finger during enrollment

// NFC - same timing as v1.0, which worked on the deployed reader.
// (v1.1.0 bounded the card search and added periodic health checks;
// that left the reader unresponsive, so it was rolled back in v1.1.1.)
#define NFC_READ_TIMEOUT_MS    250   // per poll while waiting for a card
#define NFC_RETRY_MS           5000  // retry a reader that was not found
#define CARD_REPEAT_IGNORE_MS  2500  // a card left on the reader is ignored
                                     // until it has been lifted this long

// Voter session
#define ACCREDIT_WAIT_MS       20000 // time to press OK after accreditation
#define CONFIRM_WAIT_MS        15000 // time to confirm before returning to list
#define SESSION_IDLE_MS        30000 // no button press for this long on the
                                     // candidate list -> session cancelled
#define FP_VERIFY_TIMEOUT_MS   15000 // per attempt, while waiting for a finger
#define FP_MAX_ATTEMPTS        3     // clean, non-matching reads before reject

// Optional internet uplink - used ONLY to send results to Telegram.
// Voting never depends on it; the access point above stays up regardless.
#define STA_CONNECT_TIMEOUT_MS 20000   // give up one connection attempt
#define STA_RETRY_MS           300000  // then retry this often (5 min) so a
                                       // missing router can't keep the radio
                                       // scanning and disturb the admin AP
#define TG_RETRY_MS            60000   // re-send undelivered final results
#define TG_MAX_RETRIES         30

// LittleFS data files
#define FILE_CANDIDATES  "/candidates.json"
#define FILE_VOTERS      "/voters.json"
#define FILE_STATE       "/state.json"
#define FILE_UPLINK      "/uplink.json"
