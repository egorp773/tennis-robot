/*
 * Tennis Robot вЂ” Main ESP32 Firmware v4
 *
 * UART1  RX=4,  TX=5    вЂ” Raspberry Pi (ball detection commands)
 * UART2  RX=16, TX=17   вЂ” Hoverboard (0xABCD protocol)
 * Pin 2                  вЂ” Relay collector, active-HIGH
 * Pin 34                 вЂ” Basket sensor (INPUT, LOW=basket present)
 * Pin 35                 вЂ” Ball passage sensor (FALLING в†’ ballsCollected++)
 * UWB SPI CLK=18 MISO=19 MOSI=23 CSN=14 RSTN=13 IRQ=27
 * I2C  SDA=21, SCL=22   вЂ” MPU-6050 (heading)
 *
 * WiFi AP: configured by esp32_firmware/wifi_config.h when present.
 * WebSocket port 81
 *
 * CALIBRATE:heading_deg:x:y вЂ” set robot pose in robot working-area metres.
 *   Anchors are diagonal corners of the robot working area. Calibration picks
 *   the UWB candidate closest to the app pose and seeds IMU heading.
 */

#include <Arduino.h>
#include <float.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <DW1000Ranging.h>
#include <DW1000.h>
#if __has_include(<MPU6050_light.h>)
#include <MPU6050_light.h>
#define HAS_MPU6050_LIGHT 1
#else
#define HAS_MPU6050_LIGHT 0
#endif

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ PINS в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
#define PI_RX           4
#define PI_TX           5
#define HOVER_RX       16
#define HOVER_TX       17
#define RELAY_PIN       2
#define BASKET_PIN     34
#define BALL_SENS_PIN  35
#define UWB_CLK        18
#define UWB_MISO       19
#define UWB_MOSI       23
#define UWB_CSN        14
#define UWB_RSTN       13
#define UWB_IRQ        27
#define IMU_SDA        21
#define IMU_SCL        22

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ CONSTANTS в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
#if __has_include("../wifi_config.h")
#include "../wifi_config.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID         "TennisRobot"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS         "CHANGE_ME_WIFI_PASS"
#endif

#define FW_NAME           "tennis_robot"
#define FW_VERSION        "v4.1-working-area-uwb"
#define FW_PURPOSE        "full integrated robot firmware"

#define COURT_W           23.77f   // playable court length along X, m
#define COURT_H           10.97f   // playable court width along Y, m
#define WORK_MARGIN_X      3.00f   // working area extends beyond playable court
#define WORK_MARGIN_Y      3.00f
#define WORK_X             (COURT_W + 2.0f * WORK_MARGIN_X)
#define WORK_Y             (COURT_H + 2.0f * WORK_MARGIN_Y)
#define COURT_ORIGIN_X     WORK_MARGIN_X
#define COURT_ORIGIN_Y     WORK_MARGIN_Y

// Anchor short addresses вЂ” EUI "9C:E2:...:9C:E2" Рё "9D:E2:...:9D:E2"
// РћР±Р° РІРѕР·РјРѕР¶РЅС‹С… byte-order РїРѕРєСЂС‹С‚С‹:
#define IS_ANCHOR_L(addr) ((addr) == 0x9CE2u || (addr) == 0xE29Cu)
#define IS_ANCHOR_R(addr) ((addr) == 0x9DE2u || (addr) == 0xE29Du)
// Anchors are fixed diagonal corners of the robot working area.
// Playable court is inside this rectangle at (COURT_ORIGIN_X, COURT_ORIGIN_Y).
#define ANCHOR_L_X         0.0f
#define ANCHOR_L_Y         0.0f
#define ANCHOR_R_X         WORK_X
#define ANCHOR_R_Y         WORK_Y
#define ANCHOR_DIST        sqrtf((ANCHOR_R_X - ANCHOR_L_X) * (ANCHOR_R_X - ANCHOR_L_X) + \
                                 (ANCHOR_R_Y - ANCHOR_L_Y) * (ANCHOR_R_Y - ANCHOR_L_Y))
#define ANCHOR_TIMEOUT_MS 2000UL

#define TAG_EUI           "7D:00:22:EA:82:60:3B:9C"

// в”Ђв”Ђ Antenna delay (DW1000 time units, 1 unit в‰€ 15.65 ps) в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
// Calibrated via calibration/analyze.py (was 16436)
#define ANTENNA_DELAY     16327

#define ARRIVE_DIST         0.3f
#define ZONE_ARRIVE_DIST    1.0f    // radius to start zone sweep
#define MAX_BALLS           60
#define MAX_ZONES           10
#define COLLECT_RELAY_MS    2000UL
#define UNLOAD_SETTLE_MS    5000UL

// Grid sweep вЂ” 3Г—3 СЃРµС‚РєР° С‚РѕС‡РµРє РІРЅСѓС‚СЂРё Р·РѕРЅС‹
#define SWEEP_COLS          3
#define SWEEP_ROWS          3
#define SWEEP_SPACING       1.5f    // СЂР°СЃСЃС‚РѕСЏРЅРёРµ РјРµР¶РґСѓ С‚РѕС‡РєР°РјРё СЃРµС‚РєРё, Рј
#define SWEEP_SPIN_MS       1500UL  // РІСЂРµРјСЏ РІСЂР°С‰РµРЅРёСЏ РЅР° РєР°Р¶РґРѕР№ С‚РѕС‡РєРµ, РјСЃ
#define MAX_SWEEP_PTS       (SWEEP_ROWS * SWEEP_COLS)  // 9

// UWB watchdog вЂ” РµСЃР»Рё РѕР±Р° СЏРєРѕСЂСЏ РјРѕР»С‡Р°С‚ РґРѕР»СЊС€Рµ СЌС‚РѕРіРѕ, СЃС‚РѕРї
#define UWB_LOST_TIMEOUT_MS 5000UL

#define PI_UART_TIMEOUT_MS  5000UL   // Pi silence in AUTO в†’ warn/stop
#define NAV_STUCK_MS        60000UL  // navigating to same target > 60s в†’ skip
#define NAV_STUCK_RETURN_MS 120000UL // returning > 120s в†’ emergency stop
#define DEMO_TOTAL_TIMEOUT_MS 45000UL
#define DEMO_NAV_TIMEOUT_MS   25000UL
#define CAL_MISMATCH_M      1.0f
#define MAX_UWB_JUMP_M      1.5f
#define MAX_UWB_REJECTS     4
#define BOUNDARY_SLOW_M     0.7f

// Heading PD gains
#define KP_HEADING        80.0f   // proportional steer gain (В°в†’steer units)
#define MAX_STEER         100
#define NAV_SPEED         50      // forward speed while navigating
#define NAV_SPEED_SLOW    30      // slow down near target

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ HOVERBOARD в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
#pragma pack(push, 1)
struct HoverMsg {
  uint16_t start;
  int16_t  steer;
  int16_t  speed;
  uint16_t checksum;
};
#pragma pack(pop)

