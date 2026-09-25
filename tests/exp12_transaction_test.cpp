#define EXP12_HOST_TEST
#include "../firmware/exp12/receiver/receiver.ino"
#include <assert.h>
#include <stdio.h>
int main() {
  CommandCache c;
  assert(c.accept(1,true)==0 && c.led && c.applied==1);
  // Lost response: retry twice, execution count must remain one.
  assert(c.accept(1,true)==1 && c.applied==1);
  assert(c.accept(1,true)==1 && c.applied==1);
  // Same transaction with altered content must not affect output/cache.
  assert(c.accept(1,false)==2 && c.led && c.applied==1);
  assert(c.accept(1,true)==1 && c.applied==1);
  assert(c.accept(2,false)==0 && !c.led && c.applied==2);
  assert(c.accept(2,false)==1 && c.applied==2);
  puts("PASS: lost replies, duplicate suppression, conflicting payload, next transaction");
}
