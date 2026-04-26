/*
 * Manual Control Firmware вЂ” ESP32 в†’ Hoverboard via WebSocket
 *
 * WiFi AP: configured by esp32_firmware/wifi_config.h when present.
 * WebSocket port 81
 *
 * Pins:
 *   UART2 RX=16, TX=17  вЂ” Hoverboard (115200, 0xABCD protocol)
 *   Pin 2               вЂ” Relay (attachment, active-HIGH)
 *
 * App в†’ ESP32 commands:
 *   M,left,right        вЂ” drive wheels; left/right = -100..100
 *   STOP                вЂ” smooth stop
 *   ATTACHMENT_ON/OFF   вЂ” relay
 *   PING                вЂ” keepalive
 *
 * ESP32 в†’ App:
 *   STATE,CONNECTED     вЂ” on connect
 *   PONG                вЂ” reply to PING
 *   BAT_PCT,n           вЂ” battery % every 10 s
 *
 * HOVERBOARD MAPPING (confirmed by hardware test):
 *   hoverSend(steer, speed)  в†’  steer = LEFT wheel, speed = RIGHT wheel
 *   (direct per-wheel control, NOT differential)
 */

#include <WiFi.h>
#include <WebSocketsServer.h>
#include <esp_wifi.h>

// в”Ђв”Ђв”Ђ Pins в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
#define HOVER_RX   16
#define HOVER_TX   17
#define RELAY_PIN   2

// в”Ђв”Ђв”Ђ WiFi в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
#if __has_include("../wifi_config.h")
#include "../wifi_config.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID  "TennisRobot"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS  "CHANGE_ME_WIFI_PASS"
#endif

#define FW_NAME     "manual_control"
#define FW_VERSION  "v1.1-manual-motor-test"
#define FW_PURPOSE  "manual motor and relay test firmware, no AUTO/UWB"

// в”Ђв”Ђв”Ђ Speed scaling в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
// App sends -100..100
// Input is reduced 2x before sending to hoverboard
#define INPUT_DIVIDER  2
#define SPEED_SCALE    2

// в”Ђв”Ђв”Ђ Smooth ramp в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
// Sent every 20 ms
#define ACCEL_STEP  3   // smooth acceleration
#define BRAKE_STEP  5   // smooth deceleration

// в”Ђв”Ђв”Ђ WebSocket heartbeat в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
#define WS_PING_INTERVAL_MS  5000
#define WS_PONG_TIMEOUT_MS   3000
#define WS_DISCONNECT_COUNT  2

// в”Ђв”Ђв”Ђ Battery range (36V Li-Ion hoverboard) в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
#define BAT_V_MIN  32.0f
#define BAT_V_MAX  42.0f

#pragma pack(push, 1)
struct HoverCmd {
  uint16_t start;
  int16_t  steer;   // LEFT wheel
  int16_t  speed;   // RIGHT wheel
  uint16_t checksum;
};

struct HoverFeedback {
  uint16_t start;
  int16_t  cmd1;
  int16_t  cmd2;
  int16_t  speedR;
  int16_t  speedL;
  int16_t  batVoltage;   // 0.1 V units
  int16_t  boardTemp;    // 0.1 В°C units
  uint16_t cmdLed;
  uint16_t checksum;
};
#pragma pack(pop)

WebSocketsServer ws(81);
uint8_t wsClient = 255;

// Desired wheel commands
int16_t targetLeft  = 0;
int16_t targetRight = 0;

// Current actually sent values
int16_t curLeft  = 0;
int16_t curRight = 0;

// Hoverboard feedback parser
uint8_t hoverBuf[sizeof(HoverFeedback)];
int hoverBufIdx = 0;
int batPercent = 100;

unsigned long lastHoverSend = 0;
unsigned long lastBatSend   = 0;

// в”Ђв”Ђв”Ђ Hoverboard send в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
void hoverSend(int16_t leftWheel, int16_t rightWheel) {
  HoverCmd cmd;
  cmd.start    = 0xABCD;
  cmd.steer    = leftWheel;
  cmd.speed    = rightWheel;
  cmd.checksum = cmd.start ^ (uint16_t)cmd.steer ^ (uint16_t)cmd.speed;
  Serial2.write((uint8_t*)&cmd, sizeof(cmd));
}

// РџР»Р°РІРЅР°СЏ РѕСЃС‚Р°РЅРѕРІРєР°
void hoverStop() {
  targetLeft  = 0;
  targetRight = 0;
}

// в”Ђв”Ђв”Ђ Hoverboard feedback reader в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
void readHoverFeedback() {
  while (Serial2.available()) {
    uint8_t b = (uint8_t)Serial2.read();

    // Sync on start bytes 0xCD 0xAB (little-endian 0xABCD)
    if (hoverBufIdx == 0) {
      if (b != 0xCD) continue;
    } else if (hoverBufIdx == 1) {
      if (b != 0xAB) {
        hoverBufIdx = 0;
        continue;
      }
    }

    hoverBuf[hoverBufIdx++] = b;

    if (hoverBufIdx >= (int)sizeof(HoverFeedback)) {
      hoverBufIdx = 0;
      HoverFeedback *fb = (HoverFeedback*)hoverBuf;

      uint16_t cs = fb->start ^ (uint16_t)fb->cmd1 ^ (uint16_t)fb->cmd2
                  ^ (uint16_t)fb->speedR ^ (uint16_t)fb->speedL
                  ^ (uint16_t)fb->batVoltage ^ (uint16_t)fb->boardTemp
                  ^ fb->cmdLed;

      if (cs == fb->checksum && fb->batVoltage > 0) {
        float v = fb->batVoltage * 0.1f;
        batPercent = (int)constrain(
          (v - BAT_V_MIN) / (BAT_V_MAX - BAT_V_MIN) * 100.0f,
          0.0f, 100.0f
        );
      }
    }
  }
}

