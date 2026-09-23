#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "boot_can.h"
#include "boot_isotp.h"
static BootCan_FrameType sent[64], incoming;
static unsigned sends, receives;
static bool available, tx_fail;
bool BootCan_Transmit(const BootCan_FrameType *f)
{ if(tx_fail) { return false; } assert(sends<64); sent[sends++]=*f; return true; }
bool BootCan_Receive(BootCan_FrameType *f)
{ receives++; if(!available) { return false; } *f=incoming; return true; }
static void setup(uint8_t bs,uint8_t st)
{
    sends=receives=0; available=true; tx_fail=false; memset(&incoming,0,sizeof(incoming));
    incoming.id=BOOT_CAN_REQUEST_ID; incoming.dlc=8;
    incoming.data[0]=0x30; incoming.data[1]=bs; incoming.data[2]=st; BootIsoTp_Init();
}
int main(void)
{
    BootIsoTp_PduType p;
    unsigned i,j,offset;
    for(i=0;i<256;i++) { p.data[i]=(uint8_t)i; }
    setup(0,0); p.length=35; assert(BootIsoTp_Transmit(&p)); assert(sends==6 && receives==1);
    assert(sent[0].data[0]==0x10 && sent[0].data[1]==35);
    offset=6;
    for(i=1;i<sends;i++) {
        assert(sent[i].data[0]==(0x20|i));
        for(j=1;j<8 && offset<p.length;j++) { assert(sent[i].data[j]==p.data[offset++]); }
    }
    assert(offset==35);
    setup(2,0); assert(BootIsoTp_Transmit(&p)); assert(receives==3 && sends==6);
    setup(0,1); assert(!BootIsoTp_Transmit(&p)); assert(sends==1);
    setup(0,0); incoming.data[0]=0x31; assert(!BootIsoTp_Transmit(&p));
    setup(0,0); incoming.data[0]=0x32; assert(!BootIsoTp_Transmit(&p));
    setup(0,0); incoming.dlc=2; assert(!BootIsoTp_Transmit(&p));
    setup(0,0); available=false; assert(!BootIsoTp_Transmit(&p)); assert(receives==BOOT_ISOTP_TIMEOUT_LOOPS);
    setup(0,0); tx_fail=true; assert(!BootIsoTp_Transmit(&p));
    setup(0,0); p.length=7; assert(BootIsoTp_Transmit(&p)); assert(sends==1 && sent[0].data[0]==7);
    setup(0,0); p.length=256; assert(BootIsoTp_Transmit(&p));
    assert(sent[16].data[0]==0x20 && sent[17].data[0]==0x21);
    setup(0,0); p.length=0; assert(!BootIsoTp_Transmit(&p));
    puts("PASS Stage4 production ISO-TP TX: 11 SF/FF/CF/FC, block-size, rollover, timeout/error cases");
    return 0;
}
