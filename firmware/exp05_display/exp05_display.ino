#include <SPI.h>
#include <mcp2515.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

// Classic 5V Nano: MCP CS D10; LCD SDA A4, SCL A5.
#define MCP_CLOCK MCP_8MHZ
MCP2515 canController(10, 8000000);
hd44780_I2Cexp lcd; // Automatically detects supported backpack address/mapping.
bool lcdReady = false;
uint8_t targetAngle = 90, appliedAngle = 90;
uint8_t actuatorState = 0;

struct LinkState {
  bool seen;
  uint8_t counter;
  uint8_t steps;
  uint32_t progressMs;
};
LinkState commandLink = {}, statusLink = {};

// Require three +1 steps; repeats/jumps clear confidence immediately.
void track(LinkState &link, uint8_t value) {
  const uint32_t now = millis();
  if (!link.seen || now - link.progressMs >= 500) {
    link.seen = true;
    link.counter = value;
    link.steps = 0;
    link.progressMs = now;
    return;
  }
  if (value == static_cast<uint8_t>(link.counter + 1U)) {
    link.progressMs = now;
    if (link.steps < 3) link.steps++;
  } else {
    link.steps = 0;
  }
  link.counter = value;
}

bool healthy(const LinkState &link) {
  return link.seen && link.steps >= 3 && millis() - link.progressMs < 500;
}

void receiveFrame(const struct can_frame &frame) {
  if (frame.can_id == 0x130) {
    if (frame.can_dlc != 4 || frame.data[0] != 0xA5) {
      commandLink.steps = 0;
      return;
    }
    const uint16_t raw = static_cast<uint16_t>(frame.data[2]) |
                        (static_cast<uint16_t>(frame.data[3]) << 8);
    if (raw > 1023) { commandLink.steps = 0; return; }
    track(commandLink, frame.data[1]);
    targetAngle = 30U + (static_cast<uint32_t>(raw) * 120U / 1023U);
  } else if (frame.can_id == 0x140) {
    if (frame.can_dlc != 4 || frame.data[0] != 0xB5 ||
        frame.data[2] < 30 || frame.data[2] > 150 || frame.data[3] > 5 ||
        (frame.data[3] != 1 && frame.data[2] != 90)) {
      statusLink.steps = 0;
      return;
    }
    track(statusLink, frame.data[1]);
    appliedAngle = frame.data[2];
    actuatorState = frame.data[3];
  }
}

void refreshDisplay() {
  static uint32_t lastRefreshMs = 0;
  if (millis() - lastRefreshMs < 200) return;
  lastRefreshMs = millis();
  char row0[17], row1[17];
  if (healthy(commandLink))
    snprintf(row0, sizeof(row0), "HEDEF:%3u       ", static_cast<unsigned>(targetAngle));
  else snprintf(row0, sizeof(row0), "KOMUT YOK/HATA  ");
  if (!healthy(statusLink))
    snprintf(row1, sizeof(row1), "AKTUATOR YOK    ");
  else {
    switch (actuatorState) {
      case 0: snprintf(row1, sizeof(row1), "BEKLIYOR        "); break;
      case 1: snprintf(row1, sizeof(row1), "UYG:%3u AKTIF   ", static_cast<unsigned>(appliedAngle)); break;
      case 2: snprintf(row1, sizeof(row1), "SAYAC HATASI    "); break;
      case 3: snprintf(row1, sizeof(row1), "VERI HATASI     "); break;
      case 4: snprintf(row1, sizeof(row1), "TIMEOUT         "); break;
      default: snprintf(row1, sizeof(row1), "TOPARLANIYOR    "); break;
    }
  }
  if (lcdReady) {
    lcd.setCursor(0, 0); lcd.print(row0);
    lcd.setCursor(0, 1); lcd.print(row1);
  }
  static char previous[17] = "";
  if (strcmp(previous, row1) != 0) {
    Serial.print(millis()); Serial.print(F(",DISPLAY,")); Serial.println(row1);
    strcpy(previous, row1);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  lcdReady = lcd.begin(16, 2) == 0;
  if (!lcdReady) Serial.println(F("LCD INIT ERROR: I2CexpDiag or wiring"));
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  SPI.begin();
  if (canController.reset() != MCP2515::ERROR_OK ||
      canController.setBitrate(CAN_125KBPS, MCP_CLOCK) != MCP2515::ERROR_OK ||
      canController.setNormalMode() != MCP2515::ERROR_OK) {
    Serial.println(F("CAN INIT ERROR"));
    if (lcdReady) { lcd.clear(); lcd.print("CAN INIT ERROR"); }
    while (true) { delay(1000); }
  }
  Serial.println(F("EXP05 NANO DISPLAY"));
}

void loop() {
  struct can_frame frame;
  for (uint8_t i = 0; i < 4; i++) {
    if (canController.readMessage(&frame) != MCP2515::ERROR_OK) break;
    receiveFrame(frame);
  }
  refreshDisplay();
}
