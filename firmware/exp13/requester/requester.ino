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

bool waiting=false;
uint32_t requestedAt=0;
void setup() {
  Serial.begin(115200); pinMode(2,INPUT_PULLUP); beginCAN(false);
  Serial.println(F("EXP13 requester: D2 sends STD RTR 0x350, DLC=4, no payload"));
}
void loop() {
  checkOverflow();
  // Deadline first: a frame read after this point is not considered in-window.
  if(waiting && millis()-requestedAt>=500) {
    waiting=false; Serial.println(F("NO_DATA_IN_WINDOW (500ms)"));
  }
  struct can_frame f;
  for(uint8_t i=0;i<8;i++) {
    if(canController.readMessage(&f)!=MCP2515::ERROR_OK) break;
    // Exact ID excludes extended and RTR frames before touching payload.
    if(f.can_id!=VALUE_ID || f.can_dlc!=VALUE_DLC) continue;
    uint16_t raw=static_cast<uint16_t>(f.data[0]) | (static_cast<uint16_t>(f.data[1])<<8);
    uint16_t sample=static_cast<uint16_t>(f.data[2]) | (static_cast<uint16_t>(f.data[3])<<8);
    if(raw>1023) { Serial.println(F("INVALID ADC")); continue; }
    Serial.print(waiting ? F("DATA_DURING_WAIT") : F("DATA_WITHOUT_WAIT"));
    Serial.print(F(" ADC=")); Serial.print(raw);
    Serial.print(F(" sample=")); Serial.println(sample);
    waiting=false;
  }
  if(pressed()) {
    if(waiting) { Serial.println(F("WAITING: press again after timeout/data")); return; }
    if(canController.getStatus() & 0x04) { Serial.println(F("TX_BUSY")); return; }
    struct can_frame request={};
    request.can_id=VALUE_ID | CAN_RTR_FLAG; request.can_dlc=VALUE_DLC;
    // DLC requests four bytes in the DATA frame; RTR itself carries no data.
    requestedAt=millis(); waiting=true;
    MCP2515::ERROR result=canController.sendMessage(MCP2515::TXB0,&request);
    Serial.println(result==MCP2515::ERROR_OK ? F("RTR_SUBMITTED") : F("RTR_API_ERROR: delivery unknown"));
  }
}
