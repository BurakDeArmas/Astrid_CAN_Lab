#define EXP06_TRACKER_TEST
#include "../firmware/exp6/receiver/receiver.ino"
#include <assert.h>
#include <stdio.h>

int main() {
  SequenceTracker reordered;
  reordered.observe(100); reordered.observe(102); reordered.observe(101);
  reordered.observe(103);
  assert(reordered.agedMissing == 0 && reordered.pending() == 0 && reordered.late == 1);
  reordered.observe(101); assert(reordered.repeats == 1);
  reordered.clearReport();
  reordered.observe(104); assert(reordered.pending() == 0 && reordered.repeats == 0);

  SequenceTracker gap;
  gap.observe(0); gap.observe(2);
  assert(gap.pending() == 1 && gap.agedMissing == 0);
  for (unsigned i = 3; i <= 32; i++) gap.observe(i);
  assert(gap.pending() == 1 && gap.agedMissing == 0);
  gap.observe(33); assert(gap.pending() == 0 && gap.agedMissing == 1);
  gap.observe(1); assert(gap.oldOrReset == 1 && gap.agedMissing == 1);
  gap.observe(34); assert(gap.agedMissing == 1);

  SequenceTracker jump;
  jump.observe(100); jump.observe(200);
  assert(jump.agedMissing == 68 && jump.pending() == 31);
  for (unsigned i = 201; i <= 231; i++) jump.observe(i);
  assert(jump.agedMissing == 99 && jump.pending() == 0);
  jump.observe(0); assert(jump.oldOrReset == 1);
  jump.observe(232); assert(jump.agedMissing == 99);

  SequenceTracker wrap;
  wrap.observe(0xFFFFFFFEUL); wrap.observe(0); wrap.observe(0xFFFFFFFFUL); wrap.observe(1);
  assert(wrap.pending() == 0 && wrap.agedMissing == 0 && wrap.late == 1);
  puts("PASS: reordered delivery, repeats, report reset, aging boundary, large gap, old/reset, uint32 wrap");
}
