#include <assert.h>
#include <stdio.h>
#include "../firmware/exp05_actuator/CommandMonitor.h"

void send(CommandMonitor &m, uint32_t now, uint8_t counter, uint16_t adc = 1023) {
  const uint8_t data[] = {0xA5, counter, static_cast<uint8_t>(adc), static_cast<uint8_t>(adc >> 8)};
  m.receive(now, 4, data);
}

int main() {
  CommandMonitor m;
  assert(m.state == CommandMonitor::WAITING && m.angle == 90);
  send(m, 100, 254); send(m, 200, 255); send(m, 300, 0);
  assert(m.state == CommandMonitor::RECOVERING && m.angle == 90);
  send(m, 400, 1);
  assert(m.state == CommandMonitor::ACTIVE && m.angle == 150);

  // Frozen stream must remain a counter error, not turn into a timeout.
  for (uint32_t t = 500; t <= 1500; t += 100) {
    send(m, t, 1); m.tick(t + 50);
    assert(m.state == CommandMonitor::COUNTER_ERROR && m.angle == 90);
  }
  send(m, 1600, 2); send(m, 1700, 3);
  assert(m.state == CommandMonitor::RECOVERING);
  send(m, 1800, 4, 0);
  assert(m.state == CommandMonitor::ACTIVE && m.angle == 30);

  // Repeated bad data must remain a data error while frames arrive.
  for (uint32_t t = 1900; t <= 2900; t += 100) {
    send(m, t, 5, 2047); m.tick(t + 50);
    assert(m.state == CommandMonitor::DATA_ERROR && m.angle == 90);
  }
  send(m, 3000, 6); send(m, 3100, 7); send(m, 3200, 8);
  assert(m.state == CommandMonitor::RECOVERING);
  send(m, 3300, 9); assert(m.state == CommandMonitor::ACTIVE);
  m.tick(3799); assert(m.state == CommandMonitor::ACTIVE);
  m.tick(3800); assert(m.state == CommandMonitor::TIMEOUT && m.angle == 90);
  send(m, 3900, 80); send(m, 4000, 81); send(m, 4100, 82);
  assert(m.state == CommandMonitor::RECOVERING);
  send(m, 4200, 83); assert(m.state == CommandMonitor::ACTIVE);

  uint8_t bad[] = {0, 84, 0, 0};
  m.receive(4300, 4, bad); assert(m.state == CommandMonitor::DATA_ERROR);
  m.receive(4400, 0, nullptr); assert(m.state == CommandMonitor::DATA_ERROR);
  // Counter jump is not accepted as recovery.
  send(m, 4500, 0); send(m, 4600, 10);
  assert(m.state == CommandMonitor::COUNTER_ERROR);

  CommandMonitor wrap;
  send(wrap, 0xFFFFFF00UL, 10); send(wrap, 0xFFFFFF64UL, 11);
  send(wrap, 0xFFFFFFC8UL, 12); send(wrap, 0x0000002CUL, 13);
  assert(wrap.state == CommandMonitor::ACTIVE);
  wrap.tick(0x21FUL); assert(wrap.state == CommandMonitor::ACTIVE);
  wrap.tick(0x220UL); assert(wrap.state == CommandMonitor::TIMEOUT);
  CommandMonitor empty;
  empty.tick(500); assert(empty.state == CommandMonitor::TIMEOUT);
  puts("PASS: freeze, invalid data, recovery, timeout boundary, counter/millis wrap, malformed frames");
}
