/* Production Journal/Update/Recovery/Programming/ImageInstall; only device IO
 * and image reads are mocked. Torn-write injection is NOT a physical power cut. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include "boot_config.h"
#include "boot_update.h"
#include "boot_recovery_policy.h"
#include "boot_journal_storage.h"
#include "boot_programming.h"
#include "boot_image.h"
#include "boot_slot.h"
#include "boot_flash.h"
#include "boot_fault.h"
static uint8_t storage[2][128], memory[BOOT_PFLASH_SIZE], image[BOOT_SLOT_A_SIZE];
static unsigned commits, programs, assertions;
static int fail_program = -1, torn_bytes, fail_read_copy = -1;
static bool fail_erase, fail_image, fail_flash;
static bool corrupt_readback;
static unsigned readback_after;
static BootSlotIdType configured_active, configured_target;
static jmp_buf cut;
static uint32_t armed, skip;
#define CHECK(x) do { assertions++; if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
void BootFault_Hit(BootFaultPoint point)
{ if (armed==(uint32_t)point) { if (skip) { skip--; } else { armed=0; longjmp(cut,1); } } }
bool BootJournalStorage_Read(uint32_t c,uint8_t *d,uint32_t n)
{
    assert(c<2 && n<=128);
    if ((int)c==fail_read_copy) { return false; }
    memcpy(d,storage[c],n);
    if(corrupt_readback && programs>=readback_after) { d[0]^=1; }
    return true;
}
bool BootJournalStorage_Erase(uint32_t c)
{ assert(c<2); if(fail_erase) { return false; } memset(storage[c],255,128); commits++; return true; }
bool BootJournalStorage_Program(uint32_t c,uint32_t o,const uint8_t *d,uint32_t n)
{
    unsigned i; bool fail=(int)programs++==fail_program;
    assert(c<2 && n==8 && o%8==0 && o+n<=128);
    for(i=0;i<(fail ? (unsigned)torn_bytes : n);i++) { assert((storage[c][o+i]&d[i])==d[i]); storage[c][o+i]=d[i]; }
    return !fail;
}
bool BootFlash_ConfigureSlots(BootSlotIdType a,BootSlotIdType t)
{ configured_active=a; configured_target=t; return a!=t && (uint32_t)t<2; }
bool BootFlash_Erase(uint32_t a,uint32_t n)
{
    assert(configured_active!=configured_target && BootSlot_ContainsRange(configured_target,a,n));
    assert(n==4096 && a%4096==0);
    if(fail_flash) { return false; } memset(memory+a,255,n); return true;
}
bool BootFlash_Program(uint32_t a,const uint8_t *d,uint32_t n)
{
    uint32_t i;
    assert(configured_active!=configured_target && BootSlot_ContainsRange(configured_target,a,n));
    assert(a%8==0 && n%8==0);
    if(fail_flash) { return false; }
    for(i=0;i<n;i++) { assert((memory[a+i]&d[i])==d[i]); memory[a+i]=d[i]; } return true;
}
BootImageValidationResultType BootImage_Validate(BootSlotIdType slot,BootImageInfoType *info)
{
    BootImageHeaderType *h=&info->header;
    info->valid=false;
    if(fail_image || (uint32_t)slot>1 || h->magic!=BOOT_IMAGE_HEADER_MAGIC ||
       h->header_crc32!=BootJournal_Crc(h,60) || h->image_size!=256 ||
       h->image_crc32!=BootJournal_Crc(memory+BootSlot_GetBaseAddress(slot),h->image_size)) { return BOOT_IMAGE_ERR_IMAGE_CRC; }
    info->valid=true; return BOOT_IMAGE_VALID;
}
BootImageValidationResultType BootImage_LoadInfo(BootSlotIdType slot,BootImageInfoType *info)
{ memcpy(&info->header,memory+BootSlot_GetHeaderAddress(slot),64); return BootImage_Validate(slot,info); }
static BootImageHeaderType header(BootSlotIdType slot,uint32_t version)
{
    BootImageHeaderType h={0};
    h.magic=BOOT_IMAGE_HEADER_MAGIC; h.header_version=1; h.software_version=version; h.image_size=256;
    h.image_crc32=BootJournal_Crc(memory+BootSlot_GetBaseAddress(slot),256);
    h.vector_address=BootSlot_GetBaseAddress(slot); h.entry_address=h.vector_address+129;
    h.header_crc32=BootJournal_Crc(&h,60); return h;
}
static void setup(BootSlotIdType active)
{
    BootImageHeaderType h;
    BootSlotIdType target=BootSlot_GetOtherSlot(active);
    memset(storage,255,sizeof(storage)); memset(memory,255,sizeof(memory));
    memset(memory+BootSlot_GetBaseAddress(active),0xA5,256);
    h=header(active,0x02000000u); memcpy(memory+BootSlot_GetHeaderAddress(active),&h,64);
    fail_program=-1; torn_bytes=0; fail_read_copy=-1; fail_erase=fail_image=fail_flash=false; armed=0; skip=0;
    corrupt_readback=false; readback_after=0;
    commits=programs=0; assert(!BootUpdate_LoadRecoveryState());
    assert(BootUpdate_InitializeJournal(active));
    memset(image,255,sizeof(image)); memset(image,0x69,256);
    memcpy(memory+BootSlot_GetBaseAddress(target),image,256);
    h=header(target,0x03000000u); memcpy(image+BOOT_SLOT_A_SIZE-4096,&h,64);
    BootProgramming_Init(active,0x02000000u); BootProgramming_SetAccess(true,true);
}
static bool reach(uint32_t state)
{
    BootSlotIdType active=(BootSlotIdType)BootUpdate_Current.active_slot;
    BootSlotIdType target=BootSlot_GetOtherSlot(active);
    BootImageHeaderType h;
    if(!BootUpdate_Begin(active,target,BOOT_SLOT_A_SIZE)) { return false; }
    if(state==BOOT_UPDATE_PREPARING) { return true; }
    if(!BootUpdate_SetState(BOOT_UPDATE_ERASING)) { return false; }
    if(state==BOOT_UPDATE_ERASING) { return true; }
    if(!BootUpdate_SetState(BOOT_UPDATE_DOWNLOAD_READY)) { return false; }
    if(state==BOOT_UPDATE_DOWNLOAD_READY) { return true; }
    if(!BootUpdate_SetState(BOOT_UPDATE_PROGRAMMING)) { return false; }
    if(state==BOOT_UPDATE_PROGRAMMING) { return true; }
    h=header(target,0x03000000u);
    if(!BootUpdate_MarkTransferComplete(&h,0)) { return false; }
    if(state==BOOT_UPDATE_TRANSFER_COMPLETE) { return true; }
    if(!BootUpdate_SetState(BOOT_UPDATE_VERIFYING)) { return false; }
    if(state==BOOT_UPDATE_VERIFYING) { return true; }
    if(!BootUpdate_SetState(BOOT_UPDATE_VERIFIED)) { return false; }
    if(state==BOOT_UPDATE_VERIFIED) { return true; }
    return BootUpdate_MarkVerified(false);
}
static void full_program(void)
{
    unsigned offset, n;
    uint8_t sequence=1;
    BootSlotIdType target=BootProgramming_Context.target_slot;
    assert(BootProgramming_Erase(BootSlot_GetBaseAddress(target),BOOT_SLOT_A_SIZE,NULL)==0);
    assert(BootProgramming_Download(BootSlot_GetBaseAddress(target),BOOT_SLOT_A_SIZE)==0);
    for(offset=0;offset<sizeof(image);offset+=n) {
        n=sizeof(image)-offset; if(n>127) { n=127; }
        assert(BootProgramming_Transfer(sequence++,image+offset,n)==0);
    }
    assert(BootProgramming_Exit()==0); assert(BootProgramming_Verify(NULL)==0);
}
int main(void)
{
    BootUpdateJournal a,b,selected;
    unsigned state, slot, i, j, before, old_crc;
    uint8_t snapshot[2][128];
    static const bool allowed[10][10] = {
        {[1]=true}, {[2]=true,[9]=true}, {[3]=true,[9]=true}, {[4]=true,[9]=true},
        {[5]=true,[9]=true}, {[6]=true,[9]=true}, {[7]=true,[9]=true},
        {[8]=true,[9]=true}, {[0]=true,[1]=true,[9]=true}, {[1]=true,[9]=true}
    };
    for(i=0;i<10;i++) { for(j=0;j<10;j++) { CHECK(BootUpdate_TransitionAllowed(i,j)==allowed[i][j]); } }
    CHECK(BootJournal_Crc("123456789",9)==0xCBF43926u);
    CHECK(BootJournal_SequenceNewer(0,0xFFFFFFFFu)); CHECK(!BootJournal_SequenceNewer(0xFFFFFFFFu,0));
    CHECK(!BootJournal_SequenceNewer(1,1)); CHECK(!BootJournal_SequenceNewer(0x80000000u,0));
    setup(BOOT_SLOT_ID_A); a=BootUpdate_Current;
    CHECK(BootJournal_Validate(&a)); a.magic^=1; CHECK(!BootJournal_Validate(&a));
    a=BootUpdate_Current; a.commit_marker=0; CHECK(!BootJournal_Validate(&a));
    CHECK(!BootUpdate_SetState(BOOT_UPDATE_VERIFYING));
    CHECK(!BootUpdate_Begin(BOOT_SLOT_ID_A,BOOT_SLOT_ID_A,BOOT_SLOT_A_SIZE));
    CHECK(!BootUpdate_Begin(BOOT_SLOT_ID_B,BOOT_SLOT_ID_A,BOOT_SLOT_A_SIZE));
    CHECK(BootJournal_LoadLatest(&a)==BOOT_JOURNAL_VALID); b=a; b.sequence=0;
    a.sequence=0xFFFFFFFFu; a.journal_crc=BootJournal_Crc(&a,120); b.journal_crc=BootJournal_Crc(&b,120);
    memcpy(storage[0],&a,128); memcpy(storage[1],&b,128);
    CHECK(BootJournal_LoadLatest(&selected)==BOOT_JOURNAL_VALID && selected.sequence==0);
    storage[1][20]^=1; CHECK(BootJournal_LoadLatest(&selected)==BOOT_JOURNAL_VALID && selected.sequence==0xFFFFFFFFu);
    memcpy(storage[1],&b,128); storage[0][0]^=1;
    CHECK(BootJournal_LoadLatest(&selected)==BOOT_JOURNAL_VALID && selected.sequence==0);
    storage[1][0]^=1; CHECK(!BootUpdate_LoadRecoveryState());
    CHECK(!BootUpdate_IsSlotBootable(BOOT_SLOT_ID_A) && !BootUpdate_IsSlotBootable(BOOT_SLOT_ID_B));
    CHECK(!BootUpdate_Begin(BOOT_SLOT_ID_UNKNOWN,BOOT_SLOT_ID_A,BOOT_SLOT_A_SIZE));
    /* Equal sequence with conflicting records, and half-range: fail safe. */
    a.sequence=b.sequence=7; a.transaction_id=10; b.transaction_id=11;
    a.journal_crc=BootJournal_Crc(&a,120); b.journal_crc=BootJournal_Crc(&b,120);
    memcpy(storage[0],&a,128); memcpy(storage[1],&b,128);
    CHECK(BootJournal_LoadLatest(&selected)==BOOT_JOURNAL_INVALID);
    a.sequence=b.sequence+0x80000000u; a.journal_crc=BootJournal_Crc(&a,120); memcpy(storage[0],&a,128);
    CHECK(BootJournal_LoadLatest(&selected)==BOOT_JOURNAL_INVALID);
    puts("PASS CRC/commit marker, single/both corruption, wrap/equal/ambiguous sequence, fail-safe");

    /* Every phrase boundary, with every possible prefix of a torn phrase. */
    for(i=0;i<16;i++) { for(j=0;j<8;j++) {
        setup(BOOT_SLOT_ID_A); a=BootUpdate_Current; b=a; b.last_result=BOOT_UPDATE_INTERRUPTED;
        fail_program=(int)(programs+i); torn_bytes=(int)j;
        CHECK(!BootJournal_Commit(&b)); fail_program=-1;
        CHECK(BootJournal_LoadLatest(&selected)==BOOT_JOURNAL_VALID && selected.sequence==a.sequence);
        CHECK(selected.state==a.state && selected.active_slot==0);
    } }
    setup(BOOT_SLOT_ID_A); CHECK(reach(BOOT_UPDATE_PROGRAMMING));
    fail_erase=true; a=BootUpdate_Current;
    CHECK(!BootUpdate_SetProgress(32768,255)); CHECK(BootUpdate_StorageFault);
    CHECK(BootUpdate_Current.sequence==a.sequence && BootUpdate_Current.committed_size==0);
    CHECK(!BootUpdate_SetState(BOOT_UPDATE_TRANSFER_COMPLETE));
    CHECK(!BootUpdate_IsSlotBootable(BOOT_SLOT_ID_B));
    fail_erase=false; CHECK(BootUpdate_LoadRecoveryState()); CHECK(BootUpdate_Current.state==BOOT_UPDATE_ABORTED);
    setup(BOOT_SLOT_ID_A); a=BootUpdate_Current; fail_read_copy=1;
    CHECK(!BootJournal_Commit(&a)); fail_read_copy=-1;
    setup(BOOT_SLOT_ID_A); CHECK(reach(BOOT_UPDATE_PROGRAMMING));
    readback_after=programs+16; corrupt_readback=true;
    CHECK(!BootUpdate_SetProgress(32768,1)); CHECK(BootUpdate_StorageFault);
    CHECK(BootUpdate_Current.committed_size==0 && !BootUpdate_IsSlotBootable(BOOT_SLOT_ID_B));
    corrupt_readback=false; CHECK(BootUpdate_LoadRecoveryState()); CHECK(BootUpdate_Current.state==BOOT_UPDATE_ABORTED);
    puts("PASS 128 torn-write positions, erase/read failure, freeze on Journal failure");

    for(slot=0;slot<2;slot++) { for(state=1;state<=8;state++) {
        setup((BootSlotIdType)slot); old_crc=BootJournal_Crc(memory+BootSlot_GetBaseAddress((BootSlotIdType)slot),BOOT_SLOT_A_SIZE);
        CHECK(reach(state)); CHECK(BootUpdate_LoadRecoveryState());
        CHECK(BootUpdate_IsSlotBootable((BootSlotIdType)slot));
        CHECK(BootUpdate_IsSlotBootable((BootSlotIdType)(1-slot))==(state>=5));
        CHECK(BootUpdate_Current.state==(state<5 ? BOOT_UPDATE_ABORTED : BOOT_UPDATE_PENDING_ACTIVATION));
        CHECK(BootJournal_Crc(memory+BootSlot_GetBaseAddress((BootSlotIdType)slot),BOOT_SLOT_A_SIZE)==old_crc);
        before=commits; CHECK(BootUpdate_LoadRecoveryState()); CHECK(before==commits);
    } }
    for(state=5;state<=8;state++) {
        setup(BOOT_SLOT_ID_A); CHECK(reach(state)); memory[BOOT_SLOT_B_BASE_ADDRESS]^=1;
        CHECK(!BootUpdate_LoadRecoveryState()); CHECK(BootUpdate_Current.state==BOOT_UPDATE_ABORTED);
        CHECK(!BootUpdate_IsSlotBootable(BOOT_SLOT_ID_B)); CHECK(BootUpdate_IsSlotBootable(BOOT_SLOT_ID_A));
    }
    puts("PASS all 8 recovery states in A/B directions; invalid image rejected; old slot unchanged");

    setup(BOOT_SLOT_ID_A); CHECK(reach(BOOT_UPDATE_PROGRAMMING)); before=commits;
    CHECK(BootUpdate_SetProgress(32760,255)); CHECK(commits==before);
    CHECK(BootUpdate_SetProgress(32768,0)); CHECK(commits==before+1 && BootUpdate_Current.committed_size==32768);
    CHECK(BootUpdate_SetProgress(32776,1)); CHECK(commits==before+1);
    CHECK(BootUpdate_SetProgress(65536,2)); CHECK(commits==before+2);
    a=BootUpdate_Current; CHECK(BootUpdate_Abort(BOOT_UPDATE_PROGRAM_FAILED));
    CHECK(BootUpdate_Current.transaction_id==a.transaction_id && BootUpdate_Current.target_slot==a.target_slot);
    CHECK(BootUpdate_Current.committed_size==a.committed_size && BootUpdate_Current.last_result==BOOT_UPDATE_PROGRAM_FAILED);
    CHECK(!BootJournal_Clear());
    CHECK(BootUpdate_Begin(BOOT_SLOT_ID_A,BOOT_SLOT_ID_B,BOOT_SLOT_A_SIZE));
    CHECK(BootUpdate_Current.transaction_id==a.transaction_id+1 && BootUpdate_Current.committed_size==0);
    puts("PASS checkpoint granularity, BSC wrap, abort diagnostic preservation, full restart");
    setup(BOOT_SLOT_ID_A); CHECK(!BootUpdate_InitializeJournal(BOOT_SLOT_ID_A));
    memset(storage,255,sizeof(storage)); CHECK(!BootUpdate_LoadRecoveryState());
    CHECK(!BootUpdate_InitializeJournal(BOOT_SLOT_ID_UNKNOWN)); /* A is still a valid protected image. */
    CHECK(!BootUpdate_InitializeJournal(BOOT_SLOT_ID_B)); /* B has no published header. */
    CHECK(BootUpdate_InitializeJournal(BOOT_SLOT_ID_A)); CHECK(!BootUpdate_IsSlotBootable(BOOT_SLOT_ID_B));
    memset(storage,255,sizeof(storage)); memset(memory,255,sizeof(memory));
    CHECK(!BootUpdate_LoadRecoveryState()); CHECK(BootUpdate_InitializeJournal(BOOT_SLOT_ID_UNKNOWN));
    CHECK(BootUpdate_Begin(BOOT_SLOT_ID_UNKNOWN,BOOT_SLOT_ID_A,BOOT_SLOT_A_SIZE));
    puts("PASS explicit commissioning gates, valid legacy/old image protection, empty-device recovery");

    /* Software-reset hooks driven by actual ProgrammingManager. */
    for(i=BOOT_FAULT_AFTER_PREPARING;i<=BOOT_FAULT_AFTER_VERIFY;i++) {
        setup(BOOT_SLOT_ID_A); old_crc=BootJournal_Crc(memory+BOOT_SLOT_A_BASE_ADDRESS,BOOT_SLOT_A_SIZE);
        armed=i;
        if(setjmp(cut)==0) { full_program(); CHECK(false); }
        CHECK(BootUpdate_LoadRecoveryState());
        CHECK(BootUpdate_IsSlotBootable(BOOT_SLOT_ID_A));
        CHECK(BootUpdate_IsSlotBootable(BOOT_SLOT_ID_B)==(i>=BOOT_FAULT_AFTER_TRANSFER_EXIT));
        CHECK(BootJournal_Crc(memory+BOOT_SLOT_A_BASE_ADDRESS,BOOT_SLOT_A_SIZE)==old_crc);
    }
    /* Reset at all 17 Journal hook hits: old record, or new when final phrase committed. */
    for(i=0;i<17;i++) {
        setup(BOOT_SLOT_ID_A); a=BootUpdate_Current; memcpy(snapshot,storage,sizeof(snapshot));
        armed=BOOT_FAULT_DURING_JOURNAL_COMMIT; skip=i;
        if(setjmp(cut)==0) { b=a; b.last_result=BOOT_UPDATE_INTERRUPTED; (void)BootJournal_Commit(&b); CHECK(false); }
        CHECK(BootJournal_LoadLatest(&selected)==BOOT_JOURNAL_VALID);
        CHECK(selected.sequence==(i==16 ? a.sequence+1 : a.sequence));
        CHECK(memcmp(storage[0],snapshot[0],128)==0);
    }
    for(slot=0;slot<2;slot++) {
        setup((BootSlotIdType)slot); before=commits; full_program();
        CHECK(BootProgramming_CanReset()); CHECK(commits-before==14);
        CHECK(BootUpdate_Current.state==BOOT_UPDATE_PENDING_ACTIVATION);
        CHECK(BootUpdate_LoadRecoveryState()); CHECK(BootUpdate_IsSlotBootable((BootSlotIdType)(1-slot)));
    }
    puts("PASS 10 programming reset hooks, 17 commit hooks, complete A/B updates, 14 commits/update");
    printf("PASS Stage4 production C: %u assertions\n",assertions);
    return 0;
}
