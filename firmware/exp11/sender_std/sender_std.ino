#include <SPI.h>
#include <mcp2515.h>
#define MCP_CLOCK MCP_8MHZ
MCP2515 canController(10, 8000000);
const uint32_t ids[] = {0x123, 0x124, 0x12F};
uint8_t slot = 0;
uint32_t sequence = 0, submitted = 0, busy = 0, flagged = 0;
uint32_t lastSendUs = 0, lastReportMs = 0;
void setup() {
  Serial.begin(115200); pinMode(10, OUTPUT); digitalWrite(10, HIGH); SPI.begin();
  if (canController.reset() != MCP2515::ERROR_OK ||
      canController.setBitrate(CAN_125KBPS, MCP_CLOCK) != MCP2515::ERROR_OK ||
      canController.setNormalMode() != MCP2515::ERROR_OK) {
    Serial.println(F("CAN INIT ERROR")); while (true) delay(1000);
  }
  Serial.println(F("EXP11 sender | ms,submitted,busy,flagged (cumulative)"));
}
void loop() {
  struct can_frame discard;
  for (uint8_t i=0; i<4; i++) if (canController.readMessage(&discard) != MCP2515::ERROR_OK) break;
  const uint32_t nowUs = micros();
  if (nowUs - lastSendUs >= 33333UL) {
    lastSendUs = nowUs;
    if (canController.getStatus() & 0x04) busy++;
    else {
      struct can_frame frame = {};
      frame.can_id = ids[slot]; frame.can_dlc = 6;
      frame.data[0] = 0xAB; frame.data[1] = 1;
      for (uint8_t i=0; i<4; i++) frame.data[2+i] = sequence >> (8*i);
      if (canController.sendMessage(MCP2515::TXB0, &frame) != MCP2515::ERROR_OK) flagged++;
      sequence++; submitted++; slot = (slot+1)%3;
    }
  }
  const uint32_t now = millis();
  if (now-lastReportMs >= 1000) {
    lastReportMs = now;
    Serial.print(now); Serial.print(','); Serial.print(submitted);
    Serial.print(','); Serial.print(busy); Serial.print(','); Serial.println(flagged);
  }
}
