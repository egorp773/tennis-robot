/*
 * Tennis Robot вЂ” CALIBRATION FIRMWARE
 * For UWB range calibration in the two-anchor diagonal working-area model.
 *
 * РћС‚Р»РёС‡РёСЏ РѕС‚ РѕСЃРЅРѕРІРЅРѕР№ РїСЂРѕС€РёРІРєРё:
 *   - CAL_RAW is always enabled: sends RANGE,L/R,dist
 *   - No hoverboard or collector control
 *   - No MPU-6050
 *
 * РџРѕРґРєР»СЋС‡РµРЅРёРµ:
 *   UWB SPI CLK=18 MISO=19 MOSI=23 CSN=14 RSTN=13 IRQ=27
 *   Wi-Fi AP: configured by esp32_firmware/wifi_config.h when present.
 *   WebSocket port 81
 *
 * РљРѕРјРїРёР»СЏС†РёСЏ Рё Р·Р°РіСЂСѓР·РєР°:
 *   ./arduino-cli-bin/arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware/tennis_robot_cal/
 *   ./arduino-cli-bin/arduino-cli.exe upload -p COM3 --fqbn esp32:esp32:esp32 esp32_firmware/tennis_robot_cal/
 */

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <DW1000Ranging.h>
#include <DW1000.h>

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ PINS в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
#define UWB_CLK   18
#define UWB_MISO  19
#define UWB_MOSI  23
#define UWB_CSN   14
#define UWB_RSTN  13
#define UWB_IRQ   27
#define PIN_LED    2

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ CONSTANTS в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
#if __has_include("../wifi_config.h")
#include "../wifi_config.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID   "TennisRobot"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS   "CHANGE_ME_WIFI_PASS"
#endif

#define FW_NAME     "tennis_robot_cal"
#define FW_VERSION  "v3.1-working-area-uwb-cal"
#define FW_PURPOSE  "UWB calibration/range firmware, no motors"

// Playable court is inside the larger robot working area.
#define COURT_W           23.77f
#define COURT_H           10.97f
#define WORK_MARGIN_X      3.00f
#define WORK_MARGIN_Y      3.00f
#define WORK_X             (COURT_W + 2.0f * WORK_MARGIN_X)
#define WORK_Y             (COURT_H + 2.0f * WORK_MARGIN_Y)
#define COURT_ORIGIN_X     WORK_MARGIN_X
#define COURT_ORIGIN_Y     WORK_MARGIN_Y

// РђРґСЂРµСЃР° РјР°СЏРєРѕРІ вЂ” EUI СЃРёРјРјРµС‚СЂРёС‡РЅС‹Р№ ("9C:E2:...:9C:E2" Рё "9D:E2:...:9D:E2")
// РћР±Рµ РІРѕР·РјРѕР¶РЅС‹С… РёРЅС‚РµСЂРїСЂРµС‚Р°С†РёРё byte-order РїРѕРєСЂС‹С‚С‹ РјР°РєСЂРѕСЃР°РјРё:
#define IS_ANCHOR_L(addr) ((addr) == 0x9CE2u || (addr) == 0xE29Cu)
#define IS_ANCHOR_R(addr) ((addr) == 0x9DE2u || (addr) == 0xE29Du)

// Anchors are diagonal corners of the robot working area.
#define ANCHOR_L_X         0.0f
#define ANCHOR_L_Y         0.0f
#define ANCHOR_R_X         WORK_X
#define ANCHOR_R_Y         WORK_Y
#define ANCHOR_DIST        sqrtf((ANCHOR_R_X - ANCHOR_L_X) * (ANCHOR_R_X - ANCHOR_L_X) + \
                                 (ANCHOR_R_Y - ANCHOR_L_Y) * (ANCHOR_R_Y - ANCHOR_L_Y))

#define ANCHOR_TIMEOUT_MS  2000UL

// Tag EUI вЂ” РґРѕР»Р¶РµРЅ СЃРѕРІРїР°РґР°С‚СЊ СЃ РїСЂРѕС€РёРІРєРѕР№ СЂРѕР±РѕС‚Р°
#define TAG_EUI  "7D:00:22:EA:82:60:3B:9C"

// в”Ђв”Ђ Antenna delay в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
// Calibrated via calibration/analyze.py (was 16436).
// РџРѕСЃР»Рµ РїРѕРІС‚РѕСЂРЅРѕР№ РєР°Р»РёР±СЂРѕРІРєРё РЅР° РєРѕСЂС‚Рµ вЂ” РѕР±РЅРѕРІРёС‚СЊ Рё РїРµСЂРµРїСЂРѕС€РёС‚СЊ РІСЃС‘.
#define ANTENNA_DELAY  16327

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ GLOBALS в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
float rangeL = -1.0f, rangeR = -1.0f;
float uwbX = 0.0f, uwbY = 0.0f;
float prevUwbX = WORK_X * 0.5f;
float prevUwbY = WORK_Y * 0.5f;

