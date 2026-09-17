#include <SPI.h>
#include <mcp2515.h>
#define MCP_CLOCK MCP_8MHZ
MCP2515 canController(10, 8000000);
uint8_t mode = 0;
uint32_t counts[6] = {}, readCount = 0, kept = 0, dropped = 0, unexpected = 0, overflows = 0;
uint32_t lastReportMs = 0;
const uint16_t ids[] = {0x100, 0x101, 0x10F, 0x200, 0x201, 0x20F};

// Set mask MIDE bit: compare frame format against each filter's EXIDE=0.
// autowp setFilterMask(false, mask) writes SID correctly but leaves MIDE=0.
// Read/write mask SIDL in configuration mode: RXM0SIDL=0x21 / RXM1SIDL=0x25.
void requireStandard(uint8_t address) {
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  digitalWrite(10, LOW);
  SPI.transfer(0x03); SPI.transfer(address);
  const uint8_t value = SPI.transfer(0);
  digitalWrite(10, HIGH);
  digitalWrite(10, LOW);
  SPI.transfer(0x02); SPI.transfer(address); SPI.transfer(value | 0x08);
  digitalWrite(10, HIGH); SPI.endTransaction();
}

bool configure() {
  // Reset discards queued frames so modes have separate measurement windows.
  if (canController.reset() != MCP2515::ERROR_OK ||
      canController.setBitrate(CAN_125KBPS, MCP_CLOCK) != MCP2515::ERROR_OK) return false;
  const uint16_t mask = mode == 1 ? 0x7FF : mode == 2 ? 0x7F0 : 0;
  const uint16_t filter = mode == 1 || mode == 2 ? 0x100 : 0;
  if (canController.setFilterMask(MCP2515::MASK0, false, mask) != MCP2515::ERROR_OK ||
      canController.setFilterMask(MCP2515::MASK1, false, mask) != MCP2515::ERROR_OK) return false;
  const MCP2515::RXF filters[] = {MCP2515::RXF0, MCP2515::RXF1, MCP2515::RXF2,
                                MCP2515::RXF3, MCP2515::RXF4, MCP2515::RXF5};
  for (uint8_t i=0; i<6; i++)
    if (canController.setFilter(filters[i], false, filter) != MCP2515::ERROR_OK) return false;
  requireStandard(0x21); requireStandard(0x25);
  if (canController.setNormalMode() != MCP2515::ERROR_OK) return false;
  for (uint8_t i=0; i<6; i++) counts[i] = 0;
  readCount = kept = dropped = unexpected = overflows = 0;
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
  Serial.println(F("EXP07: ms,mode,read,kept,sw_drop,id100,id101,id10F,id200,id201,id20F,unexpected,ovr"));
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
    readCount++;
    // Count every frame delivered by hardware BEFORE any software rejection.
    uint8_t index = 6;
    for (uint8_t j=0; j<6; j++) if (frame.can_id == ids[j]) index = j;
    if (index < 6) counts[index]++;
    if (index == 6 || frame.can_dlc != 6 || frame.data[0] != 0xA7 ||
        frame.data[1] != (index < 3 ? 1 : 2)) { unexpected++; continue; }
    if (mode == 3 && frame.can_id != 0x100) { dropped++; continue; }
    kept++;
  }
  const uint32_t now = millis();
  if (now-lastReportMs < 1000) return;
  Serial.print(now-lastReportMs); lastReportMs = now;
  Serial.print(','); Serial.print(mode); Serial.print(','); Serial.print(readCount);
  Serial.print(','); Serial.print(kept); Serial.print(','); Serial.print(dropped);
  for (uint8_t i=0; i<6; i++) { Serial.print(','); Serial.print(counts[i]); counts[i]=0; }
  Serial.print(','); Serial.print(unexpected); Serial.print(','); Serial.println(overflows);
  readCount = kept = dropped = unexpected = overflows = 0;
}
