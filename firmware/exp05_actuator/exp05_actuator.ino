#include <SPI.h>
#include <mcp2515.h>
#include <Servo.h>
#include "CommandMonitor.h"

#define MCP_CLOCK MCP_8MHZ
MCP2515 canController(10, 8000000);
Servo actuator;
CommandMonitor monitor;

const char *stateName(uint8_t state) {
  switch (state) {
    case 0: return "WAITING";
    case 1: return "ACTIVE";
    case 2: return "COUNTER_ERROR";
    case 3: return "DATA_ERROR";
    case 4: return "TIMEOUT";
    default: return "RECOVERING";
  }
}

void applyOutputs() {
  static uint8_t previous = 255;
  actuator.write(monitor.angle);
  digitalWrite(7, monitor.state == CommandMonitor::ACTIVE ? LOW : HIGH);
  if (previous != monitor.state) {
    Serial.print(millis()); Serial.print(F(",STATE,"));
    Serial.print(stateName(monitor.state)); Serial.print(F(",ANGLE,"));
    Serial.println(monitor.angle);
    previous = monitor.state;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(7, OUTPUT);
  actuator.write(90); actuator.attach(5);
  applyOutputs();
  pinMode(10, OUTPUT); digitalWrite(10, HIGH); SPI.begin();
  if (canController.reset() != MCP2515::ERROR_OK ||
      canController.setBitrate(CAN_125KBPS, MCP_CLOCK) != MCP2515::ERROR_OK ||
      canController.setNormalMode() != MCP2515::ERROR_OK) {
    Serial.println(F("CAN INIT ERROR"));
    while (true) { delay(1000); }
  }
  Serial.println(F("EXP05 ACTUATOR | ms,event,state,angle"));
}

void loop() {
  monitor.tick(millis());
  applyOutputs();
  struct can_frame frame;
  for (uint8_t i = 0; i < 4; i++) {
    if (canController.readMessage(&frame) != MCP2515::ERROR_OK) break;
    if (frame.can_id == 0x130) {
      monitor.receive(millis(), frame.can_dlc, frame.data);
      applyOutputs();
    }
  }
  static uint32_t lastStatusMs = 0;
  static uint8_t counter = 0;
  const uint32_t now = millis();
  if (now - lastStatusMs < 100) return;
  lastStatusMs = now;
  struct can_frame status = {};
  status.can_id = 0x140; status.can_dlc = 4;
  status.data[0] = 0xB5; status.data[1] = counter;
  status.data[2] = monitor.angle; status.data[3] = monitor.state;
  if (canController.sendMessage(&status) == MCP2515::ERROR_OK) counter++;
  else { Serial.print(now); Serial.println(F(",STATUS_TX_ERROR")); }
}
