#include <SPI.h>
#include <mcp2515.h>
MCP2515 canController(10, 8000000);
void beginCAN(bool silent) {
  pinMode(10,OUTPUT); digitalWrite(10,HIGH); SPI.begin();
  if(canController.reset()!=MCP2515::ERROR_OK ||
     canController.setBitrate(CAN_125KBPS,MCP_8MHZ)!=MCP2515::ERROR_OK ||
     (silent ? canController.setListenOnlyMode() : canController.setNormalMode())!=MCP2515::ERROR_OK) {
    Serial.println(F("CAN INIT ERROR")); while(true) delay(1000);
  }
}
void checkOverflow() {
  if(canController.getErrorFlags() & 0xC0) {
    Serial.println(F("RX_OVERFLOW: observations incomplete"));
    canController.clearRXnOVRFlags();
  }
}

void setup() {
  Serial.begin(115200); beginCAN(true);
  Serial.println(F("EXP15 listen-only: ms,id_hex,dlc,payload_hex"));
}
void loop() {
  checkOverflow();
  struct can_frame f;
  for(uint8_t i=0;i<8;i++) {
    if(canController.readMessage(&f)!=MCP2515::ERROR_OK) break;
    if(f.can_id!=0x380 && f.can_id!=0x381) continue;
    Serial.print(millis()); Serial.print(','); Serial.print(f.can_id,HEX);
    Serial.print(','); Serial.print(f.can_dlc); Serial.print(',');
    for(uint8_t j=0;j<f.can_dlc;j++) {
      if(j) Serial.print(' ');
      if(f.data[j]<16) Serial.print('0'); Serial.print(f.data[j],HEX);
    }
    Serial.println();
  }
}
