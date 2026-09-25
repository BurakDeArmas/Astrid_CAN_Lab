#include <stdint.h>
// One sender, one outstanding transaction, no independent resets in a session.
class CommandCache {
public:
  bool valid=false, led=false;
  uint32_t last=0, applied=0;
  uint8_t accept(uint32_t id, bool value) {
    if(valid && id==last) return value==led ? 1 : 2;
    valid=true; last=id; led=value; applied++; return 0;
  }
};
#ifndef EXP12_HOST_TEST
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
bool pressed() {
  static bool candidate=HIGH, stable=HIGH;
  static uint32_t edge=0;
  bool raw=digitalRead(2);
  if (raw!=candidate) { candidate=raw; edge=millis(); }
  if (millis()-edge>=30 && stable!=candidate) {
    stable=candidate; return stable==LOW;
  }
  return false;
}

CommandCache cache;
uint8_t mode=0;
void setup() {
  Serial.begin(115200); pinMode(2,INPUT_PULLUP); pinMode(7,OUTPUT); digitalWrite(7,LOW);
  beginCAN(false);
  Serial.println(F("EXP12 receiver MODE=0 (0=reply, 1=drop first, 2=drop all)"));
}
void loop() {
  if(pressed()) { mode=(mode+1)%3; Serial.print(F("MODE=")); Serial.println(mode); }
  struct can_frame f;
  for(uint8_t i=0;i<8;i++) {
    if(canController.readMessage(&f)!=MCP2515::ERROR_OK) break;
    if(f.can_id!=0x320 || f.can_dlc!=8 || f.data[0]!=0xC1 || f.data[1]!=1 ||
       f.data[6]>1 || f.data[7]!=0) continue;
    uint32_t id=readTransaction(f.data);
    uint8_t result=cache.accept(id,f.data[6]);
    if(result==0) digitalWrite(7,cache.led ? HIGH : LOW);
    Serial.print(F("REQUEST tx=")); Serial.print(id);
    Serial.print(F(" result=")); Serial.print(result);
    Serial.print(F(" applied_total=")); Serial.println(cache.applied);
    if(mode==2 || (mode==1 && result==0)) { Serial.println(F("REPLY_SUPPRESSED")); continue; }
    if(canController.getStatus() & 0x04) { Serial.println(F("REPLY_TX_BUSY")); continue; }
    f.can_id=0x321; f.data[0]=0xD1; f.data[6]=cache.led; f.data[7]=result;
    if(canController.sendMessage(MCP2515::TXB0,&f)!=MCP2515::ERROR_OK)
      Serial.println(F("REPLY_TX_API_ERROR"));
  }
}
#endif