unsigned long lastRangeL = 0, lastRangeR = 0;
unsigned long lastPosSend = 0, lastAnchorSend = 0;
bool anchorLOnline = false, anchorROnline = false;

WebSocketsServer wsServer(81);
uint8_t wsClient = 255;

unsigned long lastBlink = 0;
bool ledState = false;

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ TRILATERATION в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
// Two diagonal anchors produce two possible circle intersections.
// This calibration sketch has no app pose, so it chooses the candidate nearest
// to the previously accepted POS. Use RANGE values as the source of truth for
// antenna-delay calibration.
void trilaterate(float rL, float rR, float &ox, float &oy) {
  const float dx = ANCHOR_R_X - ANCHOR_L_X;
  const float dy = ANCHOR_R_Y - ANCHOR_L_Y;
  const float ex = dx / ANCHOR_DIST;
  const float ey = dy / ANCHOR_DIST;

  const float along = (rL * rL - rR * rR + ANCHOR_DIST * ANCHOR_DIST) / (2.0f * ANCHOR_DIST);
  float h2 = rL * rL - along * along;
  if (h2 < 0.0f) h2 = 0.0f;
  const float h = sqrtf(h2);

  const float bx = ANCHOR_L_X + along * ex;
  const float by = ANCHOR_L_Y + along * ey;
  const float px = -ey;
  const float py = ex;

  const float ax = bx + h * px;
  const float ay = by + h * py;
  const float bx2 = bx - h * px;
  const float by2 = by - h * py;

  const float da = hypotf(ax - prevUwbX, ay - prevUwbY);
  const float db = hypotf(bx2 - prevUwbX, by2 - prevUwbY);
  const float x = (da <= db) ? ax : bx2;
  const float y = (da <= db) ? ay : by2;

  ox = constrain(x, 0.0f, WORK_X);
  oy = constrain(y, 0.0f, WORK_Y);
  prevUwbX = ox;
  prevUwbY = oy;
}

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ WEBSOCKET в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
void wsSend(const String &msg) {
  if (wsClient != 255) wsServer.sendTXT(wsClient, msg.c_str());
  Serial.printf("[WS TX] %s\n", msg.c_str());
}

void wsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      wsClient = num;
      wsSend("STATE,CONNECTED");
      // Р’ СЂРµР¶РёРјРµ РєР°Р»РёР±СЂРѕРІРєРё СЃСЂР°Р·Сѓ РІРєР»СЋС‡Р°РµРј CAL_RAW
      wsSend("CAL_MODE,ON");
      Serial.printf("[WS] #%d connected\n", num);
      break;
    case WStype_DISCONNECTED:
      if (num == wsClient) wsClient = 255;
      break;
    case WStype_TEXT: {
      String cmd = String((char *)payload);
      cmd.trim();
      Serial.printf("[WS RX] %s\n", cmd.c_str());
      if (cmd == "PING") { wsSend("PONG"); }
      // CAL_RAW РІСЃРµРіРґР° РІРєР»СЋС‡С‘РЅ РІ СЌС‚РѕР№ РїСЂРѕС€РёРІРєРµ
      break;
    }
    default: break;
  }
}

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ UWB CALLBACKS в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
void uwbNewRange() {
  DW1000Device *dev = DW1000Ranging.getDistantDevice();
  if (!dev) return;

  float    dist = dev->getRange();
  uint16_t addr = dev->getShortAddress();
  unsigned long now = millis();

  // Debug: print actual address (РїРѕРјРѕР¶РµС‚ СѓРІРёРґРµС‚СЊ СЂРµР°Р»СЊРЅС‹Рµ Р·РЅР°С‡РµРЅРёСЏ)
  const char* side = IS_ANCHOR_L(addr) ? "L" : (IS_ANCHOR_R(addr) ? "R" : "?");
  Serial.printf("[UWB] addr=0x%04X side=%s dist=%.3f\n", addr, side, dist);

  if (IS_ANCHOR_L(addr))      { rangeL = dist; lastRangeL = now; }
  else if (IS_ANCHOR_R(addr)) { rangeR = dist; lastRangeR = now; }

  // Р’СЃРµРіРґР° С€Р»С‘Рј СЃС‹СЂС‹Рµ РґР°Р»СЊРЅРѕСЃС‚Рё вЂ” СЌС‚Рѕ СЂРµР¶РёРј РєР°Р»РёР±СЂРѕРІРєРё
  wsSend(String("RANGE,") + String(side) + "," + String(dist, 3));

  // РћР±РЅРѕРІР»СЏРµРј РїРѕР·РёС†РёСЋ РµСЃР»Рё РѕР±Р° РјР°СЏРєР° РІРёРґРЅС‹
  if (rangeL > 0.0f && rangeR > 0.0f &&
      (now - lastRangeL) < ANCHOR_TIMEOUT_MS &&
      (now - lastRangeR) < ANCHOR_TIMEOUT_MS) {
    trilaterate(rangeL, rangeR, uwbX, uwbY);
  }
}

