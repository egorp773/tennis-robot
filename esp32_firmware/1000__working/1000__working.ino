// ESP32 → Hoverboard FOC (EFeru) via USART2
// GPIO 17 (TX) → PA3 (RX платы, синий провод)
// GPIO 16 (RX) → PA2 (TX платы, зелёный провод)
// GND → GND (чёрный провод)
// КРАСНЫЙ ПРОВОД НЕ ПОДКЛЮЧАТЬ (15V!)

#define HOVER_SERIAL Serial2
#define BAUD_RATE    115200
#define FW_NAME      "1000__working"
#define FW_VERSION   "snapshot-legacy-hoverboard-serial"
#define FW_PURPOSE   "preserved known-working hoverboard serial test"

typedef struct {
  uint16_t start;
  int16_t  steer;
  int16_t  speed;
  uint16_t checksum;
} SerialCommand;

typedef struct {
  uint16_t start;
  int16_t  cmd1;
  int16_t  cmd2;
  int16_t  speedR_meas;
  int16_t  speedL_meas;
  int16_t  batVoltage;
  int16_t  boardTemp;
  uint16_t cmdLed;
  uint16_t checksum;
} SerialFeedback;

SerialFeedback feedback;
byte feedbackBuffer[sizeof(SerialFeedback)];
int feedbackIdx = 0;

void sendCommand(int16_t steer, int16_t speed) {
  SerialCommand cmd;
  cmd.start    = 0xABCD;
  cmd.steer    = steer;
  cmd.speed    = speed;
  cmd.checksum = cmd.start ^ cmd.steer ^ cmd.speed;
  HOVER_SERIAL.write((uint8_t*)&cmd, sizeof(cmd));
}

bool readFeedback() {
  while (HOVER_SERIAL.available()) {
    byte c = HOVER_SERIAL.read();
    feedbackBuffer[feedbackIdx++] = c;
    if (feedbackIdx == 2) {
      uint16_t start = feedbackBuffer[0] | (feedbackBuffer[1] << 8);
      if (start != 0xABCD) {
        feedbackBuffer[0] = feedbackBuffer[1];
        feedbackIdx = 1;
        continue;
      }
    }
    if (feedbackIdx == sizeof(SerialFeedback)) {
      feedbackIdx = 0;
      memcpy(&feedback, feedbackBuffer, sizeof(SerialFeedback));
      uint16_t cs = feedback.start ^ feedback.cmd1 ^ feedback.cmd2 ^
                    feedback.speedR_meas ^ feedback.speedL_meas ^
                    feedback.batVoltage ^ feedback.boardTemp ^ feedback.cmdLed;
      if (cs == feedback.checksum) return true;
    }
  }
  return false;
}

int currentSpeed = 0;
int targetSpeed  = 50;
int currentSteer = 0;

unsigned long lastSend  = 0;
unsigned long lastPrint = 0;

void setup() {
  Serial.begin(115200);
  pinMode(16, INPUT);
  pinMode(17, INPUT);
  delay(100);
  HOVER_SERIAL.begin(BAUD_RATE, SERIAL_8N1, 16, 17);
  Serial.printf("%s %s\n", FW_NAME, FW_VERSION);
  Serial.printf("[FW] %s\n", FW_PURPOSE);
  Serial.println("Hoverboard USART2 Control Ready");
  Serial.println("w=forward s=back a=left d=right x=stop +=faster -=slower");
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'w': targetSpeed =  50; currentSteer =   0; break;
      case 's': targetSpeed = -50; currentSteer =   0; break;
      case 'a': targetSpeed =   0; currentSteer = -50; break;
      case 'd': targetSpeed =   0; currentSteer =  50; break;
      case 'q': targetSpeed =  50; currentSteer = -50; break;
      case 'e': targetSpeed =  50; currentSteer =  50; break;
      case 'x': targetSpeed =   0; currentSteer =   0; break;
      case '+': targetSpeed += 10; if (targetSpeed >  300) targetSpeed =  300; break;
      case '-': targetSpeed -= 10; if (targetSpeed < -300) targetSpeed = -300; break;
    }
    Serial.printf("Target: speed=%d, steer=%d\n", targetSpeed, currentSteer);
  }

  if (millis() - lastSend >= 20) {
    lastSend = millis();
    if (currentSpeed < targetSpeed) { currentSpeed += 2; if (currentSpeed > targetSpeed) currentSpeed = targetSpeed; }
    if (currentSpeed > targetSpeed) { currentSpeed -= 2; if (currentSpeed < targetSpeed) currentSpeed = targetSpeed; }
    sendCommand((int16_t)currentSteer, (int16_t)currentSpeed);
  }

  if (readFeedback()) {
    if (millis() - lastPrint >= 500) {
      lastPrint = millis();
      Serial.printf("Speed L:%d R:%d | Bat:%.1fV | Temp:%.1fC\n",
                    feedback.speedL_meas, feedback.speedR_meas,
                    feedback.batVoltage / 100.0,
                    feedback.boardTemp / 10.0);
    }
  }
}
