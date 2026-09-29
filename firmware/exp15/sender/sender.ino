#include <stdint.h>
#include <string.h>
// Teaching subset: Classical CAN, normal addressing, padded DLC8, <=128 bytes.
class IsoSender {
public:
  enum State : uint8_t { IDLE, FIRST, TX_PENDING, WAIT_FC, CONSECUTIVE, DONE, FAILED };
  enum Error : uint8_t { OK, FC_TIMEOUT, TX_TIMEOUT, BAD_FC, UNSUPPORTED_FC };
  State state=IDLE;
  Error error=OK;
  uint8_t blockSize=0, stmin=0;
  bool start(const uint8_t *p,uint16_t n) {
    if((state!=IDLE && state!=DONE) || n==0 || n>128) return false;
    memcpy(payload,p,n); length=n; offset=0; sequence=1; used=0; error=OK;
    state=FIRST; return true;
  }
  bool frame(uint32_t now,uint8_t *d) {
    if(state!=FIRST && state!=CONSECUTIVE) return false;
    // +1ms avoids rounding down below the advertised minimum at millis resolution.
    if(state==CONSECUTIVE && static_cast<uint32_t>(now-lastEnd)<static_cast<uint32_t>(stmin)+1) return false;
    memset(d,0,8);
    if(state==FIRST) {
      if(length<=7) { kind=0; amount=length; d[0]=length; memcpy(d+1,payload,amount); }
      else { kind=1; amount=6; d[0]=0x10 | (length>>8); d[1]=length; memcpy(d+2,payload,6); }
    } else {
      kind=2; amount=(length-offset>7) ? 7 : length-offset;
      d[0]=0x20 | sequence; memcpy(d+1,payload+offset,amount);
    }
    state=TX_PENDING; changed=now; return true;
  }
  void transmitted(uint32_t now) {
    if(state!=TX_PENDING) return;
    offset+=amount; lastEnd=now; changed=now;
    if(offset==length) { state=DONE; return; }
    if(kind==1) { state=WAIT_FC; return; }
    sequence=(sequence+1)&15; used++;
    state=(blockSize!=0 && used>=blockSize) ? WAIT_FC : CONSECUTIVE;
  }
  void flowControl(uint32_t now,uint8_t dlc,const uint8_t *d) {
    tick(now);
    if(state!=WAIT_FC) return;
    if(dlc<3 || dlc>8 || (d[0]>>4)!=3) { fail(BAD_FC); return; }
    // WAIT/overflow and sub-millisecond STmin are deliberately not implemented.
    if((d[0]&15)!=0 || d[2]>127) { fail(UNSUPPORTED_FC); return; }
    blockSize=d[1]; stmin=d[2]; used=0; changed=now; state=CONSECUTIVE;
  }
  void tick(uint32_t now) {
    if(static_cast<uint32_t>(now-changed)<1000) return;
    if(state==WAIT_FC) fail(FC_TIMEOUT);
    else if(state==TX_PENDING) fail(TX_TIMEOUT);
  }
private:
  uint8_t payload[128]={},sequence=1,used=0,kind=0,amount=0;
  uint16_t length=0,offset=0;
  uint32_t changed=0,lastEnd=0;
  void fail(Error e) { error=e; state=FAILED; }
};
#ifndef EXP15_HOST_TEST
#include <SPI.h>
#include <mcp2515.h>
MCP2515 canController(10,8000000);
IsoSender transport;
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

uint16_t number=0;
bool hardwarePending=false;
void pollTx() {
  if(hardwarePending && (canController.getInterrupts() & 0x04)) {
    canController.clearTXInterrupts(); hardwarePending=false;
    transport.transmitted(millis());
  }
}
void setup() {
  Serial.begin(115200); pinMode(2,INPUT_PULLUP); beginCAN(false);
  Serial.println(F("EXP15 sender D2: 24-byte ISO-TP transfer. Errors require reset ALL nodes."));
}
void loop() {
  pollTx(); transport.tick(millis()); checkOverflow();
  struct can_frame f;
  for(uint8_t i=0;i<8;i++) {
    if(canController.readMessage(&f)!=MCP2515::ERROR_OK) break;
    pollTx(); // Completion may occur between the first poll and receiving FC.
    if(f.can_id!=0x381) continue;
    transport.flowControl(millis(),f.can_dlc,f.data);
    Serial.print(F("FC_RECEIVED state=")); Serial.print(static_cast<uint8_t>(transport.state));
    Serial.print(F(" BS=")); Serial.print(transport.blockSize);
    Serial.print(F(" STmin_ms=")); Serial.println(transport.stmin);
  }
  if(pressed()) {
    char message[25];
    snprintf(message,sizeof(message),"ASTRID ISO MESSAGE %05u",static_cast<unsigned>(number+1));
    if(transport.start(reinterpret_cast<const uint8_t*>(message),24)) {
      number++; Serial.print(F("START ")); Serial.println(message);
    } else Serial.println(F("BLOCKED: busy or failed"));
  }
  if(!hardwarePending && !(canController.getStatus() & 0x04)) {
    struct can_frame out={}; out.can_id=0x380; out.can_dlc=8;
    if(transport.frame(millis(),out.data)) {
      canController.clearTXInterrupts(); hardwarePending=true;
      MCP2515::ERROR e=canController.sendMessage(MCP2515::TXB0,&out);
      Serial.print(F("SUBMITTED PCI=0x")); Serial.print(out.data[0],HEX);
      Serial.print(F(" api=")); Serial.println(static_cast<uint8_t>(e));
    }
  }
  static uint8_t previous=255;
  if(previous!=transport.state) {
    previous=transport.state;
    if(transport.state==IsoSender::DONE) Serial.println(F("TX_DONE: all CAN frames sent; no application confirmation"));
    if(transport.state==IsoSender::FAILED) {
      Serial.print(F("FAILED error=")); Serial.println(static_cast<uint8_t>(transport.error));
    }
  }
}
#endif
