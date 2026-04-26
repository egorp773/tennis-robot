/*
 * Tennis Robot - Collector Test Firmware
 *
 * Purpose:
 *   Isolated collector relay, basket sensor, ball passage sensor, and ball count test.
 *
 * Moving hardware enabled:
 *   - Collector relay only.
 *   - Hoverboard motors are not initialized.
 *   - UWB is not initialized.
 *
 * Commands over USB Serial and WebSocket:
 *   PING
 *   STOP
 *   ATTACHMENT_ON
 *   ATTACHMENT_OFF
 *   COLLECT_ON
 *   COLLECT_OFF
 *   PULSE:<ms>
 *   RESET_COUNT
 *   STATUS
 *
 * Telemetry:
 *   FW,<name>,<version>
 *   STATE,CONNECTED
 *   COLLECTOR,ON/OFF
 *   BALLS_COUNT,n
 *   BASKET,PRESENT/MISSING
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>

#define FW_NAME     "collector_test"
#define FW_VERSION  "v1.0-collector-sensor-test"
#define FW_PURPOSE  "collector relay, basket sensor, and ball counter test; no motors/UWB"

#if __has_include("../wifi_config.h")
#include "../wifi_config.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID   "TennisRobot"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS   "CHANGE_ME_WIFI_PASS"
#endif

#define RELAY_PIN       2
#define BASKET_PIN     34
#define BALL_SENS_PIN  35

#define BALL_DEBOUNCE_MS 120UL

WebSocketsServer ws(81);
uint8_t wsClient = 255;

volatile int ballsCollected = 0;
volatile unsigned long lastBallIsrMs = 0;

bool collectorOn = false;
unsigned long pulseEndMs = 0;
unsigned long lastStatusMs = 0;

void wsSend(const String& msg) {
  if (wsClient != 255) ws.sendTXT(wsClient, msg.c_str());
  Serial.printf("[TX] %s\n", msg.c_str());
}

bool basketPresent() {
  return digitalRead(BASKET_PIN) == LOW;
}

void setCollector(bool enabled) {
  collectorOn = enabled;
  digitalWrite(RELAY_PIN, enabled ? HIGH : LOW);
  wsSend(enabled ? "COLLECTOR,ON" : "COLLECTOR,OFF");
}

void IRAM_ATTR onBall() {
  unsigned long now = millis();
  if (now - lastBallIsrMs < BALL_DEBOUNCE_MS) return;
  lastBallIsrMs = now;
  ballsCollected++;
}

void sendStatus() {
  wsSend(String("FW,") + FW_NAME + "," + FW_VERSION);
  wsSend(String("BALLS_COUNT,") + ballsCollected);
  wsSend(basketPresent() ? "BASKET,PRESENT" : "BASKET,MISSING");
  wsSend(collectorOn ? "COLLECTOR,ON" : "COLLECTOR,OFF");
}

void handleCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;
  Serial.printf("[RX] %s\n", cmd.c_str());

  if (cmd == "PING") {
    wsSend("PONG");
  } else if (cmd == "STOP" || cmd == "ATTACHMENT_OFF" || cmd == "COLLECT_OFF") {
    pulseEndMs = 0;
    setCollector(false);
  } else if (cmd == "ATTACHMENT_ON" || cmd == "COLLECT_ON") {
    pulseEndMs = 0;
    setCollector(true);
  } else if (cmd.startsWith("PULSE:")) {
    int ms = cmd.substring(6).toInt();
    if (ms < 100) ms = 100;
    if (ms > 10000) ms = 10000;
    setCollector(true);
    pulseEndMs = millis() + (unsigned long)ms;
    wsSend(String("PULSE_MS,") + ms);
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
      setCollector(false);
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

  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  Serial.printf("[WIFI] AP %s IP %s\n", WIFI_SSID, WiFi.softAPIP().toString().c_str());

  ws.begin();
  ws.onEvent(wsEvent);
  Serial.println("[WS] :81 ready");
  sendStatus();
}

void loop() {
  ws.loop();

  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    handleCommand(cmd);
  }

  if (pulseEndMs != 0 && millis() >= pulseEndMs) {
    pulseEndMs = 0;
    setCollector(false);
  }

  if (millis() - lastStatusMs >= 1000UL) {
    lastStatusMs = millis();
    sendStatus();
  }
}