void hoverSend(int16_t steer, int16_t speed) {
  HoverMsg m;
  m.start    = 0xABCDu;
  m.steer    = steer;
  m.speed    = speed;
  m.checksum = (uint16_t)(m.start ^ (uint16_t)m.steer ^ (uint16_t)m.speed);
  Serial2.write((const uint8_t *)&m, sizeof(m));
}

void hoverStop() { hoverSend(0, 0); }

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ STATE MACHINE в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
enum MainState { ST_MANUAL, ST_AUTO, ST_UNLOADING };
enum AutoSub   { AS_SEARCH, AS_TRACKING, AS_COLLECTING, AS_RETURNING };
enum SearchSub { SS_NAVIGATE, SS_SWEEP };
struct Point2f { float x, y; };

bool isInsideWork(float x, float y);
bool hasFreshUwb(unsigned long now);
void safetyStop(const char *reason, bool invalidatePose = false);
void wsSend(const String &msg);
float dist2f(Point2f a, Point2f b);
bool trilaterateCandidates(float rL, float rR, Point2f &a, Point2f &b);
bool acceptPose(Point2f p, unsigned long now);
void reportModeToPi(AutoSub sub);
void seedSafetyTestAutoState();
bool startAutoFromZones(bool requestDemoMode);

MainState mainState = ST_MANUAL;
AutoSub   autoSub   = AS_SEARCH;
SearchSub searchSub = SS_NAVIGATE;
AutoSub   lastReportedSub = AS_SEARCH;

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ GLOBALS в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
// UWB
float    uwbX = 0.0f, uwbY = 0.0f;
float    rangeL = -1.0f, rangeR = -1.0f;
unsigned long lastRangeL = 0, lastRangeR = 0;
bool     anchorLOnline = false, anchorROnline = false;
Point2f  candA = {0.0f, 0.0f}, candB = {0.0f, 0.0f};
bool     haveCandidates = false;
bool     haveAcceptedPose = false;
unsigned long lastPoseMs = 0;
int      uwbRejects = 0;

// Home
float homeX = 0.0f, homeY = 0.0f;

// Ball collection
volatile int ballsCollected = 0;
int      maxBalls = MAX_BALLS;

// Timers
unsigned long collectStartMs  = 0;
unsigned long unloadWaitMs    = 0;
unsigned long lastPosSend     = 0;
unsigned long lastBatSend     = 0;
unsigned long lastAnchorSend  = 0;
unsigned long lastUwbCheck    = 0;
bool          unloadBasketOut = false;
unsigned long lastPiMsgMs    = 0;     // last any message received from Pi
unsigned long navStartMs     = 0;     // when current navigation leg started
unsigned long lastBallsSend  = 0;     // for periodic ball count send
int16_t       navSpeed       = NAV_SPEED;       // adjustable via SET_SPEED
int16_t       navSpeedSlow   = NAV_SPEED_SLOW;  // adjustable via SET_SPEED
bool          demoMode       = false;
unsigned long demoStartMs    = 0;

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ SWEEP в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
Point2f       sweepPts[MAX_SWEEP_PTS];
int           sweepPtCount   = 0;
int           sweepPtIdx     = 0;
bool          sweepSpinning  = false;
unsigned long sweepSpinStart = 0;

// Р“РµРЅРµСЂРёСЂСѓРµС‚ 3Г—3 СЃРµС‚РєСѓ С‚РѕС‡РµРє РІРѕРєСЂСѓРі С†РµРЅС‚СЂР° Р·РѕРЅС‹ (cx,cy), СЃРµСЂРїР°РЅС‚РёРЅ
void generateSweepGrid(float cx, float cy) {
  sweepPtIdx  = 0;
  sweepPtCount = 0;
  int k = 0;
  for (int r = 0; r < SWEEP_ROWS; r++) {
    float fy = cy + (r - SWEEP_ROWS / 2) * SWEEP_SPACING;
    fy = constrain(fy, 0.0f, WORK_Y);
    for (int c = 0; c < SWEEP_COLS; c++) {
      // РЎРµСЂРїР°РЅС‚РёРЅ: С‡С‘С‚РЅС‹Рµ СЂСЏРґС‹ Р»РµРІРѕв†’РїСЂР°РІРѕ, РЅРµС‡С‘С‚РЅС‹Рµ РїСЂР°РІРѕв†’Р»РµРІРѕ
      int cc = (r % 2 == 0) ? c : (SWEEP_COLS - 1 - c);
      float fx = cx + (cc - SWEEP_COLS / 2) * SWEEP_SPACING;
      fx = constrain(fx, 0.0f, WORK_X);
      sweepPts[k++] = {fx, fy};
    }
  }
  sweepPtCount = k;
  Serial.printf("[SWEEP] Generated %d points for zone (%.1f,%.1f)\n", sweepPtCount, cx, cy);
}

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ HOVERBOARD FEEDBACK в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
// РЎС‚Р°РЅРґР°СЂС‚РЅС‹Р№ С„РёРґР±СЌРє-С„СЂРµР№Рј open-source hoverboard РїСЂРѕС€РёРІРєРё
#pragma pack(push, 1)
struct HoverFeedback {
  uint16_t start;       // 0xABCD
  int16_t  cmd1;
  int16_t  cmd2;
  int16_t  speedR;
  int16_t  speedL;
  int16_t  batVoltage;  // 0.1 Р’
  int16_t  boardTemp;   // 0.1 В°C
  uint16_t cmdLed;
  uint16_t checksum;
};
#pragma pack(pop)

uint8_t  hoverBuf[sizeof(HoverFeedback)];
int      hoverBufIdx = 0;
int      batPercent  = 100;

// Р”РёР°РїР°Р·РѕРЅ РЅР°РїСЂСЏР¶РµРЅРёР№ Р±Р°С‚Р°СЂРµРё hoverboard (36 Р’ Li-Ion)
#define BAT_V_MIN  32.0f   // 0 %
#define BAT_V_MAX  42.0f   // 100 %

void readHoverFeedback() {
  while (Serial2.available()) {
    uint8_t b = (uint8_t)Serial2.read();
    // РћР¶РёРґР°РµРј СЃС‚Р°СЂС‚-Р±Р°Р№С‚С‹ 0xCD 0xAB (little-endian 0xABCD)
    if (hoverBufIdx == 0) {
      if (b != 0xCDu) continue;
    } else if (hoverBufIdx == 1) {
      if (b != 0xABu) { hoverBufIdx = 0; continue; }
    }
    hoverBuf[hoverBufIdx++] = b;
    if (hoverBufIdx >= (int)sizeof(HoverFeedback)) {
      hoverBufIdx = 0;
      HoverFeedback *fb = (HoverFeedback *)hoverBuf;
      uint16_t cs = fb->start ^ (uint16_t)fb->cmd1 ^ (uint16_t)fb->cmd2
                  ^ (uint16_t)fb->speedR ^ (uint16_t)fb->speedL
                  ^ (uint16_t)fb->batVoltage ^ (uint16_t)fb->boardTemp
                  ^ fb->cmdLed;
      if (cs == fb->checksum && fb->batVoltage > 0) {
        float volts = fb->batVoltage * 0.1f;
        batPercent  = (int)constrain(
          (volts - BAT_V_MIN) / (BAT_V_MAX - BAT_V_MIN) * 100.0f, 0.0f, 100.0f);
      }
    }
  }
}

