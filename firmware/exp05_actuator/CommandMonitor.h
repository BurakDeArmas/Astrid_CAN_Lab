#pragma once
#include <stdint.h>

// State codes are also carried in status byte 3 (ID 0x140).
class CommandMonitor {
public:
  enum State : uint8_t { WAITING, ACTIVE, COUNTER_ERROR, DATA_ERROR, TIMEOUT, RECOVERING };
  State state = WAITING;
  uint8_t angle = 90;

  void tick(uint32_t now) {
    if (static_cast<uint32_t>(now - lastFrameMs) >= 500) {
      synchronized = false;
      steps = 0;
      stop(TIMEOUT);
    }
  }

  // Called only for standard data frames with this experiment's command ID.
  // Arrival and valid counter progression are deliberately separate concepts:
  // repeated/invalid frames remain COUNTER_ERROR/DATA_ERROR, not TIMEOUT.
  void receive(uint32_t now, uint8_t dlc, const uint8_t *data) {
    tick(now);
    lastFrameMs = now;
    if (dlc != 4 || data[0] != 0xA5) {
      synchronized = false;
      steps = 0;
      stop(DATA_ERROR);
      return;
    }
    const uint16_t raw = static_cast<uint16_t>(data[2]) |
                         (static_cast<uint16_t>(data[3]) << 8);
    if (raw > 1023) {
      synchronized = false;
      steps = 0;
      stop(DATA_ERROR);
      return;
    }
    const uint8_t value = data[1];
    if (!synchronized) {
      synchronized = true;
      lastCounter = value;
      steps = 0;
      stop(RECOVERING);
      return;
    }
    const uint8_t expected = static_cast<uint8_t>(lastCounter + 1U);
    lastCounter = value;
    if (value != expected) {
      steps = 0;
      stop(COUNTER_ERROR);
      return;
    }
    if (steps < 3) steps++;
    if (steps < 3) { stop(RECOVERING); return; }
    state = ACTIVE;
    angle = 30U + (static_cast<uint32_t>(raw) * 120U / 1023U);
  }

private:
  bool synchronized = false;
  uint8_t lastCounter = 0, steps = 0;
  uint32_t lastFrameMs = 0;
  void stop(State reason) { state = reason; angle = 90; }
};
