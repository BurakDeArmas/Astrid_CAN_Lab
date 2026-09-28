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
bool pressed() {
  static bool candidate=HIGH,stable=HIGH;
  static uint32_t edge=0;
  bool raw=digitalRead(2);
  if(raw!=candidate) { candidate=raw; edge=millis(); }
  if(millis()-edge>=30 && stable!=candidate) {
    stable=candidate; return stable==LOW;
  }
  return false;
}

uint8_t mode=0;
uint16_t sample=0;
uint32_t lastPublish=0;
void publishValue() {
  if(canController.getStatus() & 0x04) { Serial.println(F("DATA_TX_BUSY")); return; }
  uint16_t raw=analogRead(A0);
  struct can_frame f={}; f.can_id=VALUE_ID; f.can_dlc=VALUE_DLC;
  f.data[0]=raw; f.data[1]=raw>>8;
  f.data[2]=sample; f.data[3]=sample>>8;
  MCP2515::ERROR result=canController.sendMessage(MCP2515::TXB0,&f);
  Serial.print(F("DATA_ATTEMPT sample=")); Serial.print(sample);
  Serial.print(F(" ADC=")); Serial.print(raw);
  Serial.print(F(" api=")); Serial.println(static_cast<uint8_t>(result));
  sample++; // Counts attempts, not acknowledgements or confirmed delivery.
}
void setup() {
  Serial.begin(115200); pinMode(2,INPUT_PULLUP); beginCAN(false);
  Serial.println(F("EXP13 responder MODE=0: 0=reply RTR, 1=suppress, 2=periodic only"));
}
void loop() {
  if(pressed()) {
    mode=(mode+1)%3; lastPublish=millis();
    Serial.print(F("MODE=")); Serial.println(mode);
  }
  checkOverflow();
  struct can_frame f;
  for(uint8_t i=0;i<8;i++) {
    if(canController.readMessage(&f)!=MCP2515::ERROR_OK) break;
    if(f.can_id!=(VALUE_ID | CAN_RTR_FLAG)) continue;
    Serial.print(F("RTR_RECEIVED dlc=")); Serial.println(f.can_dlc);
    // Never inspect f.data[] on an RTR frame: there is no received payload.
    if(f.can_dlc!=VALUE_DLC) { Serial.println(F("DLC_MISMATCH: ignored")); continue; }
    if(mode==0) publishValue();
    else Serial.println(F("RTR_IGNORED_BY_APPLICATION"));
  }
  if(mode==2 && millis()-lastPublish>=250) { lastPublish=millis(); publishValue(); }
}
