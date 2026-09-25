#include <SPI.h>
#include <mcp2515.h>
MCP2515 canController(10, 8000000);
uint32_t readTransaction(const uint8_t *d) {
  uint32_t n=0;
  for (uint8_t i=0;i<4;i++) n |= static_cast<uint32_t>(d[2+i]) << (8*i);
  return n;
}
void beginCAN(bool silent) {
  pinMode(10, OUTPUT); digitalWrite(10, HIGH); SPI.begin();
  if (canController.reset()!=MCP2515::ERROR_OK ||
      canController.setBitrate(CAN_125KBPS, MCP_8MHZ)!=MCP2515::ERROR_OK ||
      (silent ? canController.setListenOnlyMode() : canController.setNormalMode())!=MCP2515::ERROR_OK) {
    Serial.println(F("CAN INIT ERROR")); while (true) delay(1000);
  }
}

void setup() {
  Serial.begin(115200); beginCAN(true);
  Serial.println(F("EXP12 listen-only: ms,kind,transaction,LED,result (255=request)"));
}
void loop() {
  if(canController.getErrorFlags() & 0xC0) {
    Serial.println(F("RX_OVERFLOW: trace incomplete")); canController.clearRXnOVRFlags();
  }
  struct can_frame f;
  for(uint8_t i=0;i<8;i++) {
    if(canController.readMessage(&f)!=MCP2515::ERROR_OK) break;
    if(f.can_dlc!=8 || f.data[1]!=1 || f.data[6]>1) continue;
    bool request=f.can_id==0x320 && f.data[0]==0xC1 && f.data[7]==0;
    bool reply=f.can_id==0x321 && f.data[0]==0xD1 && f.data[7]<=2;
    if(!request && !reply) continue;
    Serial.print(millis()); Serial.print(','); Serial.print(request ? F("REQ") : F("RSP"));
    Serial.print(','); Serial.print(readTransaction(f.data));
    Serial.print(','); Serial.print(f.data[6]); Serial.print(',');
    Serial.println(request ? 255 : f.data[7]);
  }
}
