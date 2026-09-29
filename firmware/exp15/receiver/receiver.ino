#include <stdint.h>
#include <string.h>
class IsoReceiver {
public:
  enum Error : uint8_t { OK, TIMEOUT, FORMAT, SEQUENCE, CAPACITY, UNEXPECTED };
  bool active=false, fcNeeded=false, waitingFcTx=false;
  uint8_t blockSize=2,stmin=50;
  uint8_t published[128]={};
  uint16_t publishedLength=0;
  uint32_t completed=0;
  Error error=OK;
  void tick(uint32_t now) {
    if(active && static_cast<uint32_t>(now-changed)>=1000) abort(TIMEOUT);
  }
  void feed(uint32_t now,uint8_t dlc,const uint8_t *d) {
    tick(now);
    if(dlc!=8) { abort(FORMAT); return; }
    uint8_t kind=d[0]>>4;
    if(kind==0 || kind==1) {
      if(active) { abort(UNEXPECTED); return; }
      error=OK;
      if(kind==0) {
        uint8_t n=d[0]&15;
        if(n==0 || n>7) { abort(FORMAT); return; }
        memcpy(published,d+1,n); publishedLength=n; completed++; return;
      }
      length=(static_cast<uint16_t>(d[0]&15)<<8) | d[1];
      if(length<=7) { abort(FORMAT); return; }
      if(length>128) { abort(CAPACITY); return; }
      memcpy(pending,d+2,6); offset=6; sequence=1; used=0;
      active=true; fcNeeded=true; waitingFcTx=true; changed=now; return;
    }
    if(kind!=2) { abort(FORMAT); return; }
    if(!active) return;
    if(waitingFcTx) { abort(UNEXPECTED); return; }
    if((d[0]&15)!=sequence) { abort(SEQUENCE); return; }
    uint8_t n=(length-offset>7) ? 7 : length-offset;
    memcpy(pending+offset,d+1,n); offset+=n; changed=now;
    sequence=(sequence+1)&15; used++;
    if(offset==length) {
      memcpy(published,pending,length); publishedLength=length; completed++;
      active=false; fcNeeded=false; waitingFcTx=false; return;
    }
    if(blockSize!=0 && used>=blockSize) { fcNeeded=true; waitingFcTx=true; }
  }
  void fcTransmitted(uint32_t now) {
    if(!active || !waitingFcTx) return;
    fcNeeded=false; waitingFcTx=false; used=0; changed=now;
  }
private:
  uint8_t pending[128]={},sequence=1,used=0;
  uint16_t length=0,offset=0;
  uint32_t changed=0;
  void abort(Error e) { error=e; active=false; fcNeeded=false; waitingFcTx=false; }
};
#ifndef EXP15_HOST_TEST
#include <SPI.h>
#include <mcp2515.h>
MCP2515 canController(10,8000000);
IsoReceiver transport;
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
bool hardwarePending=false;
void pollTx() {
  if(hardwarePending && (canController.getInterrupts() & 0x04)) {
    canController.clearTXInterrupts(); hardwarePending=false;
    transport.fcTransmitted(millis());
  }
}
void setup() {
  Serial.begin(115200); pinMode(2,INPUT_PULLUP); pinMode(7,OUTPUT); digitalWrite(7,LOW);
  beginCAN(false);
  Serial.println(F("EXP15 MODE=0: 0=BS2/ST50, 1=BS1/ST100, 2=no FC"));
}
void loop() {
  pollTx(); transport.tick(millis()); checkOverflow();
  if(pressed()) {
    if(transport.active || hardwarePending) Serial.println(F("MODE_BLOCKED"));
    else {
      mode=(mode+1)%3;
      transport.blockSize=(mode==1) ? 1 : 2;
      transport.stmin=(mode==1) ? 100 : 50;
      Serial.print(F("MODE=")); Serial.println(mode);
    }
  }
  struct can_frame f;
  for(uint8_t i=0;i<8;i++) {
    if(canController.readMessage(&f)!=MCP2515::ERROR_OK) break;
    pollTx();
    if(f.can_id==0x380) transport.feed(millis(),f.can_dlc,f.data);
  }
  if(transport.fcNeeded && !hardwarePending && mode!=2 && !(canController.getStatus() & 0x04)) {
    struct can_frame fc={}; fc.can_id=0x381; fc.can_dlc=8;
    fc.data[0]=0x30; fc.data[1]=transport.blockSize; fc.data[2]=transport.stmin;
    canController.clearTXInterrupts(); hardwarePending=true;
    MCP2515::ERROR e=canController.sendMessage(MCP2515::TXB0,&fc);
    Serial.print(F("FC_SUBMITTED BS=")); Serial.print(fc.data[1]);
    Serial.print(F(" STmin_ms=")); Serial.print(fc.data[2]);
    Serial.print(F(" api=")); Serial.println(static_cast<uint8_t>(e));
  }
  static uint32_t previous=0;
  static uint8_t previousError=0;
  if(previous!=transport.completed) {
    previous=transport.completed; digitalWrite(7,(previous & 1) ? HIGH : LOW);
    Serial.print(F("COMPLETE length=")); Serial.print(transport.publishedLength);
    Serial.print(F(" count=")); Serial.println(previous);
    Serial.print(F("MESSAGE=")); Serial.write(transport.published,transport.publishedLength); Serial.println();
  }
  if(previousError!=transport.error) {
    previousError=transport.error;
    Serial.print(F("RX_ERROR=")); Serial.println(previousError);
  }
}
#endif
