/*
 * UWB Anchor R — Tennis Robot System
 * Position: (29.770, 16.970) — robot working-area diagonal corner R
 *
 * SPI: CLK=18, MISO=19, MOSI=23, CSN=14, RSTN=13, IRQ=27
 * LED: pin 2 blinks 500 ms (heartbeat)
 *
 * Library: arduino-dw1000 (thotro)
 */

#include <SPI.h>
#include <DW1000Ranging.h>
#include <DW1000.h>

// ── Pins ─────────────────────────────────────────────────────────────────────
#define PIN_CLK   18
#define PIN_MISO  19
#define PIN_MOSI  23
#define PIN_CSN   14
#define PIN_RSTN  13
#define PIN_IRQ   27
#define PIN_LED    2

// ── Anchor identity ───────────────────────────────────────────────────────────
// EUI симметричный: первые И последние 2 байта = 9D:E2
// → short address = 0x9DE2 или 0xE29D в зависимости от библиотеки
// (в любом случае отличается от L: 9C:E2 / 0x9CE2 / 0xE29C)
#define ANCHOR_EUI  "9D:E2:5B:D5:A9:9A:9D:E2"
#define ANCHOR_ID   "R"
#define ANCHOR_X    29.770f
#define ANCHOR_Y    16.970f
#define FW_NAME     "anchor_R"
#define FW_VERSION  "v1.1-working-area-anchor"
#define FW_PURPOSE  "UWB anchor R at robot working-area diagonal corner"

// ── Antenna delay (DW1000 time units, 1 unit ≈ 15.65 ps) ─────────────────────
// Calibrated via calibration/analyze.py (was 16436)
#define ANTENNA_DELAY 16327

// ── Reply delay (µs) — РАЗНЫЕ у L и R чтобы избежать коллизии ────────────────
#define REPLY_DELAY_US 7000

// ── LED ───────────────────────────────────────────────────────────────────────
unsigned long lastBlink = 0;
bool ledState = false;

// ── DW1000 callbacks ─────────────────────────────────────────────────────────
void newRange() {
  DW1000Device *dev = DW1000Ranging.getDistantDevice();
  Serial.printf("[RANGE] Anchor %s: tag 0x%04X → %.3f m\n",
                ANCHOR_ID, dev->getShortAddress(), dev->getRange());
}

void newBlink(DW1000Device *device) {
  Serial.printf("[BLINK] 0x%04X joined\n", device->getShortAddress());
}

void inactiveDevice(DW1000Device *device) {
  Serial.printf("[INACTIVE] 0x%04X lost\n", device->getShortAddress());
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== %s %s ===\n", FW_NAME, FW_VERSION);
  Serial.printf("[FW] %s\n", FW_PURPOSE);
  Serial.printf("[ANCHOR] %s (%.1f, %.1f)\n", ANCHOR_ID, ANCHOR_X, ANCHOR_Y);

  pinMode(PIN_LED, OUTPUT);

  SPI.begin(PIN_CLK, PIN_MISO, PIN_MOSI, PIN_CSN);

  DW1000Ranging.initCommunication(PIN_RSTN, PIN_CSN, PIN_IRQ);
  DW1000.setAntennaDelay(ANTENNA_DELAY);
  DW1000Ranging.setReplyTime(REPLY_DELAY_US);   // L=3000, R=7000 → нет коллизии
  DW1000Ranging.attachNewRange(newRange);
  DW1000Ranging.attachBlinkDevice(newBlink);
  DW1000Ranging.attachInactiveDevice(inactiveDevice);

  // false = deterministic short address from EUI
  DW1000Ranging.startAsAnchor(ANCHOR_EUI, DW1000.MODE_LONGDATA_RANGE_LOWPOWER, false);
  Serial.printf("[EUI] %s  reply=%d µs\n", ANCHOR_EUI, REPLY_DELAY_US);

  Serial.println("Ready — waiting for robot tag...");
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
  DW1000Ranging.loop();

  // Heartbeat blink every 500 ms
  unsigned long now = millis();
  if (now - lastBlink >= 500UL) {
    lastBlink = now;
    ledState  = !ledState;
    digitalWrite(PIN_LED, ledState);
  }
}
