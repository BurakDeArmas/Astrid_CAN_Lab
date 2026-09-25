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

uint32_t transaction=0, lastAttempt=0;
uint8_t attempts=0;
bool waiting=false, uncertain=false, desired=false;
void attempt() {
  lastAttempt=millis(); attempts++;
  Serial.print(F("ATTEMPT tx=")); Serial.print(transaction);
  Serial.print(F(" n=")); Serial.println(attempts);
  if (canController.getStatus() & 0x04) { Serial.println(F("TX_BUSY")); return; }
  struct can_frame f={}; f.can_id=0x320; f.can_dlc=8;
  f.data[0]=0xC1; f.data[1]=1;
  for(uint8_t i=0;i<4;i++) f.data[2+i]=transaction>>(8*i);
  f.data[6]=desired;
  if(canController.sendMessage(MCP2515::TXB0,&f)!=MCP2515::ERROR_OK)
    Serial.println(F("TX_API_ERROR (delivery unknown)"));
}
void setup() {
  Serial.begin(115200); pinMode(2,INPUT_PULLUP); beginCAN(false);
  Serial.println(F("EXP12 sender: press D2 to SET LED. Reply timeout 200ms, 3 attempts."));
}
void loop() {
  struct can_frame f;
  for(uint8_t i=0;i<8;i++) {
    if(canController.readMessage(&f)!=MCP2515::ERROR_OK) break;
    if(f.can_id!=0x321 || f.can_dlc!=8 || f.data[0]!=0xD1 || f.data[1]!=1 ||
       f.data[6]>1 || f.data[7]>2) continue;
    if(!waiting || readTransaction(f.data)!=transaction) {
      Serial.println(F("IGNORED late/unmatched reply")); continue;
    }
    if(f.data[7]==2 || f.data[6]!=desired) {
      waiting=false; uncertain=true; Serial.println(F("CONFLICT: reset all nodes"));
    } else {
      waiting=false;
      Serial.print(F("CONFIRMED tx=")); Serial.print(transaction);
      Serial.print(F(" LED=")); Serial.print(f.data[6]);
      Serial.print(F(" result=")); Serial.println(f.data[7]==0 ? F("APPLIED") : F("DUPLICATE"));
    }
  }
  if(waiting && millis()-lastAttempt>=200) {
    if(attempts<3) attempt();
    else { waiting=false; uncertain=true; Serial.println(F("UNKNOWN: no reply; LED may have changed. Reset all nodes.")); }
  }
  if(pressed()) {
    if(waiting || uncertain) { Serial.println(F("BLOCKED: pending or unknown result")); return; }
    if(transaction==0xFFFFFFFFUL) { uncertain=true; Serial.println(F("ID exhausted: reset all nodes")); return; }
    transaction++; desired=!desired; attempts=0; waiting=true; attempt();
  }
}
