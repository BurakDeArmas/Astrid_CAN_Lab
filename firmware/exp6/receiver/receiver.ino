#include <stdint.h>

// Single-file sketch; host tests compile this exact tracker without Arduino code.
class SequenceTracker {
public:
  uint32_t agedMissing = 0, repeats = 0, late = 0, oldOrReset = 0;
  void observe(uint32_t value) {
    if (!seen) { seen = true; highest = value; bitmap = 0xFFFFFFFFUL; return; }
    const uint32_t forward = value - highest;
    if (forward == 0) { repeats++; return; }
    if (forward < 0x80000000UL) {
      if (forward >= 32) {
        agedMissing += missingBits(bitmap) + forward - 32;
        bitmap = 1;
      } else {
        for (uint8_t i = 0; i < forward; i++) {
          if (!(bitmap & 0x80000000UL)) agedMissing++;
          bitmap <<= 1;
        }
        bitmap |= 1;
      }
      highest = value;
      return;
    }
    const uint32_t behind = highest - value;
    if (behind >= 32) { oldOrReset++; return; }
    const uint32_t bit = static_cast<uint32_t>(1) << behind;
    if (bitmap & bit) repeats++;
    else { bitmap |= bit; late++; }
  }
  uint8_t pending() const { return seen ? missingBits(bitmap) : 0; }
  void clearReport() { agedMissing = repeats = late = oldOrReset = 0; }
private:
  bool seen = false;
  uint32_t highest = 0, bitmap = 0;
  static uint8_t missingBits(uint32_t value) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < 32; i++) { if (!(value & 1)) count++; value >>= 1; }
    return count;
  }
};

#ifndef EXP06_TRACKER_TEST
#include <SPI.h>
#include <mcp2515.h>

#define MCP_CLOCK MCP_8MHZ
MCP2515 canController(10, 8000000);
uint32_t received[2] = {};
SequenceTracker tracks[2];
uint32_t overflowSamples = 0, invalid = 0, other = 0;
uint32_t lastReportMs = 0;

void processFrame(const struct can_frame &frame) {
  uint8_t node;
  if (frame.can_id == 0x100) node = 0;
  else if (frame.can_id == 0x500) node = 1;
  else { other++; return; }
  if (frame.can_dlc != 8 || frame.data[0] != 0xA6 ||
      frame.data[1] != node + 1 || frame.data[6] > 3 || frame.data[7] != 0x5A) {
    invalid++; return;
  }
  uint32_t sequence = 0;
  for (uint8_t i = 0; i < 4; i++) sequence |= static_cast<uint32_t>(frame.data[2+i]) << (8*i);
  received[node]++;
  tracks[node].observe(sequence);
}

void setup() {
  Serial.begin(115200);
  pinMode(2, INPUT_PULLUP); // Hold D2-GND to deliberately stall receiver software.
  pinMode(10, OUTPUT); digitalWrite(10, HIGH); SPI.begin();
  if (canController.reset() != MCP2515::ERROR_OK ||
      canController.setBitrate(CAN_125KBPS, MCP_CLOCK) != MCP2515::ERROR_OK ||
      canController.setNormalMode() != MCP2515::ERROR_OK) {
    Serial.println(F("CAN INIT ERROR"));
    while (true) delay(1000);
  }
  Serial.println(F("EXP06: window_ms,A_rx,B_rx,A_aged_missing,B_aged_missing,A_repeat,B_repeat,A_late,B_late,A_old_or_reset,B_old_or_reset,ovr_samples,invalid,other,EFLG,REC,A_pending,B_pending"));
  lastReportMs = millis();
}

void loop() {
  if (digitalRead(2) == LOW) delay(20); // Deliberate bottleneck, not production behavior.
  const uint8_t flags = canController.getErrorFlags();
  if (flags & 0xC0) { overflowSamples++; canController.clearRXnOVRFlags(); }
  struct can_frame frame;
  for (uint8_t i = 0; i < 16; i++) {
    if (canController.readMessage(&frame) != MCP2515::ERROR_OK) break;
    processFrame(frame);
  }
  const uint32_t now = millis();
  if (now - lastReportMs < 1000) return;
  Serial.print(now - lastReportMs); lastReportMs = now;
  for (uint8_t i = 0; i < 2; i++) { Serial.print(','); Serial.print(received[i]); received[i] = 0; }
  for (uint8_t i = 0; i < 2; i++) { Serial.print(','); Serial.print(tracks[i].agedMissing); }
  for (uint8_t i = 0; i < 2; i++) { Serial.print(','); Serial.print(tracks[i].repeats); }
  for (uint8_t i = 0; i < 2; i++) { Serial.print(','); Serial.print(tracks[i].late); }
  for (uint8_t i = 0; i < 2; i++) { Serial.print(','); Serial.print(tracks[i].oldOrReset); tracks[i].clearReport(); }
  Serial.print(','); Serial.print(overflowSamples); overflowSamples = 0;
  Serial.print(','); Serial.print(invalid); invalid = 0;
  Serial.print(','); Serial.print(other); other = 0;
  Serial.print(','); Serial.print(canController.getErrorFlags(), HEX);
  Serial.print(','); Serial.print(canController.errorCountRX());
  Serial.print(','); Serial.print(tracks[0].pending());
  Serial.print(','); Serial.println(tracks[1].pending());
}

#endif // EXP06_TRACKER_TEST
