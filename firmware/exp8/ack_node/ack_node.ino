#include <SPI.h>
#include <mcp2515.h>
#define MCP_CLOCK MCP_8MHZ
MCP2515 canController(10, 8000000);
uint8_t mode = 0;
uint32_t received = 0, other = 0, lastReportMs = 0;

// Same three modes on every press: accept+ACK, filter-out+ACK, listen-only.
bool configure() {
  if (canController.reset() != MCP2515::ERROR_OK ||
      canController.setBitrate(CAN_125KBPS, MCP_CLOCK) != MCP2515::ERROR_OK) return false;
  const uint16_t filter = mode == 1 ? 0x700 : 0x180;
  if (canController.setFilterMask(MCP2515::MASK0, false, 0x7FF) != MCP2515::ERROR_OK ||
      canController.setFilterMask(MCP2515::MASK1, false, 0x7FF) != MCP2515::ERROR_OK) return false;
  const MCP2515::RXF filters[] = {MCP2515::RXF0, MCP2515::RXF1, MCP2515::RXF2,
                                MCP2515::RXF3, MCP2515::RXF4, MCP2515::RXF5};
  for (uint8_t i=0; i<6; i++)
    if (canController.setFilter(filters[i], false, filter) != MCP2515::ERROR_OK) return false;
  // This isolated experiment sends standard frames only. No extended-ID claim.
  const MCP2515::ERROR result = mode == 2 ? canController.setListenOnlyMode() : canController.setNormalMode();
  if (result != MCP2515::ERROR_OK) return false;
  received = other = 0; lastReportMs = millis();
  Serial.print(F("MODE=")); Serial.print(mode); Serial.print(' ');
  if (mode == 0) Serial.println(F("NORMAL_ACCEPT_ACK"));
  else if (mode == 1) Serial.println(F("NORMAL_FILTER_OUT_ACK"));
  else Serial.println(F("LISTEN_ONLY_NO_ACK"));
  return true;
}

void setup() {
  Serial.begin(115200); pinMode(2, INPUT_PULLUP);
  pinMode(10, OUTPUT); digitalWrite(10, HIGH); SPI.begin();
  if (!configure()) { Serial.println(F("CAN CONFIG ERROR")); while (true) delay(1000); }
  Serial.println(F("EXP08 ack node: window_ms,mode,received,other"));
}

void loop() {
  static bool candidate = HIGH, stable = HIGH;
  static uint32_t edgeMs = 0;
  const bool raw = digitalRead(2);
  if (raw != candidate) { candidate = raw; edgeMs = millis(); }
  if (millis()-edgeMs >= 30 && stable != candidate) {
    stable = candidate;
    if (stable == LOW) {
      mode = (mode+1)%3;
      if (!configure()) { Serial.println(F("CAN CONFIG ERROR")); while (true) delay(1000); }
    }
  }
  struct can_frame frame;
  for (uint8_t i=0; i<8; i++) {
    if (canController.readMessage(&frame) != MCP2515::ERROR_OK) break;
    if (frame.can_id == 0x180 && frame.can_dlc == 5 && frame.data[0] == 0xA8) received++;
    else other++;
  }
  const uint32_t now = millis();
  if (now-lastReportMs >= 1000) {
    Serial.print(now-lastReportMs); lastReportMs = now;
    Serial.print(','); Serial.print(mode); Serial.print(','); Serial.print(received);
    Serial.print(','); Serial.println(other); received = other = 0;
  }
}
