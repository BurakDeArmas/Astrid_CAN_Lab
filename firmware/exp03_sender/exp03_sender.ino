#include <SPI.h>
#include <mcp2515.h>

// Uno A: pot A0, MCP2515 CS D10. Kristali fiziksel olarak kontrol edin.
#define MCP_CLOCK MCP_8MHZ
constexpr uint16_t COMMAND_ID = 0x110;
constexpr uint32_t SEND_PERIOD_MS = 100;
MCP2515 canController(10, 8000000);
uint32_t lastSendMs = 0;
uint8_t counter = 0;

void setup() {
  Serial.begin(115200);
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  SPI.begin();
  if (canController.reset() != MCP2515::ERROR_OK ||
      canController.setBitrate(CAN_125KBPS, MCP_CLOCK) != MCP2515::ERROR_OK ||
      canController.setNormalMode() != MCP2515::ERROR_OK) {
    Serial.println(F("CAN INIT ERROR: baglanti ve kristali kontrol et"));
    while (true) { delay(1000); }
  }
  Serial.println(F("EXP03 UNO A | POT A0 | 125kbps"));
}

void loop() {
  const uint32_t now = millis();
  if (now - lastSendMs < SEND_PERIOD_MS) return;
  lastSendMs = now;
  const uint16_t raw = analogRead(A0);
  struct can_frame frame = {};
  frame.can_id = COMMAND_ID;
  frame.can_dlc = 4;
  frame.data[0] = 0xA3;
  frame.data[1] = counter;
  frame.data[2] = raw & 0xFF;
  frame.data[3] = raw >> 8;
  const MCP2515::ERROR result = canController.sendMessage(&frame);
  Serial.print(F("TX queue | ADC=")); Serial.print(raw);
  Serial.print(F(" | counter=")); Serial.print(counter);
  Serial.print(F(" | result=")); Serial.println(static_cast<int>(result));
  // ERROR_OK means queued, not that the receiving application accepted it.
  if (result == MCP2515::ERROR_OK) counter++;
}
