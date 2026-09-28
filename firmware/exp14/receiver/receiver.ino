#include <stdint.h>
#include <string.h>
// Educational fixed-length protocol, NOT ISO-TP.
class Assembly {
public:
  enum Result : uint8_t { NONE=0, COMPLETE=1, ORDER_ERROR=2, TIMEOUT=3, FORMAT_ERROR=4 };
  bool active=false, hasPublished=false;
  uint16_t transfer=0, publishedTransfer=0;
  uint8_t next=0, published[24]={};
  uint32_t completed=0;
  Result tick(uint32_t now) {
    if(active && static_cast<uint32_t>(now-lastPart)>=300) return fail(TIMEOUT);
    return NONE;
  }
  Result feed(uint32_t now,uint8_t dlc,const uint8_t *d) {
    Result expired=tick(now);
    if(expired!=NONE) return expired; // Expired frame is discarded, not reused.
    if(dlc!=8 || d[0]!=0xE4 || d[3]>=6)
      return active ? fail(FORMAT_ERROR) : NONE;
    uint16_t id=static_cast<uint16_t>(d[1]) | (static_cast<uint16_t>(d[2])<<8);
    if(!active) {
      if(d[3]!=0) return NONE; // A transfer must start at index zero.
      active=true; transfer=id; next=0;
    }
    if(id!=transfer || d[3]!=next) return fail(ORDER_ERROR);
    memcpy(pending+4*next,d+4,4); next++; lastPart=now;
    if(next!=6) return NONE;
    memcpy(published,pending,24); publishedTransfer=transfer;
    hasPublished=true; completed++; active=false; return COMPLETE;
  }
private:
  uint8_t pending[24]={};
  uint32_t lastPart=0;
  Result fail(Result why) { active=false; return why; }
};
#ifndef EXP14_HOST_TEST
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

Assembly assembly;
uint8_t mode=0;
void report(uint8_t result) {
  if(result==Assembly::NONE) return;
  Serial.print(F("RESULT transfer=")); Serial.print(assembly.transfer);
  Serial.print(F(" code=")); Serial.print(result);
  Serial.print(F(" published="));
  if(assembly.hasPublished) Serial.print(assembly.publishedTransfer); else Serial.print(F("NONE"));
  Serial.print(F(" completed=")); Serial.println(assembly.completed);
  if(result==Assembly::COMPLETE) {
    digitalWrite(7,(assembly.completed & 1) ? HIGH : LOW);
    Serial.print(F("MESSAGE=")); Serial.write(assembly.published,24); Serial.println();
  }
  if(canController.getStatus() & 0x04) { Serial.println(F("STATUS_TX_BUSY")); return; }
  struct can_frame f={}; f.can_id=0x361; f.can_dlc=4;
  f.data[0]=0xD4; f.data[1]=assembly.transfer; f.data[2]=assembly.transfer>>8; f.data[3]=result;
  if(canController.sendMessage(MCP2515::TXB0,&f)!=MCP2515::ERROR_OK)
    Serial.println(F("STATUS_TX_API_ERROR"));
}
void setup() {
  Serial.begin(115200); pinMode(2,INPUT_PULLUP); pinMode(7,OUTPUT); digitalWrite(7,LOW);
  beginCAN(false);
  Serial.println(F("EXP14 MODE=0 (0=normal, 1=drop index2, 2=drop index5)"));
}
void loop() {
  report(assembly.tick(millis()));
  if(pressed()) {
    if(assembly.active) Serial.println(F("MODE_BLOCKED: transfer active"));
    else { mode=(mode+1)%3; Serial.print(F("MODE=")); Serial.println(mode); }
  }
  checkOverflow();
  struct can_frame f;
  for(uint8_t i=0;i<8;i++) {
    if(canController.readMessage(&f)!=MCP2515::ERROR_OK) break;
    if(f.can_id!=0x360) continue;
    if(f.can_dlc==8 && f.data[0]==0xE4 &&
       ((mode==1 && f.data[3]==2) || (mode==2 && f.data[3]==5))) {
      Serial.print(F("APP_DROP index=")); Serial.println(f.data[3]); continue;
    }
    report(assembly.feed(millis(),f.can_dlc,f.data));
  }
}
#endif