// Pi serial
HardwareSerial PiSerial(1);
char piLineBuf[128];
int  piLineLen = 0;

// WebSocket
WebSocketsServer wsServer(81);
uint8_t wsClient = 255;

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ IMU в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
#if HAS_MPU6050_LIGHT
MPU6050 mpu(Wire);
#endif
float robotHeading = 0.0f;  // degrees; 0 = robot points toward anchor R (+X)
bool  imuOk = false;
unsigned long lastImuMs = 0;

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ CALIBRATION в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
bool  calibrated = false;       // AUTO blocked until app pose + UWB candidate match
bool  headingValid = false;
bool  calMode    = false;       // raw range output for antenna-delay calibration

// Heading drift correction: compare gyro vs UWB displacement every MIN_CORR_DIST m
#define MIN_CORR_DIST   0.6f   // minimum travel distance between corrections
#define CORR_BLEND      0.25f  // how much to trust UWB heading (0=ignore, 1=full replace)
#define CORR_MAX_ERR    60.0f  // only correct if gyro/UWB differ by less than this
float corrBaseX = WORK_X * 0.5f, corrBaseY = WORK_Y * 0.5f;

void imuSetup() {
#if HAS_MPU6050_LIGHT
  Wire.begin(IMU_SDA, IMU_SCL);
  byte status = mpu.begin();
  if (status == 0) {
    delay(1000);
    mpu.calcOffsets();  // ~1 s calibration, keep robot still
    imuOk = true;
    lastImuMs = millis();
    Serial.println("[IMU] MPU6050 OK, offsets calculated");
  } else {
    Serial.printf("[IMU] MPU6050 error %d вЂ” heading disabled\n", status);
  }
#else
  Serial.println("[IMU] MPU6050_light library not installed вЂ” using app calibration heading only");
#endif
}

void imuUpdate() {
#if HAS_MPU6050_LIGHT
  if (!imuOk) return;
  mpu.update();
  robotHeading = mpu.getAngleZ();  // degrees, integrated from gyro
#endif
}

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ ZONES в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
struct Zone {
  float cx, cy;   // center in meters (ESP32 coords: X=court length, Y=court width)
};

Zone  zones[MAX_ZONES];
int   zoneCount      = 0;
int   currentZoneIdx = 0;

