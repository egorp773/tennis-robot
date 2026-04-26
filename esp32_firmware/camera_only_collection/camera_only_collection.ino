/*
 * Tennis Robot - Camera Only Collection Firmware
 *
 * Purpose:
 *   Test Pi camera guided ball approach and collector behavior without UWB anchors
 *   and without zone navigation.
 *
 * Moving hardware enabled:
 *   - Hoverboard motors from Pi TRACK commands and optional WebSocket manual M commands.
 *   - Collector relay on COLLECT.
 *   - No UWB.
 *   - No autonomous working-area navigation.
 *
 * Pi UART commands:
 *   TRACK:dx,size
 *   COLLECT
 *   SEARCH
 *   RETURN
 *
 * WebSocket/Serial commands:
 *   PING
 *   STOP
 *   MODE_CAMERA
 *   MODE_MANUAL
 *   M,left,right
 *   ATTACHMENT_ON
 *   ATTACHMENT_OFF
 *   RESET_COUNT
 *   STATUS
 *
 * Safety:
 *   - Motors stop if Pi UART is stale in camera mode.
 *   - STOP always stops motors and collector.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>

#define FW_NAME     "camera_only_collection"
#define FW_VERSION  "v1.0-pi-camera-no-uwb"
#define FW_PURPOSE  "Pi camera guided collection test without UWB navigation"

#if __has_include("../wifi_config.h")
#include "../wifi_config.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID   "TennisRobot"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS   "CHANGE_ME_WIFI_PASS"
#endif

#define PI_RX           4
#define PI_TX           5
#define HOVER_RX       16
#define HOVER_TX       17
#define RELAY_PIN       2
#define BASKET_PIN     34
#define BALL_SENS_PIN  35

#define TRACK_DEADBAND_PX      35
#define TRACK_STEER_GAIN       0.18f
#define TRACK_SPEED            28
#define SEARCH_STEER           24
#define COLLECT_RELAY_MS     2000UL
#define PI_STALE_TIMEOUT_MS  3000UL
#define BALL_DEBOUNCE_MS      120UL

WebSocketsServer ws(81);
HardwareSerial PiSerial(1);
uint8_t wsClient = 255;

volatile int ballsCollected = 0;
volatile unsigned long lastBallIsrMs = 0;

bool cameraMode = true;
bool collectorOn = false;
unsigned long collectEndMs = 0;
unsigned long lastPiMsgMs = 0;
unsigned long lastStatusMs = 0;

char piLine[96];
int piLineLen = 0;

#pragma pack(push, 1)
struct HoverMsg {
  uint16_t start;
  int16_t steer;
  int16_t speed;
  uint16_t checksum;
};
#pragma pack(pop)

void hoverSend(int16_t steer, int16_t speed) {
  HoverMsg m;
  m.start = 0xABCDu;
  m.steer = steer;
  m.speed = speed;
  m.checksum = (uint16_t)(m.start ^ (uint16_t)m.steer ^ (uint16_t)m.speed);
  Serial2.write((const uint8_t*)&m, sizeof(m));
}

void hoverStop() {
  hoverSend(0, 0);
}

void wsSend(const String& msg) {
  if (wsClient != 255) ws.sendTXT(wsClient, msg.c_str());
  Serial.printf("[TX] %s\n", msg.c_str());
}

void setCollector(bool enabled) {
  collectorOn = enabled;
  digitalWrite(RELAY_PIN, enabled ? HIGH : LOW);
  wsSend(enabled ? "COLLECTOR,ON" : "COLLECTOR,OFF");
}

bool basketPresent() {
  return digitalRead(BASKET_PIN) == LOW;
}

void stopAll(const char* reason) {
  hoverStop();
  setCollector(false);
  wsSend(String("STATE,STOPPED,") + reason);
}

void IRAM_ATTR onBall() {
  unsigned long now = millis();
  if (now - lastBallIsrMs < BALL_DEBOUNCE_MS) return;
  lastBallIsrMs = now;
  ballsCollected++;
}

void sendStatus() {
  wsSend(String("FW,") + FW_NAME + "," + FW_VERSION);
  wsSend(cameraMode ? "MODE,CAMERA" : "MODE,MANUAL");
  wsSend(String("BALLS_COUNT,") + ballsCollected);
  wsSend(basketPresent() ? "BASKET,PRESENT" : "BASKET,MISSING");
  wsSend(collectorOn ? "COLLECTOR,ON" : "COLLECTOR,OFF");
}

void startCollection() {
  hoverStop();
  setCollector(true);
  collectEndMs = millis() + COLLECT_RELAY_MS;
  wsSend("STATE,COLLECTING");
}

void handlePiLine(const char* line) {
  lastPiMsgMs = millis();
  Serial.printf("[PI RX] %s\n", line);
  if (!cameraMode) return;

  if (strncmp(line, "TRACK:", 6) == 0) {
    int dx = 0;
    int size = 0;
    sscanf(line + 6, "%d,%d", &dx, &size);
    int steer = 0;
    if (abs(dx) > TRACK_DEADBAND_PX) {
      steer = (int)constrain(dx * TRACK_STEER_GAIN, -60.0f, 60.0f);
    }
    hoverSend(steer, TRACK_SPEED);
    wsSend(String("STATE,TRACKING,") + dx + "," + size);
  } else if (strcmp(line, "COLLECT") == 0) {
    startCollection();
  } else if (strcmp(line, "SEARCH") == 0) {
    hoverSend(SEARCH_STEER, 0);
    wsSend("STATE,SEARCH");
  } else if (strcmp(line, "RETURN") == 0) {
    stopAll("RETURN_REQUESTED");
  }
}

void readPiSerial() {
  while (PiSerial.available()) {
    char c = (char)PiSerial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      piLine[piLineLen] = '\0';
      if (piLineLen > 0) handlePiLine(piLine);
      piLineLen = 0;
    } else if (piLineLen < (int)sizeof(piLine) - 1) {
      piLine[piLineLen++] = c;
    } else {
      piLineLen = 0;
    }
  }
}

void handleCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;
  Serial.printf("[RX] %s\n", cmd.c_str());

  if (cmd == "PING") {
    wsSend("PONG");
  } else if (cmd == "STOP") {
    stopAll("USER_STOP");
  } else if (cmd == "MODE_CAMERA") {
    cameraMode = true;
    hoverStop();
    wsSend("MODE,CAMERA");
  } else if (cmd == "MODE_MANUAL") {
    cameraMode = false;
    hoverStop();
    wsSend("MODE,MANUAL");
  } else if (cmd.startsWith("M,")) {
    cameraMode = false;
    int comma = cmd.indexOf(',', 2);
    if (comma > 0) {
      int left = cmd.substring(2, comma).toInt();
      int right = cmd.substring(comma + 1).toInt();
      left = constrain(left, -100, 100);
      right = constrain(right, -100, 100);
      hoverSend(left, right);
      wsSend("MODE,MANUAL");
    }
  } else if (cmd == "ATTACHMENT_ON") {
    setCollector(true);
  } else if (cmd == "ATTACHMENT_OFF") {
    setCollector(false);
  } else if (cmd == "RESET_COUNT") {
    noInterrupts();
    ballsCollected = 0;
    interrupts();
    wsSend("BALLS_COUNT,0");
  } else if (cmd == "STATUS") {
    sendStatus();
  } else {
    wsSend("ERROR,UNKNOWN_COMMAND");
  }
}

void wsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      wsClient = num;
      wsSend("STATE,CONNECTED");
      sendStatus();
      break;
    case WStype_DISCONNECTED:
      if (num == wsClient) wsClient = 255;
      stopAll("WS_DISCONNECTED");
      break;
    case WStype_TEXT:
      handleCommand(String((char*)payload));
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== %s %s ===\n", FW_NAME, FW_VERSION);
  Serial.printf("[FW] %s\n", FW_PURPOSE);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(BASKET_PIN, INPUT);
  pinMode(BALL_SENS_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(BALL_SENS_PIN), onBall, FALLING);

  Serial2.begin(115200, SERIAL_8N1, HOVER_RX, HOVER_TX);
  PiSerial.begin(115200, SERIAL_8N1, PI_RX, PI_TX);
  hoverStop();

  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  Serial.printf("[WIFI] AP %s IP %s\n", WIFI_SSID, WiFi.softAPIP().toString().c_str());

  ws.begin();
  ws.onEvent(wsEvent);
  Serial.println("[WS] :81 ready");
  sendStatus();
}

void loop() {
  ws.loop();
  readPiSerial();

  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    handleCommand(cmd);
  }

  if (collectEndMs != 0 && millis() >= collectEndMs) {
    collectEndMs = 0;
    setCollector(false);
    wsSend(String("BALLS_COUNT,") + ballsCollected);
    wsSend("STATE,CAMERA_READY");
  }

  if (cameraMode && lastPiMsgMs != 0 && millis() - lastPiMsgMs > PI_STALE_TIMEOUT_MS) {
    lastPiMsgMs = 0;
    hoverStop();
    wsSend("WARN,PI_TIMEOUT");
  }

  if (millis() - lastStatusMs >= 1000UL) {
    lastStatusMs = millis();
    sendStatus();
  }
}
