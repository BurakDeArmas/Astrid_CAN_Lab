#define EXP16_HOST_TEST
#include "../firmware/exp16/ecu/ecu.ino"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main() {
  UdsDemo s;
  uint8_t r[7]={};
  const uint8_t read[]={0x22,0x12,0x34};
  assert(s.handle(read,3,r)==4);
  const uint8_t initial[]={0x62,0x12,0x34,25};
  assert(memcmp(r,initial,4)==0);
  uint8_t write[]={0x2E,0x12,0x34,60};
  assert(s.handle(write,4,r)==3 && s.setting==60 && s.applied==1);
  const uint8_t written[]={0x6E,0x12,0x34}; assert(memcmp(r,written,3)==0);
  write[3]=200;
  assert(s.handle(write,4,r)==3 && s.setting==60 && s.applied==1);
  const uint8_t range[]={0x7F,0x2E,0x31}; assert(memcmp(r,range,3)==0);
  s.handle(read,3,r); assert(r[3]==60);
  s.writesAllowed=false; write[3]=40;
  assert(s.handle(write,4,r)==3 && r[2]==0x22 && s.setting==60 && s.applied==1);
  s.handle(read,3,r); assert(r[0]==0x62 && r[3]==60); // Read remains permitted.
  s.writesAllowed=true;
  write[3]=0; s.handle(write,4,r); assert(s.setting==0 && r[0]==0x6E);
  write[3]=100; s.handle(write,4,r); assert(s.setting==100 && r[0]==0x6E);
  uint32_t count=s.applied;
  write[3]=101; s.handle(write,4,r); assert(r[2]==0x31 && s.setting==100);
  write[3]=255; s.handle(write,4,r); assert(r[2]==0x31 && s.setting==100);
  const uint8_t unknown[]={0x22,0x12,0x35}; s.handle(unknown,3,r);
  const uint8_t unknownReply[]={0x7F,0x22,0x31}; assert(memcmp(r,unknownReply,3)==0);
  write[2]=0x35; write[3]=20; s.handle(write,4,r); assert(r[2]==0x31 && s.setting==100);
  for(uint8_t n=1;n<4;n++) {
    s.handle(write,n,r); assert(r[0]==0x7F && r[1]==0x2E && r[2]==0x13);
  }
  uint8_t extra[]={0x2E,0x12,0x34,10,0}; s.handle(extra,5,r); assert(r[2]==0x13);
  const uint8_t unsupported[]={0x99}; s.handle(unsupported,1,r);
  const uint8_t badService[]={0x7F,0x99,0x11}; assert(memcmp(r,badService,3)==0);
  assert(s.handle(nullptr,0,r)==0);
  assert(s.setting==100 && s.applied==count);
  puts("PASS: read/write, exact UDS response bytes, boundaries, NRC 11/13/22/31, rejected writes preserve state");
}
