#include <SPI.h>
#include <mcp2515.h>
#define MCP_CLOCK MCP_8MHZ
MCP2515 canController(10, 8000000);
uint32_t submitted = 0, completed = 0, busy = 0, flagged = 0;
uint32_t lastSendMs = 0, lastReportMs = 0;

void setup() {
  Serial.begin(115200); pinMode(10, OUTPUT); digitalWrite(10, HIGH); SPI.begin();
  if (canController.reset() != MCP2515::ERROR_OK ||
      canController.setBitrate(CAN_125KBPS, MCP_CLOCK) != MCP2515::ERROR_OK ||
      canController.setNormalMode() != MCP2515::ERROR_OK) {
    Serial.println(F("CAN INIT ERROR")); while (true) delay(1000);
  }
  Serial.println(F("EXP08 sender: ms,submitted,tx_complete,busy,api_flagged,TXREQ,TEC,REC,EFLG,state"));
}

void loop() {
  // Only TXB0 is used. In normal mode TX0IF signals successful transmission.
  if (canController.getInterrupts() & 0x04) {
    completed++;
    canController.clearTXInterrupts();
  }
  const uint32_t now = millis();
  if (now-lastSendMs >= 500) {
    lastSendMs = now;
    if (canController.getStatus() & 0x04) busy++;
    else {
      // Recheck a completion that may have arrived since the top of loop.
      if (canController.getInterrupts() & 0x04) {
        completed++; canController.clearTXInterrupts();
      }
      struct can_frame frame = {};
      frame.can_id = 0x180; frame.can_dlc = 5;
      frame.data[0] = 0xA8;
      for (uint8_t i=0; i<4; i++) frame.data[1+i] = submitted >> (8*i);
      const MCP2515::ERROR result = canController.sendMessage(MCP2515::TXB0, &frame);
      submitted++;
      if (result != MCP2515::ERROR_OK) flagged++;
    }
  }
  if (now-lastReportMs < 250) return;
  lastReportMs = now;
  const uint8_t flags = canController.getErrorFlags();
  Serial.print(now); Serial.print(','); Serial.print(submitted);
  Serial.print(','); Serial.print(completed); Serial.print(','); Serial.print(busy);
  Serial.print(','); Serial.print(flagged);
  Serial.print(','); Serial.print((canController.getStatus() & 0x04) ? 1 : 0);
  Serial.print(','); Serial.print(canController.errorCountTX());
  Serial.print(','); Serial.print(canController.errorCountRX());
  Serial.print(','); Serial.print(flags, HEX); Serial.print(',');
  if (flags & 0x20) Serial.println(F("BUS_OFF"));
  else if (flags & 0x18) Serial.println(F("ERROR_PASSIVE"));
  else if (flags & 0x01) Serial.println(F("ERROR_ACTIVE_WARNING"));
  else Serial.println(F("ERROR_ACTIVE"));
}
