#define EXP15_HOST_TEST
#include "../firmware/exp15/sender/sender.ino"
#include "../firmware/exp15/receiver/receiver.ino"
#include <assert.h>
#include <stdio.h>

void exchange(unsigned length,uint8_t bs,uint8_t st,unsigned expectedFC) {
  IsoSender tx; IsoReceiver rx; rx.blockSize=bs; rx.stmin=st;
  uint8_t payload[128]; for(unsigned i=0;i<length;i++) payload[i]=static_cast<uint8_t>(i+1);
  assert(tx.start(payload,length));
  uint32_t now=0; unsigned fcCount=0,cfCount=0;
  while(tx.state!=IsoSender::DONE && now<10000) {
    tx.tick(now); rx.tick(now);
    if(rx.fcNeeded) {
      uint8_t fc[8]={0x30,bs,st};
      rx.fcTransmitted(now); tx.flowControl(now,8,fc); fcCount++;
    }
    uint8_t d[8];
    if(tx.frame(now,d)) {
      if((d[0]>>4)==1) { assert(d[0]==0x10 && d[1]==length); assert(d[2]==1 && d[7]==6); }
      if((d[0]>>4)==2) { cfCount++; assert((d[0]&15)==(cfCount&15)); }
      // Model completion before delivering the frame to the receiving application.
      tx.transmitted(now); rx.feed(now,8,d);
    }
    assert(tx.state!=IsoSender::FAILED && rx.error==IsoReceiver::OK);
    now++;
  }
  assert(tx.state==IsoSender::DONE && rx.completed==1 && rx.publishedLength==length);
  assert(memcmp(rx.published,payload,length)==0 && fcCount==expectedFC);
}
int main() {
  exchange(1,2,50,0); exchange(7,2,50,0);
  exchange(8,2,50,1); exchange(24,2,50,2); exchange(24,1,100,3);
  exchange(128,0,0,1); // 18 CFs: sequence wraps through zero.
  IsoSender tx; uint8_t msg[24]={},d[8];
  assert(tx.start(msg,24)); assert(tx.frame(0,d)); tx.transmitted(1);
  uint8_t fc[8]={0x30,2,50}; tx.flowControl(2,8,fc);
  assert(!tx.frame(51,d)); assert(tx.frame(52,d));
  tx.transmitted(53); assert(!tx.frame(103,d)); assert(tx.frame(104,d));
  tx.transmitted(105); assert(tx.state==IsoSender::WAIT_FC);
  tx.tick(1104); assert(tx.state==IsoSender::WAIT_FC);
  tx.tick(1105); assert(tx.error==IsoSender::FC_TIMEOUT);
  assert(!tx.start(msg,24)); // Failed sessions require reset.

  IsoSender hardware; hardware.start(msg,24); hardware.frame(0xFFFFFFF0UL,d);
  hardware.tick(0x000003D7UL); assert(hardware.state==IsoSender::TX_PENDING);
  hardware.tick(0x000003D8UL); assert(hardware.error==IsoSender::TX_TIMEOUT);
  for(uint8_t flow=1;flow<=2;flow++) {
    IsoSender limited; limited.start(msg,24); limited.frame(0,d); limited.transmitted(1);
    uint8_t unsupported[8]={static_cast<uint8_t>(0x30|flow),0,0};
    limited.flowControl(2,8,unsupported); assert(limited.error==IsoSender::UNSUPPORTED_FC);
  }
  IsoSender bad; bad.start(msg,24); bad.frame(0,d); bad.transmitted(1);
  bad.flowControl(2,0,nullptr); assert(bad.error==IsoSender::BAD_FC);
  IsoSender micro; micro.start(msg,24); micro.frame(0,d); micro.transmitted(1);
  uint8_t subms[8]={0x30,0,0xF1}; micro.flowControl(2,8,subms);
  assert(micro.error==IsoSender::UNSUPPORTED_FC);

  IsoReceiver rx;
  uint8_t sf[8]={3,'O','L','D',0,0,0,0}; rx.feed(0,8,sf);
  uint8_t ff[8]={0x10,24,1,2,3,4,5,6}; rx.feed(1,8,ff); rx.fcTransmitted(2);
  uint8_t wrong[8]={0x22,7,8,9,10,11,12,13}; rx.feed(3,8,wrong);
  assert(rx.error==IsoReceiver::SEQUENCE && rx.completed==1 && rx.published[0]=='O');
  rx.feed(10,8,ff); rx.tick(1009); assert(rx.active);
  rx.tick(1010); assert(!rx.active && rx.error==IsoReceiver::TIMEOUT);
  assert(rx.completed==1 && rx.publishedLength==3);
  rx.feed(2000,8,ff); rx.feed(2001,0,nullptr); assert(rx.error==IsoReceiver::FORMAT);
  uint8_t huge[8]={0x10,129}; rx.feed(2010,8,huge); assert(rx.error==IsoReceiver::CAPACITY);
  rx.feed(2020,8,ff); rx.feed(2021,8,wrong); assert(rx.error==IsoReceiver::UNEXPECTED);
  rx.feed(3000,8,sf); assert(rx.completed==2 && rx.error==IsoReceiver::OK);
  puts("PASS: SF/FF/CF/FC, exact assembly, BS, STmin, sequence wrap/error, timeouts/wrap, malformed/unsupported input, recovery");
}
