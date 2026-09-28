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

uint16_t transfer=0;
uint8_t index=0;
bool busy=false;
uint32_t startedAt=0,lastPart=0;
char message[25]={};
void setup() {
  Serial.begin(115200); pinMode(2,INPUT_PULLUP); beginCAN(false);
  Serial.println(F("EXP14 sender: D2 starts 24-byte message in 6 parts"));
}
void loop() {
  checkOverflow();
  struct can_frame f;
  for(uint8_t i=0;i<8;i++) {
    if(canController.readMessage(&f)!=MCP2515::ERROR_OK) break;
    if(f.can_id!=0x361 || f.can_dlc!=4 || f.data[0]!=0xD4 || f.data[3]<1 || f.data[3]>4) continue;
    uint16_t id=static_cast<uint16_t>(f.data[1]) | (static_cast<uint16_t>(f.data[2])<<8);
    if(!busy || id!=transfer) { Serial.println(F("LATE_OR_UNMATCHED_STATUS")); continue; }
    busy=false;
    Serial.print(F("RESULT transfer=")); Serial.print(id);
    Serial.print(F(" code=")); Serial.println(f.data[3]);
  }
  if(busy && millis()-startedAt>=1500) {
    busy=false; Serial.println(F("NO_STATUS: outcome unknown"));
  }
  if(pressed()) {
    if(busy) Serial.println(F("BUSY"));
    else if(transfer==65535) Serial.println(F("ID exhausted: reset ALL nodes"));
    else {
      transfer++; index=0; busy=true; startedAt=millis(); lastPart=startedAt-50;
      snprintf(message,sizeof(message),"ASTRID CAN MESSAGE %05u",static_cast<unsigned>(transfer));
      Serial.print(F("START transfer=")); Serial.print(transfer);
      Serial.print(F(" text=")); Serial.println(message);
    }
  }
  if(!busy || index>=6 || millis()-lastPart<50) return;
  if(canController.getStatus() & 0x04) return;
  lastPart=millis();
  struct can_frame part={}; part.can_id=0x360; part.can_dlc=8;
  part.data[0]=0xE4; part.data[1]=transfer; part.data[2]=transfer>>8; part.data[3]=index;
  memcpy(part.data+4,message+4*index,4);
  MCP2515::ERROR result=canController.sendMessage(MCP2515::TXB0,&part);
  Serial.print(F("PART_SUBMITTED index=")); Serial.print(index);
  Serial.print(F(" api=")); Serial.println(static_cast<uint8_t>(result));
  index++; // API can report error after queuing. Do not enqueue this part twice.
}
