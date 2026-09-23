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

uint32_t lastValid=0; bool fresh=false, synchronized=false; uint8_t lastCounter=0;
void setup() { initCan(false); Serial.println(F("EXP10 receiver: counter,temp_C,voltage_V,synthetic,sequence")); }
void loop() {
  if(fresh && millis()-lastValid>=500) { fresh=false; synchronized=false; Serial.println(F("STALE: no valid data for 500ms")); }
  struct can_frame f; if(canController.readMessage(&f)!=MCP2515::ERROR_OK) return;
  if(f.can_id!=0x300) return;
  if(f.can_dlc!=8 || f.data[0]!=1 || (f.data[6]&0xFE)!=0 || f.data[7]!=0) { Serial.println(F("REJECT FORMAT/VERSION")); return; }
  const int32_t temp=signed16(readLE(&f.data[2])); const uint16_t mv=readLE(&f.data[4]);
  if(temp < -400 || temp > 1250 || mv > 5000) { Serial.println(F("REJECT RANGE")); return; }
  const bool stepOK=!synchronized || f.data[1]==static_cast<uint8_t>(lastCounter+1U);
  const bool first=!synchronized;
  lastCounter=f.data[1]; synchronized=true;
  // An invalid sequence does not refresh data freshness.
  if(stepOK) { lastValid=millis(); fresh=true; }
  Serial.print(f.data[1]); Serial.print(','); Serial.print(temp/10.0f,1);
  Serial.print(','); Serial.print(mv/1000.0f,3); Serial.print(','); Serial.print(f.data[6]&1);
  Serial.print(','); Serial.println(first ? "SYNC" : stepOK ? "OK" : "COUNTER_ERROR");
}
