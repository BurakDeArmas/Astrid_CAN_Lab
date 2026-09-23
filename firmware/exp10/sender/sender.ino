#include <SPI.h>
#include <mcp2515.h>
#define MCP_CLOCK MCP_8MHZ
MCP2515 canController(10, 8000000);

// EXP10 protocol: version, counter, signed temp*10 LE, unsigned mV LE, flags, reserved.
uint16_t readLE(const uint8_t *p) { return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8); }
void writeLE(uint8_t *p, uint16_t value) { p[0]=value & 255; p[1]=value >> 8; }
int32_t signed16(uint16_t raw) { return raw < 0x8000 ? static_cast<int32_t>(raw) : static_cast<int32_t>(raw)-65536L; }
void initCan(bool silent) {
  Serial.begin(115200); pinMode(10,OUTPUT); digitalWrite(10,HIGH); SPI.begin();
  if(canController.reset()!=MCP2515::ERROR_OK || canController.setBitrate(CAN_125KBPS,MCP_CLOCK)!=MCP2515::ERROR_OK ||
     (silent ? canController.setListenOnlyMode() : canController.setNormalMode())!=MCP2515::ERROR_OK) {
    Serial.println(F("CAN INIT ERROR")); while(true) delay(1000);
  }
}

const int16_t temperatures[] = {-123, 0, 253, 850}; // Synthetic, NOT sensor measurements.
uint8_t sample=0, counter=0;
uint32_t lastSend=0;
void setup() { pinMode(2,INPUT_PULLUP); initCan(false); Serial.println(F("EXP10 sender: synthetic temperature, A0 voltage estimate")); }
void loop() {
  static bool candidate=HIGH, stable=HIGH; static uint32_t edge=0;
  const bool raw=digitalRead(2);
  if(raw!=candidate) { candidate=raw; edge=millis(); }
  if(millis()-edge>=30 && stable!=candidate) { stable=candidate; if(stable==LOW) sample=(sample+1)%4; }
  struct can_frame discard;
  for(uint8_t i=0;i<4;i++) if(canController.readMessage(&discard)!=MCP2515::ERROR_OK) break;
  const uint32_t now=millis(); if(now-lastSend<100) return; lastSend=now;
  if(canController.getStatus() & 0x04) { Serial.println(F("TX BUSY")); return; }
  struct can_frame frame={}; frame.can_id=0x300; frame.can_dlc=8;
  frame.data[0]=1; frame.data[1]=counter++;
  writeLE(&frame.data[2],static_cast<uint16_t>(temperatures[sample]));
  const uint16_t mv=static_cast<uint32_t>(analogRead(A0))*5000UL/1023;
  writeLE(&frame.data[4],mv); frame.data[6]=1; frame.data[7]=0;
  if(canController.sendMessage(MCP2515::TXB0,&frame)!=MCP2515::ERROR_OK) Serial.println(F("TX FLAGGED"));
}
