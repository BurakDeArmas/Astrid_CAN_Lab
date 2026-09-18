#include <SPI.h>
#include <mcp2515.h>
#define MCP_CLOCK MCP_8MHZ
MCP2515 canController(10, 8000000);
uint32_t received = 0, sameSequence = 0, other = 0, overflows = 0;
uint32_t lastSequence = 0, lastReportMs = 0;
bool seen = false;
void setup() {
  Serial.begin(115200); pinMode(10, OUTPUT); digitalWrite(10, HIGH); SPI.begin();
  if (canController.reset() != MCP2515::ERROR_OK ||
      canController.setBitrate(CAN_125KBPS, MCP_CLOCK) != MCP2515::ERROR_OK ||
      canController.setListenOnlyMode() != MCP2515::ERROR_OK) {
    Serial.println(F("CAN LISTEN INIT ERROR")); while (true) delay(1000);
  }
  Serial.println(F("EXP08 LISTEN ONLY - NO ACK: window_ms,received,same_sequence,other,ovr_samples"));
}
void loop() {
  if (canController.getErrorFlags() & 0xC0) { overflows++; canController.clearRXnOVRFlags(); }
  struct can_frame frame;
  for (uint8_t i=0; i<16; i++) {
    if (canController.readMessage(&frame) != MCP2515::ERROR_OK) break;
    if (frame.can_id != 0x180 || frame.can_dlc != 5 || frame.data[0] != 0xA8) { other++; continue; }
    uint32_t sequence = 0;
    for (uint8_t j=0; j<4; j++) sequence |= static_cast<uint32_t>(frame.data[1+j]) << (8*j);
    received++;
    if (seen && sequence == lastSequence) sameSequence++;
    seen = true; lastSequence = sequence;
  }
  const uint32_t now = millis();
  if (now-lastReportMs >= 1000) {
    Serial.print(now-lastReportMs); lastReportMs = now;
    Serial.print(','); Serial.print(received); Serial.print(','); Serial.print(sameSequence);
    Serial.print(','); Serial.print(other); Serial.print(','); Serial.println(overflows);
    received = sameSequence = other = overflows = 0;
  }
}
