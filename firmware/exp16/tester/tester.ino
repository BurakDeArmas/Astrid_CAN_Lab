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

uint8_t nextCase=0,requestSid=0;
uint16_t requestDid=0;
bool waiting=false,failed=false,hardwarePending=false;
uint32_t started=0;
const char *nrcName(uint8_t code) {
  switch(code) {
    case 0x11:return "ServiceNotSupported";
    case 0x13:return "IncorrectMessageLengthOrInvalidFormat";
    case 0x22:return "ConditionsNotCorrect";
    case 0x31:return "RequestOutOfRange";
    default:return "UnimplementedNRC";
  }
}
void pollTx() {
  if(hardwarePending && (canController.getInterrupts() & 0x04)) {
    canController.clearTXInterrupts(); hardwarePending=false;
    Serial.println(F("CAN_TX_DONE (not UDS acceptance)"));
  }
}
void sendCase() {
  if(canController.getStatus() & 0x04) { Serial.println(F("TX_BUSY")); return; }
  struct can_frame f={}; f.can_id=0x700; f.can_dlc=8;
  f.data[0]=3; f.data[1]=0x22; f.data[2]=0x12; f.data[3]=0x34;
  if(nextCase==1 || nextCase==3) {
    f.data[0]=4; f.data[1]=0x2E; f.data[4]=(nextCase==1) ? 60 : 200;
  }
  if(nextCase==5) f.data[3]=0x35;
  if(nextCase==6) f.data[1]=0x2E; // Valid ISO-TP SF, but missing UDS value byte.
  if(nextCase==7) { f.data[0]=1; f.data[1]=0x99; }
  requestSid=f.data[1]; requestDid=(static_cast<uint16_t>(f.data[2])<<8)|f.data[3];
  started=millis(); waiting=true; hardwarePending=true;
  canController.clearTXInterrupts();
  MCP2515::ERROR e=canController.sendMessage(MCP2515::TXB0,&f);
  Serial.print(F("CASE=")); Serial.print(nextCase+1);
  Serial.print(F(" SID=0x")); Serial.print(requestSid,HEX);
  Serial.print(F(" api=")); Serial.println(static_cast<uint8_t>(e));
}
void setup() {
  Serial.begin(115200); pinMode(2,INPUT_PULLUP); beginCAN(false);
  Serial.println(F("EXP16 tester: D2 runs next case (1..8). Next=read setting."));
}
void loop() {
  pollTx(); checkOverflow();
  if(waiting && millis()-started>=500) {
    waiting=false; failed=true;
    Serial.println(F("TIMEOUT: outcome unknown; reset ALL nodes before retry"));
  }
  struct can_frame f;
  for(uint8_t i=0;i<8;i++) {
    if(canController.readMessage(&f)!=MCP2515::ERROR_OK) break;
    pollTx();
    if(f.can_id!=0x708 || !validSingleFrame(f)) continue;
    if(!waiting) { Serial.println(F("IGNORED: late/unrequested response")); continue; }
    const uint8_t *r=f.data+1; uint8_t n=f.data[0];
    bool matched=false;
    if(n==3 && r[0]==0x7F && r[1]==requestSid &&
       (r[2]==0x11 || r[2]==0x13 || r[2]==0x22 || r[2]==0x31)) {
      Serial.print(F("UDS_NEGATIVE NRC=0x")); Serial.print(r[2],HEX);
      Serial.print(' '); Serial.println(nrcName(r[2])); matched=true;
    } else if((requestSid==0x22 && n==4 && r[0]==0x62 && r[3]<=100) ||
              (requestSid==0x2E && n==3 && r[0]==0x6E)) {
      uint16_t did=(static_cast<uint16_t>(r[1])<<8)|r[2];
      if(did==requestDid) {
        Serial.print(F("UDS_POSITIVE DID=0x")); Serial.print(did,HEX);
        if(requestSid==0x22) { Serial.print(F(" value=")); Serial.print(r[3]); }
        Serial.println(); matched=true;
      }
    }
    if(matched) { waiting=false; nextCase=(nextCase+1)%8; }
    else Serial.println(F("IGNORED: response does not match pending request"));
  }
  if(pressed()) {
    if(waiting || failed || hardwarePending) Serial.println(F("BLOCKED: pending or failed session"));
    else sendCase();
  }
}