void uwbNewDevice(DW1000Device *dev) {
  Serial.printf("[UWB] Anchor 0x%04X appeared\n", dev->getShortAddress());
}

void uwbInactive(DW1000Device *dev) {
  Serial.printf("[UWB] Anchor 0x%04X lost\n", dev->getShortAddress());
}

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ SETUP в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== %s %s ===\n", FW_NAME, FW_VERSION);
  Serial.printf("[FW] %s\n", FW_PURPOSE);
  Serial.printf("Working area: %.2f x %.2f m, playable court origin=(%.2f,%.2f)\n",
                WORK_X, WORK_Y, COURT_ORIGIN_X, COURT_ORIGIN_Y);
  Serial.printf("Anchors: L=(%.1f,%.1f)  R=(%.1f,%.1f)\n",
                ANCHOR_L_X, ANCHOR_L_Y, ANCHOR_R_X, ANCHOR_R_Y);
  Serial.printf("ANTENNA_DELAY = %d\n", ANTENNA_DELAY);

  pinMode(PIN_LED, OUTPUT);

  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  Serial.printf("[WIFI] AP %s  IP %s\n", WIFI_SSID,
                WiFi.softAPIP().toString().c_str());

  wsServer.begin();
  wsServer.onEvent(wsEvent);
  Serial.println("[WS] :81 ready");

  SPI.begin(UWB_CLK, UWB_MISO, UWB_MOSI, UWB_CSN);
  DW1000Ranging.initCommunication(UWB_RSTN, UWB_CSN, UWB_IRQ);
  DW1000.setAntennaDelay(ANTENNA_DELAY);
  DW1000Ranging.attachNewRange(uwbNewRange);
  DW1000Ranging.attachBlinkDevice(uwbNewDevice);
  DW1000Ranging.attachInactiveDevice(uwbInactive);
  DW1000Ranging.startAsTag(TAG_EUI, DW1000.MODE_LONGDATA_RANGE_LOWPOWER, false);
  Serial.println("[UWB] Tag started. Waiting for anchors...");
}

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ LOOP в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
void loop() {
  wsServer.loop();
  DW1000Ranging.loop();

  unsigned long now = millis();

  // POS РєР°Р¶РґС‹Рµ 500 РјСЃ
  if (now - lastPosSend >= 500UL) {
    lastPosSend = now;
    wsSend(String("POS,") + String(uwbX, 3) + "," + String(uwbY, 3));
  }

  // РЎС‚Р°С‚СѓСЃ РјР°СЏРєРѕРІ РєР°Р¶РґС‹Рµ 1 СЃ
  if (now - lastAnchorSend >= 1000UL) {
    lastAnchorSend = now;
    bool lOn = (rangeL >= 0.0f && (now - lastRangeL) < ANCHOR_TIMEOUT_MS);
    bool rOn = (rangeR >= 0.0f && (now - lastRangeR) < ANCHOR_TIMEOUT_MS);
    if (lOn != anchorLOnline) {
      anchorLOnline = lOn;
      wsSend(lOn ? "ANCHOR,L,online" : "ANCHOR,L,offline");
    }
    if (rOn != anchorROnline) {
      anchorROnline = rOn;
      wsSend(rOn ? "ANCHOR,R,online" : "ANCHOR,R,offline");
    }
    Serial.printf("[POS] x=%.3f y=%.3f  L=%.3f R=%.3f  anchors:%s%s\n",
                  uwbX, uwbY, rangeL, rangeR,
                  lOn ? "L" : "-", rOn ? "R" : "-");
  }

  // Heartbeat LED
  if (now - lastBlink >= 300UL) {
    lastBlink = now;
    ledState  = !ledState;
    digitalWrite(PIN_LED, ledState);
  }
}
