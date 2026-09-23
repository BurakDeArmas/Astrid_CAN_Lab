#include <SPI.h>
#include <mcp2515.h>
#define MCP_CLOCK MCP_8MHZ
MCP2515 canController(10, 8000000);

// EXP10 protocol: version, counter, signed temp*10 LE, unsigned mV LE, flags, reserved.
uint16_t readLE(const uint8_t *p) { return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8); }
void writeLE(uint8_t *p, uint16_t value) { p[0]=value & 255; p[1]=value >> 8; }
int32_t signed16(uint16_t raw) { return raw < 0x8000 ? static_cast<int32_t>(raw) : static_cast<int32_t>(raw)-65536L; }
void initCan(bool silent) {
  Serial.begin(115200); pinMode(10,OUTPUT); digitalWrite(10,HIGH); SPI.begin();
  if(canController.reset()!=MCP2515::ERROR_OK || canController.setBitrate(CAN_125KBPS,MCP_CLOCK)!=MCP2515::ERROR_OK ||
     (silent ? canController.setListenOnlyMode() : canController.setNormalMode())!=MCP2515::ERROR_OK) {
    Serial.println(F("CAN INIT ERROR")); while(true) delay(1000);
  }
}

void setup() { initCan(true); Serial.println(F("EXP10 observer LISTEN ONLY: ms ID DLC HEX_BYTES")); }
void loop() {
  struct can_frame f; if(canController.readMessage(&f)!=MCP2515::ERROR_OK) return;
  if(f.can_id!=0x300) return;
  Serial.print(millis()); Serial.print(F(" 300 ")); Serial.print(f.can_dlc);
  for(uint8_t i=0;i<f.can_dlc && i<8;i++) { Serial.print(' '); if(f.data[i]<16) Serial.print('0'); Serial.print(f.data[i],HEX); }
  Serial.println();
}
