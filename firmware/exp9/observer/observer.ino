#include <SPI.h>
#include <mcp2515.h>

// 0=sender Uno, 1=receiver Uno, 2=listen-only Nano.
constexpr uint8_t ROLE = 2;
#define MCP_CLOCK MCP_8MHZ
constexpr uint32_t OSC_HZ = 8000000UL; // Assumed physical crystal, NOT measured.
constexpr CAN_SPEED BUS_SPEED = CAN_125KBPS; // Later: observer only -> CAN_250KBPS.
MCP2515 canController(10, 8000000);
uint32_t lastSendMs=0, lastReportMs=0, submitted=0, received=0, busy=0, flagged=0;

uint8_t readRegisterValue(uint8_t address) {
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  digitalWrite(10, LOW); SPI.transfer(0x03); SPI.transfer(address);
  const uint8_t result = SPI.transfer(0);
  digitalWrite(10, HIGH); SPI.endTransaction(); return result;
}

void reportTiming() {
  const uint8_t c1=readRegisterValue(0x2A), c2=readRegisterValue(0x29), c3=readRegisterValue(0x28);
  const uint8_t prescaler=(c1 & 0x3F)+1, sjw=(c1 >> 6)+1;
  const uint8_t prop=(c2 & 7)+1, ps1=((c2 >> 3)&7)+1;
  const uint8_t ps2=(c2 & 0x80) ? (c3 & 7)+1 : (ps1 > 2 ? ps1 : 2);
  const uint8_t total=1+prop+ps1+ps2;
  const float tqUs=2000000.0f*prescaler/OSC_HZ;
  Serial.print(F("CNF1=0x")); Serial.print(c1,HEX);
  Serial.print(F(" CNF2=0x")); Serial.print(c2,HEX);
  Serial.print(F(" CNF3=0x")); Serial.println(c3,HEX);
  Serial.print(F("ASSUMED_OSC_HZ=")); Serial.println(OSC_HZ);
  Serial.print(F("prescaler=")); Serial.print(prescaler); Serial.print(F(" SJW_TQ=")); Serial.println(sjw);
  Serial.print(F("Sync=1 Prop=")); Serial.print(prop); Serial.print(F(" PS1=")); Serial.print(ps1);
  Serial.print(F(" PS2=")); Serial.print(ps2); Serial.print(F(" Total_TQ=")); Serial.println(total);
  Serial.print(F("TQ_us=")); Serial.print(tqUs,3); Serial.print(F(" Bit_us=")); Serial.println(tqUs*total,3);
  Serial.print(F("Calculated_bps=")); Serial.println(OSC_HZ/(2UL*prescaler*total));
  Serial.print(F("Sample_percent=")); Serial.print(100.0f*(1+prop+ps1)/total,2);
  Serial.print(F(" Samples=")); Serial.println((c2 & 0x40) ? 3 : 1);
}

void setup() {
  Serial.begin(115200); pinMode(10,OUTPUT); digitalWrite(10,HIGH); SPI.begin();
  if (canController.reset()!=MCP2515::ERROR_OK ||
      canController.setBitrate(BUS_SPEED,MCP_CLOCK)!=MCP2515::ERROR_OK) {
    Serial.println(F("CAN INIT ERROR")); while(true) delay(1000);
  }
  reportTiming();
  const MCP2515::ERROR result = ROLE==2 ? canController.setListenOnlyMode() : canController.setNormalMode();
  if(result!=MCP2515::ERROR_OK) { Serial.println(F("CAN MODE ERROR")); while(true) delay(1000); }
  Serial.print(F("ROLE=")); Serial.println(ROLE);
  Serial.println(F("ms,submitted_total,received_window,busy_total,flagged_total,TEC,REC,EFLG"));
}

void loop() {
  struct can_frame frame;
  for(uint8_t i=0;i<8;i++) {
    if(canController.readMessage(&frame)!=MCP2515::ERROR_OK) break;
    if(frame.can_id==0x190 && frame.can_dlc==5 && frame.data[0]==0xA9) received++;
  }
  const uint32_t now=millis();
  if(ROLE==0 && now-lastSendMs>=100) {
    lastSendMs=now;
    if(canController.getStatus() & 0x04) busy++;
    else {
      struct can_frame tx={}; tx.can_id=0x190; tx.can_dlc=5; tx.data[0]=0xA9;
      for(uint8_t i=0;i<4;i++) tx.data[1+i]=submitted>>(8*i);
      if(canController.sendMessage(MCP2515::TXB0,&tx)!=MCP2515::ERROR_OK) flagged++;
      submitted++;
    }
  }
  if(now-lastReportMs>=1000) {
    lastReportMs=now; Serial.print(now); Serial.print(','); Serial.print(submitted);
    Serial.print(','); Serial.print(received); received=0;
    Serial.print(','); Serial.print(busy); Serial.print(','); Serial.print(flagged);
    Serial.print(','); Serial.print(canController.errorCountTX());
    Serial.print(','); Serial.print(canController.errorCountRX());
    Serial.print(','); Serial.println(canController.getErrorFlags(),HEX);
    canController.clearRXnOVRFlags();
  }
}
