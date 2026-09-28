#include <SPI.h>
#include <mcp2515.h>
MCP2515 canController(10, 8000000);
const uint32_t VALUE_ID=0x350;
const uint8_t VALUE_DLC=4;
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
  Serial.println(F("EXP13 listen-only: ms,type,id_hex,dlc,payload_hex"));
}
void loop() {
  checkOverflow();
  struct can_frame f;
  for(uint8_t i=0;i<8;i++) {
    if(canController.readMessage(&f)!=MCP2515::ERROR_OK) break;
    if(f.can_id!=VALUE_ID && f.can_id!=(VALUE_ID | CAN_RTR_FLAG)) continue;
    bool remote=(f.can_id & CAN_RTR_FLAG)!=0;
    Serial.print(millis()); Serial.print(','); Serial.print(remote ? F("RTR") : F("DATA"));
    Serial.print(','); Serial.print(f.can_id & CAN_SFF_MASK,HEX);
    Serial.print(','); Serial.print(f.can_dlc); Serial.print(',');
    if(remote) Serial.print(F("NO_PAYLOAD"));
    else for(uint8_t j=0;j<f.can_dlc;j++) {
      if(j) Serial.print(' ');
      if(f.data[j]<16) Serial.print('0');
      Serial.print(f.data[j],HEX);
    }
    Serial.println();
  }
}