// Parse "ZONES:x1,y1;x2,y2;..." вЂ” up to MAX_ZONES semicolon-separated pairs
void parseZones(const char *data) {
  zoneCount      = 0;
  currentZoneIdx = 0;
  searchSub      = SS_NAVIGATE;

  char buf[256];
  strncpy(buf, data, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char *token = strtok(buf, ";");
  while (token && zoneCount < MAX_ZONES) {
    float cx, cy;
    if (sscanf(token, "%f,%f", &cx, &cy) == 2) {
      if (isInsideWork(cx, cy)) {
        zones[zoneCount].cx = cx;
        zones[zoneCount].cy = cy;
        zoneCount++;
      } else {
        Serial.printf("[ZONES] Reject out-of-work target (%.2f, %.2f)\n", cx, cy);
      }
    }
    token = strtok(nullptr, ";");
  }
  Serial.printf("[ZONES] Stored %d zone(s)\n", zoneCount);
  for (int i = 0; i < zoneCount; i++) {
    Serial.printf("  zone[%d] = (%.2f, %.2f)\n", i, zones[i].cx, zones[i].cy);
  }
}

// Р’РѕР·РІСЂР°С‰Р°РµС‚ РёРЅРґРµРєСЃ Р±Р»РёР¶Р°Р№С€РµР№ Р·РѕРЅС‹ (РёСЃРєР»СЋС‡Р°СЏ exclude; -1 = РЅРµ РёСЃРєР»СЋС‡Р°С‚СЊ РЅРёРєРѕРіРѕ)
int nearestZone(int exclude = -1) {
  float bestDist = FLT_MAX;
  int   bestIdx  = 0;
  for (int i = 0; i < zoneCount; i++) {
    if (i == exclude) continue;
    float d = distTo(zones[i].cx, zones[i].cy);
    if (d < bestDist) { bestDist = d; bestIdx = i; }
  }
  return bestIdx;
}

void enterSearch() {
  autoSub      = AS_SEARCH;
  searchSub    = SS_NAVIGATE;
  sweepSpinning = false;
}

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ BALL ISR в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
void IRAM_ATTR onBallPass() {
  ballsCollected++;
}

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ TRILATERATION в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
bool isInsideWork(float x, float y) {
  return x >= 0.0f && x <= WORK_X && y >= 0.0f && y <= WORK_Y;
}

bool isNearWorkBoundary(float x, float y) {
  return x < BOUNDARY_SLOW_M || x > (WORK_X - BOUNDARY_SLOW_M) ||
         y < BOUNDARY_SLOW_M || y > (WORK_Y - BOUNDARY_SLOW_M);
}

float dist2f(Point2f a, Point2f b) {
  return sqrtf((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

bool hasFreshUwb(unsigned long now) {
  return rangeL > 0.0f && rangeR > 0.0f &&
         (now - lastRangeL) < ANCHOR_TIMEOUT_MS &&
         (now - lastRangeR) < ANCHOR_TIMEOUT_MS;
}

void safetyStop(const char *reason, bool invalidatePose) {
  hoverStop();
  mainState = ST_MANUAL;
  demoMode = false;
  demoStartMs = 0;
  wsSend(String("ERROR,") + reason);
  wsSend("STATE,MANUAL");
  Serial.printf("[SAFE] STOP: %s\n", reason);
  if (invalidatePose) {
    calibrated = false;
    haveAcceptedPose = false;
  }
}

void seedSafetyTestAutoState() {
  hoverStop();
  mainState = ST_AUTO;
  autoSub = AS_TRACKING;
  calibrated = true;
  headingValid = true;
  haveAcceptedPose = true;
  uwbX = WORK_X * 0.5f;
  uwbY = WORK_Y * 0.5f;
  lastPoseMs = millis();
  Point2f pose = {uwbX, uwbY};
  Point2f anchorL = {ANCHOR_L_X, ANCHOR_L_Y};
  Point2f anchorR = {ANCHOR_R_X, ANCHOR_R_Y};
  rangeL = dist2f(pose, anchorL);
  rangeR = dist2f(pose, anchorR);
  lastRangeL = millis();
  lastRangeR = millis();
  zoneCount = 0;
  sweepSpinning = false;
  wsSend("TEST,AUTO_STATE_SEEDED");
}

bool startAutoFromZones(bool requestDemoMode) {
  if (!calibrated) {
    wsSend("ERROR,NOT_CALIBRATED");
    return false;
  }
  if (!headingValid) {
    wsSend("ERROR,HEADING_INVALID");
    return false;
  }
  if (!hasFreshUwb(millis()) || !haveAcceptedPose) {
    wsSend("ERROR,UWB_STALE");
    return false;
  }
  if (!isInsideWork(uwbX, uwbY)) {
    wsSend("ERROR,OUT_OF_BOUNDS");
    return false;
  }
  if (zoneCount == 0) {
    wsSend("ERROR,NO_ZONES");
    return false;
  }
  if (requestDemoMode && zoneCount != 1) {
    wsSend("ERROR,DEMO_REQUIRES_ONE_ZONE");
    return false;
  }

  int startZone = nearestZone();
  if (!isInsideWork(zones[startZone].cx, zones[startZone].cy)) {
    wsSend("ERROR,TARGET_OUT_OF_BOUNDS");
    return false;
  }
  if (batPercent < 20) {
    wsSend(String("WARN,LOW_BATTERY,") + String(batPercent));
  }

  demoMode = requestDemoMode;
  demoStartMs = demoMode ? millis() : 0;
  if (demoMode) {
    navSpeed = 30;
    navSpeedSlow = 20;
  }

  mainState = ST_AUTO;
  currentZoneIdx = startZone;
  lastPiMsgMs = millis();
  lastReportedSub = AS_COLLECTING; // force MODE:SEARCH report
  enterSearch();
  wsSend(demoMode ? "STATE,DEMO" : "STATE,AUTO");
  PiSerial.println("MODE:SEARCH");
  return true;
}

// Returns the two possible intersections from ranges to diagonal working-area anchors.
bool trilaterateCandidates(float rL, float rR, Point2f &a, Point2f &b) {
  if (rL <= 0.0f || rR <= 0.0f || ANCHOR_DIST < 0.01f) return false;

  const float dx = ANCHOR_R_X - ANCHOR_L_X;
  const float dy = ANCHOR_R_Y - ANCHOR_L_Y;
  const float ex = dx / ANCHOR_DIST;
  const float ey = dy / ANCHOR_DIST;

  const float along = (rL * rL - rR * rR + ANCHOR_DIST * ANCHOR_DIST) /
                      (2.0f * ANCHOR_DIST);
  float h2 = rL * rL - along * along;
  if (h2 < -0.25f) return false;
  if (h2 < 0.0f) h2 = 0.0f;
  const float h = sqrtf(h2);

  const float bx = ANCHOR_L_X + along * ex;
  const float by = ANCHOR_L_Y + along * ey;
  const float px = -ey;
  const float py = ex;

  a = {bx + h * px, by + h * py};
  b = {bx - h * px, by - h * py};
  return true;
}

bool updateUwbCandidates() {
  haveCandidates = trilaterateCandidates(rangeL, rangeR, candA, candB);
  return haveCandidates;
}

bool acceptPose(Point2f p, unsigned long now) {
  if (!isInsideWork(p.x, p.y)) {
    uwbRejects++;
    return false;
  }

  if (haveAcceptedPose) {
    Point2f last = {uwbX, uwbY};
    float jump = dist2f(p, last);
    if (jump > MAX_UWB_JUMP_M) {
      uwbRejects++;
      Serial.printf("[UWB] rejected jump %.2fm (%.2f,%.2f -> %.2f,%.2f)\n",
                    jump, uwbX, uwbY, p.x, p.y);
      return false;
    }
  }

  uwbX = p.x;
  uwbY = p.y;
  haveAcceptedPose = true;
  lastPoseMs = now;
  uwbRejects = 0;
  return true;
}

bool trackUwbPose(unsigned long now) {
  if (!updateUwbCandidates()) {
    uwbRejects++;
    return false;
  }
  if (!haveAcceptedPose) return false;

  Point2f last = {uwbX, uwbY};
  bool aInside = isInsideWork(candA.x, candA.y);
  bool bInside = isInsideWork(candB.x, candB.y);
  if (!aInside && !bInside) {
    uwbRejects++;
    return false;
  }

  Point2f chosen;
  if (aInside && !bInside) chosen = candA;
  else if (!aInside && bInside) chosen = candB;
  else chosen = (dist2f(candA, last) <= dist2f(candB, last)) ? candA : candB;

  return acceptPose(chosen, now);
}

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ UWB CALLBACKS в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
void uwbNewRange() {
  DW1000Device *dev = DW1000Ranging.getDistantDevice();
  if (!dev) return;
  float    dist = dev->getRange();
  uint16_t addr = dev->getShortAddress();
  unsigned long now = millis();

  if (IS_ANCHOR_L(addr))      { rangeL = dist; lastRangeL = now; }
  else if (IS_ANCHOR_R(addr)) { rangeR = dist; lastRangeR = now; }

  // Calibration mode: send raw ranges to phone so analyze.py can tune ANTENNA_DELAY
  if (calMode) {
    const char* side = IS_ANCHOR_L(addr) ? "L" : (IS_ANCHOR_R(addr) ? "R" : "?");
    wsSend(String("RANGE,") + String(side) + "," + String(dist, 3));
  }

  if (rangeL > 0.0f && rangeR > 0.0f &&
      (now - lastRangeL) < ANCHOR_TIMEOUT_MS &&
      (now - lastRangeR) < ANCHOR_TIMEOUT_MS) {
    if (calibrated && haveAcceptedPose) {
      if (!trackUwbPose(now) && uwbRejects >= MAX_UWB_REJECTS) {
        safetyStop("UWB_BRANCH", true);
      }
    } else {
      updateUwbCandidates();
    }
  }
}

void uwbNewDevice(DW1000Device *device) {
  Serial.printf("[UWB] Anchor 0x%04X appeared\n", device->getShortAddress());
}

void uwbInactive(DW1000Device *device) {
  Serial.printf("[UWB] Anchor 0x%04X lost\n", device->getShortAddress());
}

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ WEBSOCKET в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
void wsSend(const String &msg) {
  if (wsClient != 255) wsServer.sendTXT(wsClient, msg.c_str());
  Serial.printf("[WS TX] %s\n", msg.c_str());
}

void handlePhone(const String &raw) {
  String cmd = String(raw);
  cmd.trim();
  Serial.printf("[WS RX] %s\n", cmd.c_str());

  if (cmd == "PING")          { wsSend("PONG"); return; }
  if (cmd == "STOP")          { hoverStop(); return; }
  if (cmd == "ATTACHMENT_ON") { digitalWrite(RELAY_PIN, HIGH); return; }
  if (cmd == "ATTACHMENT_OFF"){ digitalWrite(RELAY_PIN, LOW);  return; }
  if (cmd == "MOUNT_ON" || cmd == "MOUNT_OFF") return;

  // Non-driving safety injection commands for bench tests.
  // They seed AUTO in AS_TRACKING with motors stopped, then trigger a specific
  // safety path so STOP reasons can be verified from the app/serial log.
  if (cmd == "TEST_STOP:UWB_LOST") {
    seedSafetyTestAutoState();
    safetyStop("UWB_LOST");
    return;
  }
  if (cmd == "TEST_STOP:UWB_STALE") {
    seedSafetyTestAutoState();
    lastRangeL = millis() - ANCHOR_TIMEOUT_MS - 100UL;
    lastRangeR = millis() - ANCHOR_TIMEOUT_MS - 100UL;
    safetyStop("UWB_STALE");
    return;
  }
  if (cmd == "TEST_STOP:UWB_BRANCH") {
    seedSafetyTestAutoState();
    uwbRejects = MAX_UWB_REJECTS;
    safetyStop("UWB_BRANCH", true);
    return;
  }
  if (cmd == "TEST_STOP:OUT_OF_BOUNDS") {
    seedSafetyTestAutoState();
    uwbX = -0.1f;
    safetyStop("OUT_OF_BOUNDS", true);
    return;
  }
  if (cmd == "TEST_STOP:PI_TIMEOUT") {
    seedSafetyTestAutoState();
    lastPiMsgMs = millis() - PI_UART_TIMEOUT_MS - 100UL;
    wsSend("WARN,PI_TIMEOUT");
    safetyStop("PI_TIMEOUT");
    return;
  }

  if (cmd.startsWith("M,")) {
    int c = cmd.indexOf(',', 2);
    if (c > 0) {
      int L = constrain(cmd.substring(2, c).toInt(), -100, 100);
      int R = constrain(cmd.substring(c + 1).toInt(), -100, 100);
      hoverSend((int16_t)((R - L) / 2), (int16_t)((L + R) / 2));
    }
    return;
  }

  if (cmd.startsWith("ZONES:")) {
    parseZones(cmd.c_str() + 6);
    return;
  }

  if (cmd == "AUTO_START") {
    startAutoFromZones(false);
    return;
  }
  if (cmd == "DEMO_START") {
    startAutoFromZones(true);
    return;
  }
  if (cmd == "AUTO_STOP") {
    mainState = ST_MANUAL;
    demoMode = false;
    demoStartMs = 0;
    hoverStop();
    wsSend("STATE,MANUAL");
    PiSerial.println("MODE:SEARCH");
    return;
  }
  if (cmd.startsWith("SET_HOME:")) {
    String v = cmd.substring(9);
    int c = v.indexOf(',');
    if (c > 0) {
      float hx = v.substring(0, c).toFloat();
      float hy = v.substring(c + 1).toFloat();
      if (!isInsideWork(hx, hy)) {
        wsSend("ERROR,HOME_OUT_OF_BOUNDS");
        return;
      }
      homeX = hx;
      homeY = hy;
    }
    return;
  }
  if (cmd.startsWith("SET_BALLS:")) {
    maxBalls = constrain(cmd.substring(10).toInt(), 1, MAX_BALLS);
    return;
  }
  if (cmd.startsWith("SET_SPEED:")) {
    int spd = constrain(cmd.substring(10).toInt(), 0, 2);
    navSpeed      = (spd == 0) ? 30 : (spd == 1) ? 50 : 70;
    navSpeedSlow  = (spd == 0) ? 20 : (spd == 1) ? 30 : 50;
    return;
  }
  if (cmd.startsWith("CALIBRATE")) {
    float calHeading = 0.0f;
    float calX = -1.0f;
    float calY = -1.0f;
    bool hasAppPose = false;

    if (cmd.startsWith("CALIBRATE:")) {
      // Format: CALIBRATE:heading_deg:x:y
      // heading вЂ” app-provided robot front direction in degrees
      // x,y     вЂ” robot position in working-area metres
      String v = cmd.substring(10);
      int c1 = v.indexOf(':');
      int c2 = v.lastIndexOf(':');
      if (c1 > 0 && c2 > c1) {
        calHeading = v.substring(0, c1).toFloat();
        calX       = v.substring(c1 + 1, c2).toFloat();
        calY       = v.substring(c2 + 1).toFloat();
        hasAppPose = true;
      }
    }
    if (!hasAppPose) {
      wsSend("ERROR,CALIBRATION_FORMAT");
      return;
    }
    if (!isInsideWork(calX, calY)) {
      wsSend("ERROR,CALIBRATION_OUT_OF_BOUNDS");
      return;
    }
    unsigned long now = millis();
    if (!hasFreshUwb(now) || !updateUwbCandidates()) {
      wsSend("ERROR,UWB_STALE");
      return;
    }

    Point2f appPose = {calX, calY};
    float dA = dist2f(candA, appPose);
    float dB = dist2f(candB, appPose);
    Point2f selected = (dA <= dB) ? candA : candB;
    float mismatch = (dA <= dB) ? dA : dB;
    if (mismatch > CAL_MISMATCH_M) {
      wsSend(String("ERROR,CALIBRATION_MISMATCH,") + String(mismatch, 2));
      Serial.printf("[CAL] mismatch %.2fm app=(%.2f,%.2f) A=(%.2f,%.2f) B=(%.2f,%.2f)\n",
                    mismatch, calX, calY, candA.x, candA.y, candB.x, candB.y);
      return;
    }

#if HAS_MPU6050_LIGHT
    // Set IMU heading reference
    if (imuOk) {
      mpu.update();
      mpu.setAngleZ(calHeading);
    }
#endif
    robotHeading = calHeading;
    headingValid = true;

    uwbX = selected.x;
    uwbY = selected.y;
    haveAcceptedPose = true;
    lastPoseMs = now;
    uwbRejects = 0;

    // Reset drift-correction base
    corrBaseX = uwbX;
    corrBaseY = uwbY;
    homeX = uwbX;
    homeY = uwbY;

    calibrated = true;
    wsSend(String("CALIBRATED,") + String(uwbX, 2) + "," + String(uwbY, 2));
    Serial.printf("[CAL] heading=%.1f app=(%.2f,%.2f) accepted=(%.2f,%.2f) mismatch=%.2f\n",
                  calHeading, calX, calY, uwbX, uwbY, mismatch);
    return;
  }

  if (cmd == "RESET") {
    hoverStop();
    mainState    = ST_MANUAL;
    demoMode     = false;
    demoStartMs  = 0;
    calibrated   = false;
    headingValid = false;
    haveAcceptedPose = false;
    haveCandidates = false;
    uwbRejects = 0;
    zoneCount    = 0;
    currentZoneIdx = 0;
    sweepPtIdx   = 0;
    sweepPtCount = 0;
    homeX = homeY = 0.0f;
    ballsCollected = 0;
    lastReportedSub = AS_COLLECTING; // force MODE send on next AUTO
    PiSerial.println("MODE:SEARCH");
    wsSend("BALLS_COUNT,0");
    wsSend("STATE,MANUAL");
    wsSend("RESET_OK");
    return;
  }

  if (cmd == "CAL_RAW") {
    calMode = true;
    wsSend("CAL_MODE,ON");
    return;
  }
  if (cmd == "CAL_STOP") {
    calMode = false;
    wsSend("CAL_MODE,OFF");
    return;
  }

  if (cmd == "DELIVER_NOW") {
    if (!calibrated) { wsSend("ERROR,NOT_CALIBRATED"); return; }
    if (!headingValid) { wsSend("ERROR,HEADING_INVALID"); return; }
    if (!hasFreshUwb(millis()) || !haveAcceptedPose) { wsSend("ERROR,UWB_STALE"); return; }
    if (mainState == ST_UNLOADING) { return; } // already at home
    mainState = ST_AUTO;
    autoSub   = AS_RETURNING;
    hoverStop();
    navStartMs = millis();
    wsSend("STATE,AUTO");
    PiSerial.println("MODE:RETURNING");
    return;
  }
}

void wsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      wsClient = num;
      wsSend("STATE,CONNECTED");
      Serial.printf("[WS] #%d connected\n", num);
      break;
    case WStype_DISCONNECTED:
      if (num == wsClient) wsClient = 255;
      Serial.printf("[WS] #%d disconnected\n", num);
      break;
    case WStype_TEXT:
      handlePhone(String((char *)payload));
      break;
    default: break;
  }
}

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ PI UART в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
void handlePiLine(const char *line) {
  lastPiMsgMs = millis();
  Serial.printf("[PI RX] %s\n", line);
  if (mainState != ST_AUTO) return;

  if (strncmp(line, "TRACK:", 6) == 0) {
    // РќРµ РїСЂРµСЂС‹РІР°РµРј СЃР±РѕСЂ Рё РІРѕР·РІСЂР°С‚ РґРѕРјРѕР№
    if (autoSub == AS_COLLECTING || autoSub == AS_RETURNING) return;
    int dx = 0, sz = 0;
    if (sscanf(line + 6, "%d,%d", &dx, &sz) == 2) {
      if (sz > 4000) {
        autoSub = AS_COLLECTING;
        hoverStop();
        collectStartMs = millis();
        digitalWrite(RELAY_PIN, HIGH);
      } else {
        autoSub = AS_TRACKING;
        if      (dx < -20) hoverSend(-10, 30);
        else if (dx >  20) hoverSend( 10, 30);
        else               hoverSend(  0, 50);
      }
    }
  } else if (strcmp(line, "COLLECT") == 0) {
    if (autoSub == AS_RETURNING) return; // РЅРµ РїСЂРµСЂС‹РІР°РµРј РІРѕР·РІСЂР°С‚
    if (autoSub != AS_COLLECTING) {
      autoSub = AS_COLLECTING;
      hoverStop();
      collectStartMs = millis();
      digitalWrite(RELAY_PIN, HIGH);
    }
  } else if (strcmp(line, "SEARCH") == 0) {
    if (autoSub == AS_TRACKING) enterSearch();
  } else if (strcmp(line, "RETURN") == 0) {
    // Pi СЃРёРіРЅР°Р»РёР·РёСЂСѓРµС‚ BALLS_FULL вЂ” РїРµСЂРµС…РѕРґРёРј РІ РІРѕР·РІСЂР°С‚ РµСЃР»Рё РµС‰С‘ РЅРµ С‚Р°Рј
    if (autoSub != AS_RETURNING && autoSub != AS_COLLECTING) {
      autoSub = AS_RETURNING;
      hoverStop();
    }
  }
}

void readPi() {
  while (PiSerial.available()) {
    char c = (char)PiSerial.read();
    if (c == '\n' || c == '\r') {
      if (piLineLen > 0) {
        piLineBuf[piLineLen] = '\0';
        handlePiLine(piLineBuf);
        piLineLen = 0;
      }
    } else if (piLineLen < (int)sizeof(piLineBuf) - 1) {
      piLineBuf[piLineLen++] = c;
    }
  }
}

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ HEADING DRIFT CORRECTION в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
// Called while robot drives in a straight line.
// Compares gyro heading with the heading derived from UWB displacement.
// If they match within CORR_MAX_ERR, blends a small correction into the gyro.
// This keeps heading accurate over 3-hour sessions without a magnetometer.
void tryCorrectHeading() {
  if (!imuOk || !calibrated) return;

  float dx   = uwbX - corrBaseX;
  float dy   = uwbY - corrBaseY;
  float dist = sqrtf(dx * dx + dy * dy);

  if (dist < MIN_CORR_DIST) return; // not enough travel yet

  float uwbDeg = atan2f(dy, dx) * (180.0f / M_PI);

  // Shortest-path angle difference
  float diff = uwbDeg - robotHeading;
  while (diff >  180.0f) diff -= 360.0f;
  while (diff < -180.0f) diff += 360.0f;

  if (fabsf(diff) < CORR_MAX_ERR) {
    float correction = CORR_BLEND * diff;
    robotHeading += correction;
#if HAS_MPU6050_LIGHT
    mpu.setAngleZ(robotHeading);
#endif
    Serial.printf("[HDNG] drift corr %.1fВ° (uwb=%.1f gyro_was=%.1f)\n",
                  correction, uwbDeg, robotHeading - correction);
  }

  // Reset base regardless вЂ” avoids stale base after spin / direction change
  corrBaseX = uwbX;
  corrBaseY = uwbY;
}

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ NAVIGATION в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
float distTo(float tx, float ty) {
  float dx = tx - uwbX, dy = ty - uwbY;
  return sqrtf(dx * dx + dy * dy);
}

// Heading-aware navigation using MPU-6050 yaw.
// If IMU unavailable, falls back to UWB X-delta steering (old behaviour).
// NOTE: positive steer = turns right. If robot turns opposite to expected,
//       negate KP_HEADING or swap motor wiring.
void driveToward(float tx, float ty) {
  float dist = distTo(tx, ty);
  if (dist < ARRIVE_DIST) {
    hoverStop();
    return;
  }

  int16_t speed = (dist > 1.0f) ? navSpeed : navSpeedSlow;

  if (imuOk) {
    float dx = tx - uwbX;
    float dy = ty - uwbY;
    float targetDeg = atan2f(dy, dx) * (180.0f / M_PI);

    float err = targetDeg - robotHeading;
    while (err >  180.0f) err -= 360.0f;
    while (err < -180.0f) err += 360.0f;

    int16_t steer = (int16_t)constrain(-err / 180.0f * KP_HEADING, -(float)MAX_STEER, (float)MAX_STEER);

    if (fabsf(err) > 45.0f) {
      hoverSend(steer, 0);
      // Spinning in place вЂ” reset correction base so we don't mix spin into heading
      corrBaseX = uwbX;
      corrBaseY = uwbY;
    } else {
      hoverSend(steer, speed);
      tryCorrectHeading(); // only correct while moving forward
    }
  } else {
    // Fallback: steer by X-delta only
    float dx = tx - uwbX;
    int16_t steer = (int16_t)constrain(dx / WORK_X * 200.0f, -100.0f, 100.0f);
    hoverSend(steer, speed);
  }
}

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ STATE HANDLERS в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
// РћС‚РїСЂР°РІР»СЏРµС‚ MODE РЅР° Pi (С‡С‚РѕР±С‹ Pi Р·РЅР°Р» РјРѕР¶РЅРѕ Р»Рё РїРѕСЃС‹Р»Р°С‚СЊ TRACK)
void reportModeToPi(AutoSub sub) {
  if (sub == lastReportedSub) return;
  lastReportedSub = sub;
  switch (sub) {
    case AS_SEARCH:
    case AS_TRACKING:  PiSerial.println("MODE:SEARCH");     break;
    case AS_COLLECTING: PiSerial.println("MODE:COLLECTING"); break;
    case AS_RETURNING:  PiSerial.println("MODE:RETURNING");  break;
  }
}

void handleAuto() {
  if (demoMode && demoStartMs > 0 && millis() - demoStartMs > DEMO_TOTAL_TIMEOUT_MS) {
    safetyStop("DEMO_TIMEOUT");
    demoMode = false;
    demoStartMs = 0;
    return;
  }
  if (!calibrated || !headingValid || !haveAcceptedPose) {
    safetyStop(!calibrated ? "NOT_CALIBRATED" : (!headingValid ? "HEADING_INVALID" : "UWB_STALE"));
    return;
  }
  if (!isInsideWork(uwbX, uwbY)) {
    safetyStop("OUT_OF_BOUNDS", true);
    return;
  }
  if (isNearWorkBoundary(uwbX, uwbY)) {
    wsSend("WARN,BOUNDARY_NEAR");
  }

  reportModeToPi(autoSub);

  switch (autoSub) {

    // в”Ђв”Ђ AS_SEARCH в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    case AS_SEARCH:
      if (zoneCount == 0) {
        hoverSend(30, 0);   // РЅРµС‚ Р·РѕРЅ вЂ” РєСЂСѓС‚РёРјСЃСЏ РЅР° РјРµСЃС‚Рµ
        break;
      }
      {
        Zone &z = zones[currentZoneIdx % zoneCount];

        if (searchSub == SS_NAVIGATE) {
          float d = distTo(z.cx, z.cy);
          if (d < ZONE_ARRIVE_DIST) {
            // РџСЂРёР±С‹Р»Рё РІ Р·РѕРЅСѓ вЂ” РіРµРЅРµСЂРёСЂСѓРµРј СЃРµС‚РєСѓ РѕР±С…РѕРґР°
            navStartMs    = 0;
            generateSweepGrid(z.cx, z.cy);
            searchSub    = SS_SWEEP;
            sweepSpinning = false;
          } else {
            if (navStartMs == 0) navStartMs = millis();
            unsigned long navLimitMs = demoMode ? DEMO_NAV_TIMEOUT_MS : NAV_STUCK_MS;
            if (millis() - navStartMs > navLimitMs) {
              if (demoMode) {
                safetyStop("DEMO_NAV_TIMEOUT");
                demoMode = false;
                demoStartMs = 0;
                break;
              }
              // Stuck navigating вЂ” skip to nearest other zone
              Serial.printf("[NAV] Stuck on zone %d > %lu ms, skipping\n", currentZoneIdx, navLimitMs);
              navStartMs = 0;
              currentZoneIdx = nearestZone(currentZoneIdx);
              break;
            }
            driveToward(z.cx, z.cy);
          }
        } else {
          // SS_SWEEP вЂ” РѕР±СЉРµР·Р¶Р°РµРј С‚РѕС‡РєРё СЃРµС‚РєРё 3Г—3
          if (sweepPtIdx >= sweepPtCount) {
            if (demoMode) {
              hoverStop();
              mainState = ST_MANUAL;
              demoMode = false;
              demoStartMs = 0;
              wsSend("DEMO_DONE");
              wsSend("STATE,MANUAL");
              PiSerial.println("MODE:SEARCH");
              break;
            }
            // Р’СЃСЏ СЃРµС‚РєР° РїСЂРѕР№РґРµРЅР° вЂ” РјСЏС‡РµР№ РЅРµС‚, РµРґРµРј РІ Р±Р»РёР¶Р°Р№С€СѓСЋ РґСЂСѓРіСѓСЋ Р·РѕРЅСѓ
            int next = nearestZone(currentZoneIdx);
            Serial.printf("[SWEEP] Zone %d done в†’ nearest zone %d\n",
                          currentZoneIdx, next);
            currentZoneIdx = next;
            searchSub      = SS_NAVIGATE;
            sweepSpinning  = false;
            break;
          }

          Point2f &sp = sweepPts[sweepPtIdx];

          if (!sweepSpinning) {
            if (distTo(sp.x, sp.y) < ARRIVE_DIST) {
              // Р”РѕР±СЂР°Р»РёСЃСЊ РґРѕ С‚РѕС‡РєРё вЂ” РЅР°С‡РёРЅР°РµРј РІСЂР°С‰РµРЅРёРµ
              hoverStop();
              sweepSpinning  = true;
              sweepSpinStart = millis();
            } else {
              driveToward(sp.x, sp.y);
            }
          } else {
            // Р’СЂР°С‰Р°РµРјСЃСЏ РЅР° РјРµСЃС‚Рµ вЂ” Pi СЃРјРѕС‚СЂРёС‚ РїРѕ СЃС‚РѕСЂРѕРЅР°Рј
            hoverSend(30, 0);
            if (millis() - sweepSpinStart >= SWEEP_SPIN_MS) {
              sweepSpinning = false;
              sweepPtIdx++;
            }
          }
        }
      }
      break;

    // в”Ђв”Ђ AS_TRACKING в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    case AS_TRACKING:
      // РЈРїСЂР°РІР»СЏРµС‚СЃСЏ РєРѕРјР°РЅРґР°РјРё TRACK: РѕС‚ Pi
      break;

    // в”Ђв”Ђ AS_COLLECTING в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    case AS_COLLECTING:
      if (millis() - collectStartMs >= COLLECT_RELAY_MS) {
        digitalWrite(RELAY_PIN, LOW);
        int cnt = ballsCollected;
        wsSend(String("BALLS_COUNT,") + String(cnt));
        if (cnt >= maxBalls) {
          PiSerial.println("BALLS_FULL");
          autoSub = AS_RETURNING;
          navStartMs = 0;
        } else if (demoMode) {
          hoverStop();
          mainState = ST_MANUAL;
          demoMode = false;
          demoStartMs = 0;
          wsSend("DEMO_DONE");
          wsSend("STATE,MANUAL");
          PiSerial.println("MODE:SEARCH");
        } else {
          // Р’РѕР·РІСЂР°С‰Р°РµРјСЃСЏ РІ С‚Сѓ Р¶Рµ Р·РѕРЅСѓ (sweepPtIdx РЅРµ СЃР±СЂР°СЃС‹РІР°РµС‚СЃСЏ вЂ” РїСЂРѕРґРѕР»Р¶Р°РµРј СЃРµС‚РєСѓ)
          autoSub   = AS_SEARCH;
          searchSub = SS_SWEEP;
          navStartMs = 0;
        }
      }
      break;

    // в”Ђв”Ђ AS_RETURNING в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    case AS_RETURNING:
      if (navStartMs == 0) navStartMs = millis();
      if (millis() - navStartMs > NAV_STUCK_RETURN_MS) {
        hoverStop();
        mainState = ST_MANUAL;
        navStartMs = 0;
        wsSend("ERROR,RETURN_STUCK");
        wsSend("STATE,MANUAL");
        break;
      }
      driveToward(homeX, homeY);
      if (distTo(homeX, homeY) < ARRIVE_DIST) {
        hoverStop();
        mainState       = ST_UNLOADING;
        unloadBasketOut = false;
        unloadWaitMs    = 0;
        navStartMs      = 0;
        wsSend("STATE,UNLOADING");
        PiSerial.println("MODE:RETURNING");
      }
      break;
  }
}

void handleUnloading() {
  bool basketPresent = (digitalRead(BASKET_PIN) == LOW);
  if (!unloadBasketOut) {
    if (!basketPresent) { unloadBasketOut = true; }
  } else {
    if (basketPresent) {
      if (unloadWaitMs == 0) unloadWaitMs = millis();
      else if (millis() - unloadWaitMs >= UNLOAD_SETTLE_MS) {
        ballsCollected  = 0;
        unloadBasketOut = false;
        unloadWaitMs    = 0;
        currentZoneIdx  = nearestZone(); // start with nearest zone
        navStartMs      = 0;
        mainState       = ST_AUTO;
        enterSearch();
        wsSend("STATE,AUTO");
        wsSend("BALLS_COUNT,0");
      }
    }
  }
}

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ SETUP в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== %s %s ===\n", FW_NAME, FW_VERSION);
  Serial.printf("[FW] %s\n", FW_PURPOSE);
  Serial.printf("[GEOM] work=%.2fx%.2f court_origin=(%.2f,%.2f) anchors L=(%.2f,%.2f) R=(%.2f,%.2f)\n",
                WORK_X, WORK_Y, COURT_ORIGIN_X, COURT_ORIGIN_Y,
                ANCHOR_L_X, ANCHOR_L_Y, ANCHOR_R_X, ANCHOR_R_Y);

  pinMode(RELAY_PIN,     OUTPUT); digitalWrite(RELAY_PIN, LOW);
  pinMode(BASKET_PIN,    INPUT);
  pinMode(BALL_SENS_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(BALL_SENS_PIN), onBallPass, FALLING);

  PiSerial.begin(115200, SERIAL_8N1, PI_RX, PI_TX);
  Serial2.begin(115200, SERIAL_8N1, HOVER_RX, HOVER_TX);

  imuSetup();

  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  Serial.printf("[WIFI] AP %s  IP %s\n", WIFI_SSID,
                WiFi.softAPIP().toString().c_str());

  wsServer.begin();
  wsServer.onEvent(wsEvent);
  Serial.println("[WS] :81");

  SPI.begin(UWB_CLK, UWB_MISO, UWB_MOSI, UWB_CSN);
  DW1000Ranging.initCommunication(UWB_RSTN, UWB_CSN, UWB_IRQ);
  DW1000.setAntennaDelay(ANTENNA_DELAY);
  DW1000Ranging.attachNewRange(uwbNewRange);
  DW1000Ranging.attachBlinkDevice(uwbNewDevice);
  DW1000Ranging.attachInactiveDevice(uwbInactive);
  DW1000Ranging.startAsTag(TAG_EUI, DW1000.MODE_LONGDATA_RANGE_LOWPOWER, false);
  Serial.println("[UWB] Tag started");

  Serial.println("STATE=MANUAL. Ready.");
}

// в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ LOOP в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
void loop() {
  wsServer.loop();
  DW1000Ranging.loop();
  imuUpdate();
  readPi();
  readHoverFeedback();

  unsigned long now = millis();

  switch (mainState) {
    case ST_MANUAL:    break;
    case ST_AUTO:      handleAuto();     break;
    case ST_UNLOADING: handleUnloading(); break;
  }

  // POS every 500 ms
  if (now - lastPosSend >= 500UL) {
    lastPosSend = now;
    wsSend(String("POS,") + String(uwbX, 2) + "," + String(uwbY, 2));
  }

  // BAT every 10 s (СЂРµР°Р»СЊРЅРѕРµ РЅР°РїСЂСЏР¶РµРЅРёРµ СЃ hoverboard РµСЃР»Рё РґРѕСЃС‚СѓРїРЅРѕ)
  if (now - lastBatSend >= 10000UL) {
    lastBatSend = now;
    wsSend(String("BAT_PCT,") + String(batPercent));
  }

  // UWB watchdog: РµСЃР»Рё РѕР±Р° СЏРєРѕСЂСЏ РјРѕР»С‡Р°С‚ > UWB_LOST_TIMEOUT_MS вЂ” СЃС‚РѕРї
  if (now - lastUwbCheck >= 1000UL) {
    lastUwbCheck = now;
    bool lLost = (now - lastRangeL) > UWB_LOST_TIMEOUT_MS;
    bool rLost = (now - lastRangeR) > UWB_LOST_TIMEOUT_MS;
    bool stale = !hasFreshUwb(now);
    if (mainState == ST_AUTO && calibrated) {
      if (lLost && rLost) {
        safetyStop("UWB_LOST");
      } else if (stale) {
        safetyStop("UWB_STALE");
      } else if (uwbRejects >= MAX_UWB_REJECTS) {
        safetyStop("UWB_BRANCH", true);
      }
    }
  }

  // Pi UART timeout вЂ” if Pi silent > 15s while searching/tracking, warn
  if (mainState == ST_AUTO &&
      (autoSub == AS_SEARCH || autoSub == AS_TRACKING) &&
      lastPiMsgMs > 0 &&
      (now - lastPiMsgMs) > PI_UART_TIMEOUT_MS) {
    wsSend("WARN,PI_TIMEOUT");
    lastPiMsgMs = now; // reset so we don't spam warnings
    safetyStop("PI_TIMEOUT");
  }

  // Periodic ball count (every 5s in AUTO)
  if (mainState == ST_AUTO && now - lastBallsSend >= 5000UL) {
    lastBallsSend = now;
    wsSend(String("BALLS_COUNT,") + String(ballsCollected));
  }

  // Anchor status every 1 s
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
  }
}
