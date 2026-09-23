#include <SPI.h>
#include <mcp2515.h>
#define MCP_CLOCK MCP_8MHZ
MCP2515 canController(10, 8000000);
uint8_t mode=0;
uint32_t counts[6]={}, other=0, overflows=0, lastReportMs=0;
const uint32_t ids[]={0x123,0x124,0x12F,0x123 | CAN_EFF_FLAG,0x124 | CAN_EFF_FLAG,0x1ABCDE | CAN_EFF_FLAG};

bool configure() {
  // Reset discards queued frames so modes have separate measurement windows.
  if (canController.reset() != MCP2515::ERROR_OK ||
      canController.setBitrate(CAN_125KBPS, MCP_CLOCK) != MCP2515::ERROR_OK) return false;
  const bool extended = mode == 2 || mode == 3;
  const uint32_t mask = mode == 3 ? 0x1FFFFFFFUL : 0;
  const uint32_t filter = mode == 3 ? 0x123 : 0;
  if (canController.setFilterMask(MCP2515::MASK0, extended, mask) != MCP2515::ERROR_OK ||
      canController.setFilterMask(MCP2515::MASK1, extended, mask) != MCP2515::ERROR_OK) return false;
  const MCP2515::RXF filters[] = {MCP2515::RXF0, MCP2515::RXF1, MCP2515::RXF2,
                                MCP2515::RXF3, MCP2515::RXF4, MCP2515::RXF5};
  for (uint8_t i=0; i<6; i++)
    if (canController.setFilter(filters[i], mode == 0 ? (i % 2 == 1) : extended, filter) != MCP2515::ERROR_OK) return false;
  if (canController.setNormalMode() != MCP2515::ERROR_OK) return false;
  for (uint8_t i=0; i<6; i++) counts[i] = 0;
  other = overflows = 0;
  lastReportMs = millis();
  Serial.print(F("MODE=")); Serial.print(mode);
  Serial.print(F(" MASK=0x")); Serial.print(mask, HEX);
  Serial.print(F(" FILTER=0x")); Serial.println(filter, HEX);
  return true;
}

void fatal() {
  Serial.println(F("CAN CONFIG ERROR")); while (true) delay(1000);
}

void setup() {
  Serial.begin(115200); pinMode(2, INPUT_PULLUP);
  pinMode(10, OUTPUT); digitalWrite(10, HIGH); SPI.begin();
  if (!configure()) fatal();
  Serial.println(F("EXP11: window_ms,mode,STD123,STD124,STD12F,EXT123,EXT124,EXT1ABCDE,other,ovr"));
}

void loop() {
  static bool candidate = HIGH, stable = HIGH;
  static uint32_t edgeMs = 0;
  const bool raw = digitalRead(2);
  if (candidate != raw) { candidate = raw; edgeMs = millis(); }
  if (millis()-edgeMs >= 30 && stable != candidate) {
    stable = candidate;
    if (stable == LOW) { mode = (mode+1)%4; if (!configure()) fatal(); }
  }
  if (canController.getErrorFlags() & 0xC0) {
    overflows++; canController.clearRXnOVRFlags();
  }
  struct can_frame frame;
  for (uint8_t i=0; i<16; i++) {
    if (canController.readMessage(&frame) != MCP2515::ERROR_OK) break;
    uint8_t index=6;
    for(uint8_t j=0;j<6;j++) if(frame.can_id==ids[j]) index=j;
    if(index==6 || frame.can_dlc!=6 || frame.data[0]!=0xAB || frame.data[1]!=(index<3 ? 1 : 2)) { other++; continue; }
    counts[index]++;

  }
  const uint32_t now = millis();
  if (now-lastReportMs < 1000) return;
  Serial.print(now-lastReportMs); lastReportMs = now;
  Serial.print(','); Serial.print(mode);
  for(uint8_t i=0;i<6;i++) { Serial.print(','); Serial.print(counts[i]); counts[i]=0; }
  Serial.print(','); Serial.print(other); Serial.print(','); Serial.println(overflows);
  other=overflows=0;
}
