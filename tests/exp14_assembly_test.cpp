#define EXP14_HOST_TEST
#include "../firmware/exp14/receiver/receiver.ino"
#include <assert.h>
#include <stdio.h>

Assembly::Result part(Assembly &a,uint32_t now,uint16_t id,uint8_t index,uint8_t value=42) {
  uint8_t d[8]={0xE4,static_cast<uint8_t>(id),static_cast<uint8_t>(id>>8),index,value,value,value,value};
  return a.feed(now,8,d);
}
int main() {
  Assembly a;
  assert(part(a,0,1,1)==Assembly::NONE && !a.active);
  for(uint8_t i=0;i<5;i++) assert(part(a,i*50,1,i)==Assembly::NONE && !a.hasPublished);
  assert(part(a,250,1,5)==Assembly::COMPLETE);
  assert(a.completed==1 && a.publishedTransfer==1);
  for(uint8_t i=0;i<24;i++) assert(a.published[i]==42);
  // Middle loss: reject index3 while expecting index2; preserve committed data.
  part(a,1000,2,0,99); part(a,1050,2,1,99);
  assert(part(a,1150,2,3,99)==Assembly::ORDER_ERROR);
  assert(part(a,1200,2,4,99)==Assembly::NONE);
  assert(a.completed==1 && a.publishedTransfer==1 && a.published[0]==42);
  // Final loss and exact timeout boundary.
  for(uint8_t i=0;i<5;i++) part(a,2000+i*50,3,i,99);
  assert(a.tick(2499)==Assembly::NONE);
  assert(a.tick(2500)==Assembly::TIMEOUT);
  assert(part(a,2501,3,5,99)==Assembly::NONE && a.completed==1);
  // Interleaved transfer and duplicate are errors, never mixed data.
  part(a,3000,4,0); assert(part(a,3010,5,1)==Assembly::ORDER_ERROR);
  part(a,3100,6,0); assert(part(a,3110,6,0)==Assembly::ORDER_ERROR);
  // Short frame can safely supply no bytes: DLC checked before dereference.
  part(a,3200,7,0); assert(a.feed(3210,0,nullptr)==Assembly::FORMAT_ERROR);
  part(a,3300,8,0); assert(part(a,3310,8,6)==Assembly::FORMAT_ERROR);
  // uint32 millis wrap and timeout inside feed.
  part(a,0xFFFFFFF0UL,9,0);
  assert(a.tick(0x0000011BUL)==Assembly::NONE);
  assert(a.tick(0x0000011CUL)==Assembly::TIMEOUT);
  part(a,4000,10,0); assert(part(a,4300,10,1)==Assembly::TIMEOUT);
  assert(a.completed==1 && a.published[23]==42);
  // A fresh, complete transfer after errors replaces the whole message.
  for(uint8_t i=0;i<5;i++) assert(part(a,5000+i*50,11,i,77)==Assembly::NONE);
  assert(part(a,5250,11,5,77)==Assembly::COMPLETE);
  assert(a.completed==2 && a.publishedTransfer==11);
  for(uint8_t i=0;i<24;i++) assert(a.published[i]==77);
  puts("PASS: assembly, missing parts, timeout boundary/wrap, duplicates, interleaving, malformed frames, recovery");
}
