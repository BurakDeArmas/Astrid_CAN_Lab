#include <stdint.h>
class UdsDemo {
public:
  uint8_t setting=25;
  bool writesAllowed=true;
  uint32_t applied=0;
  // UDS payload only, excluding the ISO-TP PCI byte. Output capacity >=7.
  uint8_t handle(const uint8_t *q,uint8_t n,uint8_t *r) {
    if(n==0) return 0;
    uint8_t sid=q[0];
    if(sid!=0x22 && sid!=0x2E) return negative(sid,0x11,r);
    if(n!=(sid==0x22 ? 3 : 4)) return negative(sid,0x13,r);
    if(q[1]!=0x12 || q[2]!=0x34) return negative(sid,0x31,r);
    if(sid==0x22) { r[0]=0x62; r[1]=0x12; r[2]=0x34; r[3]=setting; return 4; }
    if(!writesAllowed) return negative(sid,0x22,r);
    if(q[3]>100) return negative(sid,0x31,r);
    setting=q[3]; applied++;
    r[0]=0x6E; r[1]=0x12; r[2]=0x34; return 3;
  }
private:
  uint8_t negative(uint8_t sid,uint8_t code,uint8_t *r) {
    r[0]=0x7F; r[1]=sid; r[2]=code; return 3;
  }
};
#ifndef EXP16_HOST_TEST
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

// This lab accepts only Classical CAN ISO-TP single frames padded to DLC8.
bool validSingleFrame(const struct can_frame &f) {
  return f.can_dlc==8 && f.data[0]>=1 && f.data[0]<=7;
}

UdsDemo service;
void setup() {
  Serial.begin(115200); pinMode(2,INPUT_PULLUP); pinMode(7,OUTPUT); digitalWrite(7,LOW);
  beginCAN(false);
  Serial.println(F("EXP16 ECU: DID=1234 setting=25 writesAllowed=1. D2 toggles permission."));
}
void loop() {
  if(pressed()) {
    service.writesAllowed=!service.writesAllowed;
    Serial.print(F("WRITES_ALLOWED=")); Serial.println(service.writesAllowed);
  }
  checkOverflow();
  struct can_frame f;
  for(uint8_t i=0;i<8;i++) {
    if(canController.readMessage(&f)!=MCP2515::ERROR_OK) break;
    if(f.can_id!=0x700) continue;
    if(!validSingleFrame(f)) { Serial.println(F("UNSUPPORTED_TRANSPORT: ignored")); continue; }
    struct can_frame reply={}; reply.can_id=0x708; reply.can_dlc=8;
    reply.data[0]=service.handle(f.data+1,f.data[0],reply.data+1);
    digitalWrite(7,service.setting>=50 ? HIGH : LOW);
    Serial.print(reply.data[1]==0x7F ? F("REJECTED NRC=0x") : F("ACCEPTED responseSID=0x"));
    Serial.print(reply.data[1]==0x7F ? reply.data[3] : reply.data[1],HEX);
    Serial.print(F(" setting=")); Serial.print(service.setting);
    Serial.print(F(" writes=")); Serial.println(service.applied);
    if(canController.getStatus() & 0x04) { Serial.println(F("RESPONSE_TX_BUSY: request already processed")); continue; }
    if(canController.sendMessage(MCP2515::TXB0,&reply)!=MCP2515::ERROR_OK)
      Serial.println(F("RESPONSE_TX_API_ERROR"));
  }
}
#endif