// в”Ђв”Ђв”Ђ WebSocket helpers в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
void wsSend(const String &msg) {
  if (wsClient != 255) ws.sendTXT(wsClient, msg.c_str());
  Serial.println("TX: " + msg);
}

// в”Ђв”Ђв”Ђ Adaptive ramp helper в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
int16_t rampTowardAdaptive(int16_t cur, int16_t tgt) {
  if (cur == tgt) return cur;

  bool braking =
      (tgt == 0) ||
      ((cur > 0 && tgt < 0) || (cur < 0 && tgt > 0)) ||
      (abs(tgt) < abs(cur));

  int16_t step = braking ? BRAKE_STEP : ACCEL_STEP;

  if (cur < tgt) return (int16_t)min((int)(cur + step), (int)tgt);
  if (cur > tgt) return (int16_t)max((int)(cur - step), (int)tgt);

  return cur;
}

// в”Ђв”Ђв”Ђ Command handler в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
void handleCmd(const String &raw) {
  String cmd = raw;
  cmd.trim();
  Serial.println("RX: " + cmd);

  if (cmd == "PING") {
    wsSend("PONG");
    return;
  }

  if (cmd == "STOP") {
    hoverStop();
    return;
  }

  if (cmd == "ATTACHMENT_ON") {
    digitalWrite(RELAY_PIN, HIGH);
    return;
  }

  if (cmd == "ATTACHMENT_OFF") {
    digitalWrite(RELAY_PIN, LOW);
    return;
  }

  // M,left,right  вЂ” direct per-wheel command, -100..100
  if (cmd.startsWith("M,")) {
    int comma = cmd.indexOf(',', 2);
    if (comma > 0) {
      int L = constrain(cmd.substring(2, comma).toInt(), -100, 100);
      int R = constrain(cmd.substring(comma + 1).toInt(), -100, 100);

      // СѓРјРµРЅСЊС€Р°РµРј РІС…РѕРґРЅСѓСЋ СЃРєРѕСЂРѕСЃС‚СЊ РІ 2 СЂР°Р·Р°
      L = L / INPUT_DIVIDER;
      R = R / INPUT_DIVIDER;

      // steer = LEFT wheel, speed = RIGHT wheel
      targetLeft  = (int16_t)(L * SPEED_SCALE);
      targetRight = (int16_t)(R * SPEED_SCALE);

      Serial.printf("[CMD] targetLeft=%d targetRight=%d\n", targetLeft, targetRight);
    }
    return;
  }
}

// в”Ђв”Ђв”Ђ WebSocket event handler в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
void wsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      wsClient = num;
      wsSend("STATE,CONNECTED");
      Serial.printf("[WS] #%d connected\n", num);
      break;

    case WStype_DISCONNECTED:
      if (num == wsClient) {
        wsClient = 255;
        hoverStop();  // safety: smooth stop on disconnect
        Serial.printf("[WS] #%d disconnected вЂ” smooth stop\n", num);
      }
      break;

    case WStype_TEXT:
      handleCmd(String((char*)payload));
      break;

    case WStype_PING:
      Serial.println("[WS] ping received");
      break;

    case WStype_PONG:
      Serial.println("[WS] pong received");
      break;

    default:
      break;
  }
}

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
void setup() {
  Serial.begin(115200);
  Serial.printf("\n[BOOT] %s %s\n", FW_NAME, FW_VERSION);
  Serial.printf("[FW] %s\n", FW_PURPOSE);

  // Relay
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Hoverboard UART
  Serial2.begin(115200, SERIAL_8N1, HOVER_RX, HOVER_TX);
  Serial.println("[HOVER] UART2 ready");

  // WiFi AP
  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  esp_wifi_set_max_tx_power(78);  // ~19.5 dBm
  Serial.print("[WIFI] AP started: ");
  Serial.println(WiFi.softAPIP());

  // WebSocket
  ws.begin();
  ws.onEvent(wsEvent);
  ws.enableHeartbeat(WS_PING_INTERVAL_MS, WS_PONG_TIMEOUT_MS, WS_DISCONNECT_COUNT);
  Serial.println("[WS] Server on port 81");
}

void loop() {
  ws.loop();
  readHoverFeedback();

  // Send hoverboard command every 20 ms
  if (millis() - lastHoverSend >= 20) {
    lastHoverSend = millis();

    curLeft  = rampTowardAdaptive(curLeft, targetLeft);
    curRight = rampTowardAdaptive(curRight, targetRight);

    hoverSend(curLeft, curRight);
  }

  // Battery report every 10 s
  if (millis() - lastBatSend >= 10000) {
    lastBatSend = millis();
    wsSend("BAT_PCT," + String(batPercent));
  }
}
