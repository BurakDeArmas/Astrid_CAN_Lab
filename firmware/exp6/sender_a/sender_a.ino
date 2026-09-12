#include <SPI.h>
#include <mcp2515.h>

#define MCP_CLOCK MCP_8MHZ
constexpr uint16_t MESSAGE_ID = 0x100;
constexpr uint8_t SOURCE = 1;
MCP2515 canController(10, 8000000);
const uint32_t periodsUs[] = {100000, 10000, 2000, 500};
uint8_t level = 0;
uint32_t sequence = 0, submitted = 0, busy = 0, flagged = 0;
uint32_t lastSendUs = 0, lastReportMs = 0;

void setup() {
  Serial.begin(115200);
  pinMode(2, INPUT_PULLUP);
  pinMode(10, OUTPUT); digitalWrite(10, HIGH); SPI.begin();
  if (canController.reset() != MCP2515::ERROR_OK ||
      canController.setBitrate(CAN_125KBPS, MCP_CLOCK) != MCP2515::ERROR_OK ||
      canController.setNormalMode() != MCP2515::ERROR_OK) {
    Serial.println(F("CAN INIT ERROR"));
    while (true) delay(1000);
  }
  Serial.println(F("EXP06 sender: ms,id,level,submitted,busy,flagged,TEC,REC,EFLG"));
}

void loop() {
  static bool candidate = HIGH, stable = HIGH;
  static uint32_t edgeMs = 0;
  const bool raw = digitalRead(2);
  if (raw != candidate) { candidate = raw; edgeMs = millis(); }
  if (millis() - edgeMs >= 30 && stable != candidate) {
    stable = candidate;
    if (stable == LOW) level = (level + 1) % 4;
  }
  struct can_frame discard;
  for (uint8_t i = 0; i < 4; i++) {
    if (canController.readMessage(&discard) != MCP2515::ERROR_OK) break;
  }
  const uint32_t nowUs = micros();
  if (nowUs - lastSendUs >= periodsUs[level]) {
    lastSendUs = nowUs;
    // MCP2515 READ STATUS bit 2 = TXB0.TXREQ. Use one buffer to preserve order.
    if (canController.getStatus() & 0x04) busy++;
    else {
      struct can_frame frame = {};
      frame.can_id = MESSAGE_ID; frame.can_dlc = 8;
      frame.data[0] = 0xA6; frame.data[1] = SOURCE;
      for (uint8_t i = 0; i < 4; i++) frame.data[2+i] = sequence >> (8*i);
      frame.data[6] = level; frame.data[7] = 0x5A;
      const MCP2515::ERROR result = canController.sendMessage(MCP2515::TXB0, &frame);
      // The API can flag an error after setting TXREQ. Never reuse that sequence.
      sequence++; submitted++;
      if (result != MCP2515::ERROR_OK) flagged++;
    }
  }
  const uint32_t now = millis();
  if (now - lastReportMs >= 1000) {
    lastReportMs = now;
    Serial.print(now); Serial.print(','); Serial.print(MESSAGE_ID, HEX);
    Serial.print(','); Serial.print(level); Serial.print(','); Serial.print(submitted);
    Serial.print(','); Serial.print(busy); Serial.print(','); Serial.print(flagged);
    Serial.print(','); Serial.print(canController.errorCountTX());
    Serial.print(','); Serial.print(canController.errorCountRX());
    Serial.print(','); Serial.println(canController.getErrorFlags(), HEX);
    canController.clearRXnOVRFlags();
  }
}
