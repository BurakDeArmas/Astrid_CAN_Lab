#include <SPI.h>
#include <mcp2515.h>
#include <Servo.h>

// Uno B: servo D5, fault LED D7, MCP2515 CS D10.
#define MCP_CLOCK MCP_8MHZ
constexpr uint16_t COMMAND_ID = 0x110;
constexpr uint32_t TIMEOUT_MS = 500;
constexpr uint8_t SAFE_ANGLE = 90;
MCP2515 canController(10, 8000000);
Servo actuator;
bool synchronized = false;
bool running = false;
uint8_t lastCounter = 0;
uint8_t validSteps = 0;
uint32_t lastProgressMs = 0;

void safePosition() {
  running = false;
  digitalWrite(7, HIGH);
  actuator.write(SAFE_ANGLE);
}

void checkTimeout() {
  if (synchronized && millis() - lastProgressMs >= TIMEOUT_MS) {
    synchronized = false;
    validSteps = 0;
    safePosition();
    Serial.println(F("TIMEOUT | servo=90"));
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(7, OUTPUT);
  actuator.write(SAFE_ANGLE);
  actuator.attach(5);
  safePosition();
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  SPI.begin();
  if (canController.reset() != MCP2515::ERROR_OK ||
      canController.setBitrate(CAN_125KBPS, MCP_CLOCK) != MCP2515::ERROR_OK ||
      canController.setNormalMode() != MCP2515::ERROR_OK) {
    Serial.println(F("CAN INIT ERROR | servo=90"));
    while (true) { delay(1000); }
  }
  Serial.println(F("EXP04 UNO B | waiting | servo=90"));
}

void processCommands() {
  checkTimeout();
  struct can_frame frame;
  if (canController.readMessage(&frame) != MCP2515::ERROR_OK) return;
  // Exact comparison also excludes extended, RTR and error flags.
  if (frame.can_id != COMMAND_ID) return;
  if (frame.can_dlc != 4 || frame.data[0] != 0xA3) {
    validSteps = 0;
    safePosition();
    Serial.println(F("INVALID FORMAT | servo=90"));
    return;
  }
  const uint16_t raw = static_cast<uint16_t>(frame.data[2]) |
                       (static_cast<uint16_t>(frame.data[3]) << 8);
  if (raw > 1023) {
    validSteps = 0;
    safePosition();
    Serial.println(F("INVALID ADC | servo=90"));
    return;
  }
  const uint8_t current = frame.data[1];
  if (!synchronized) {
    synchronized = true;
    lastCounter = current;
    lastProgressMs = millis();
    validSteps = 0;
    safePosition();
    Serial.println(F("SYNC | waiting for 3 counter steps"));
    return;
  }
  const uint8_t expected = static_cast<uint8_t>(lastCounter + 1U);
  lastCounter = current;
  if (current != expected) {
    validSteps = 0;
    safePosition();
    Serial.println(F("COUNTER FAULT | servo=90"));
    return;
  }
  lastProgressMs = millis();
  if (validSteps < 3) validSteps++;
  if (validSteps < 3) return;
  const uint8_t angle = 30U + (static_cast<uint32_t>(raw) * 120U / 1023U);
  if (!running) Serial.println(F("ACTIVE"));
  running = true;
  digitalWrite(7, LOW);
  actuator.write(angle);
  Serial.print(F("RX | ADC=")); Serial.print(raw);
  Serial.print(F(" | angle=")); Serial.println(angle);
}

// Status: marker, sequence, applied command angle, active (0/1).
// This reports the software command, not measured shaft position.
void publishStatus() {
  static uint32_t lastStatusMs = 0;
  static uint8_t statusCounter = 0;
  const uint32_t now = millis();
  if (now - lastStatusMs < 100) return;
  lastStatusMs = now;
  struct can_frame status = {};
  status.can_id = 0x120;
  status.can_dlc = 4;
  status.data[0] = 0xB4;
  status.data[1] = statusCounter;
  status.data[2] = actuator.read();
  status.data[3] = running ? 1 : 0;
  const MCP2515::ERROR result = canController.sendMessage(&status);
  if (result == MCP2515::ERROR_OK) statusCounter++;
  else Serial.println(F("STATUS TX ERROR"));
}

void loop() {
  processCommands();
  publishStatus();
}
