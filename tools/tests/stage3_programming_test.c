/* Native tests of production boot_programming.c / boot_uds.c.
 * Only hardware and BootImage IO are fakes; not a reimplementation of the state machine. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "boot_programming.h"
#include "boot_config.h"
#include "boot_slot.h"
#include "boot_flash.h"
#include "boot_image.h"
#include "boot_manager.h"
#include "boot_uds.h"
#include "boot_security.h"
#include "boot_update.h"
#include "boot_journal_storage.h"
static uint8_t journal_memory[2][128];
bool BootJournalStorage_Read(uint32_t copy, uint8_t *data, uint32_t size)
{ assert(copy<2 && size<=128); memcpy(data,journal_memory[copy],size); return true; }
bool BootJournalStorage_Erase(uint32_t copy)
{ assert(copy<2); memset(journal_memory[copy],255,128); return true; }
bool BootJournalStorage_Program(uint32_t copy, uint32_t offset, const uint8_t *data, uint32_t size)
{ assert(copy<2 && offset+size<=128); memcpy(journal_memory[copy]+offset,data,size); return true; }
void BootManager_RefreshActiveSlot(void) { }
static uint8_t flash_memory[BOOT_PFLASH_SIZE];
static uint8_t image[BOOT_SLOT_A_SIZE];
static BootSlotIdType active, target;
static BootImageValidationResultType validation;
static unsigned writes, erases, checks, loads, pending_count;
static bool flash_fail, unlocked;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while (0)

bool BootFlash_ConfigureSlots(BootSlotIdType a, BootSlotIdType t)
{ active=a; target=t; return a!=t && BootSlot_IsValidId(t); }
bool BootFlash_IsRangeValid(uint32_t a,uint32_t n)
{ return target!=active && BootSlot_ContainsRange(target,a,n); }
bool BootFlash_Erase(uint32_t a,uint32_t n)
{
    assert(BootFlash_IsRangeValid(a,n) && n==BOOT_FLASH_SECTOR_SIZE && a%n==0);
    if (flash_fail) { return false; } erases++; memset(flash_memory+a,255,n); return true;
}
bool BootFlash_Program(uint32_t a,const uint8_t *d,uint32_t n)
{
    uint32_t i;
    assert(BootFlash_IsRangeValid(a,n) && a%8==0 && n%8==0);
    if (flash_fail) { return false; }
    for(i=0;i<n;i++) { assert((flash_memory[a+i] & d[i])==d[i]); flash_memory[a+i]=d[i]; }
    writes++; return true;
}
BootImageValidationResultType BootImage_Validate(BootSlotIdType slot, BootImageInfoType *info)
{
    assert(slot==target);
    assert(info->header.software_version==0x03000000u);
    checks++; return validation;
}
BootImageValidationResultType BootImage_LoadInfo(BootSlotIdType slot, BootImageInfoType *info)
{
    assert(slot==target);
    assert(memcmp(flash_memory+BootSlot_GetHeaderAddress(slot),
                  &BootProgramming_Context.staged_header,64)==0);
    info->valid=validation==BOOT_IMAGE_VALID; loads++; return validation;
}
BootSlotIdType BootManager_GetActiveSlot(void) { return active; }
uint32_t BootManager_GetActiveVersion(void) { return 0x02000000u; }
void BootSecurity_Init(void) { unlocked=false; }
bool BootSecurity_IsUnlocked(void) { return unlocked; }
uint32_t BootSecurity_GenerateSeed(void) { return 0x12345678u; }
bool BootSecurity_ValidateKey(uint32_t key) { unlocked=(key==0x12345678u); return unlocked; }
bool BootIsoTp_Transmit(const BootIsoTp_PduType *r)
{ assert(r->length==3 && r->data[2]==0x78); pending_count++; return true; }
static bool pending(void) { pending_count++; return true; }
static bool pending_failure(void) { return false; }

static void init(BootSlotIdType a)
{
    BootUpdateJournal j = {0};
    BootImageHeaderType h;
    uint32_t i;
    active=a; target=BootSlot_GetOtherSlot(a);
    if(a==BOOT_SLOT_ID_UNKNOWN) { target=BOOT_SLOT_ID_A; }
    memset(flash_memory,0xA5,sizeof(flash_memory));
    memset(image,0xFF,sizeof(image)); memset(&h,0,sizeof(h));
    for(i=0;i<256;i++) { image[i]=(uint8_t)i; }
    h.magic=BOOT_IMAGE_HEADER_MAGIC; h.header_version=1; h.image_size=256;
    h.software_version=0x03000000u; h.vector_address=BootSlot_GetBaseAddress(target);
    memcpy(image+BOOT_SLOT_A_SIZE-BOOT_FLASH_SECTOR_SIZE,&h,sizeof(h));
    writes=erases=checks=loads=pending_count=0; flash_fail=false; validation=BOOT_IMAGE_VALID;
    memset(journal_memory,255,sizeof(journal_memory));
    j.active_slot=(uint32_t)a; j.target_slot=(uint32_t)target;
    j.state=BOOT_UPDATE_ABORTED; j.last_result=BOOT_UPDATE_COMMISSIONED;
    assert(BootJournal_Initialize(&j)); assert(BootUpdate_LoadRecoveryState());
    BootProgramming_Init(a,0x02000000u);
    BootProgramming_SetAccess(true,true);
}
static uint8_t start(void)
{
    uint32_t base=BootSlot_GetBaseAddress(target), size=BootSlot_GetSize(target);
    uint8_t rc=BootProgramming_Erase(base,size,pending);
    return rc ? rc : BootProgramming_Download(base,size);
}
static uint8_t transfer_all(uint32_t chunk_size)
{
    uint32_t sent=0;
    uint8_t seq=1;
    while(sent<sizeof(image)) {
        uint32_t n=sizeof(image)-sent; uint8_t rc;
        if(n>chunk_size) { n=chunk_size; }
        rc=BootProgramming_Transfer(seq,image+sent,n);
        if(rc) { return rc; }
        sent+=n; seq++;
    }
    return 0;
}
int main(void)
{
    BootIsoTp_PduType q,r;
    uint32_t base,size; unsigned before;
    init(BOOT_SLOT_ID_A); CHECK(target==BOOT_SLOT_ID_B);
    CHECK(!BootProgramming_CanReset());
    BootProgramming_SetAccess(false,false);
    CHECK(BootProgramming_Erase(BOOT_SLOT_B_BASE_ADDRESS,BOOT_SLOT_B_SIZE,pending)==0x22);
    BootProgramming_SetAccess(true,false);
    CHECK(BootProgramming_Erase(BOOT_SLOT_B_BASE_ADDRESS,BOOT_SLOT_B_SIZE,pending)==0x33);
    BootProgramming_SetAccess(true,true);
    CHECK(BootProgramming_Erase(BOOT_SLOT_A_BASE_ADDRESS,BOOT_SLOT_A_SIZE,pending)==0x31);
    CHECK(BootProgramming_Erase(0,BOOT_SLOT_A_SIZE,pending)==0x31);
    CHECK(BootProgramming_Erase(BOOT_FLEXNVM_BASE_ADDRESS,BOOT_SLOT_A_SIZE,pending)==0x31);
    CHECK(writes==0 && erases==0);
    CHECK(BootProgramming_Download(BOOT_SLOT_B_BASE_ADDRESS,BOOT_SLOT_B_SIZE)==0x24);
    CHECK(start()==0);
    CHECK(BootProgramming_Context.target_slot!=BootProgramming_Context.active_slot);
    CHECK(BootProgramming_Download(BOOT_SLOT_B_BASE_ADDRESS,BOOT_SLOT_B_SIZE)==0x24);
    CHECK(BootProgramming_Erase(BOOT_SLOT_B_BASE_ADDRESS,BOOT_SLOT_B_SIZE,pending)==0x24);
    CHECK(BootProgramming_Transfer(2,image,127)==0x73);
    CHECK(BootProgramming_Context.received_size==0);
    CHECK(BootProgramming_Transfer(1,image,0)==0x13);
    CHECK(BootProgramming_Transfer(1,image,129)==0x13);
    CHECK(BootProgramming_Exit()==0x24);
    CHECK(BootProgramming_Verify(pending)==0x24);
    CHECK(transfer_all(127)==0); /* Deliberately crosses phrase boundaries and BSC FF->00. */
    CHECK(BootProgramming_Context.received_size==sizeof(image));
    CHECK(BootProgramming_Transfer(BootProgramming_Context.expected_block_sequence_counter,image,1)==0x31);
    CHECK(!BootProgramming_CanReset());
    CHECK(flash_memory[BOOT_SLOT_B_HEADER_ADDRESS]==0); /* Header never published during transfer. */
    CHECK(BootProgramming_Exit()==0);
    CHECK(BootProgramming_Verify(pending)==0);
    CHECK(checks==1 && loads==1 && BootProgramming_CanReset());
    CHECK(flash_memory[BOOT_SLOT_A_BASE_ADDRESS]==0xA5);
    CHECK(flash_memory[0]==0xA5 && flash_memory[BOOT_V2_SHARED_METADATA_ADDRESS]==0xA5);
    CHECK(BootProgramming_Verify(pending)==0x24);
    puts("PASS A->B, alignment buffer, BSC wrap, active/reserved protection, header-last verify");

    init(BOOT_SLOT_ID_B); CHECK(target==BOOT_SLOT_ID_A); CHECK(start()==0);
    CHECK(transfer_all(128)==0 && BootProgramming_Exit()==0 && BootProgramming_Verify(pending)==0);
    CHECK(flash_memory[BOOT_SLOT_B_BASE_ADDRESS]==0xA5);
    puts("PASS B->A complete programming");

    init(BOOT_SLOT_ID_A); base=BootSlot_GetBaseAddress(target); size=BootSlot_GetSize(target);
    CHECK(BootProgramming_Erase(base,size,pending_failure)==0x72 && erases==0);
    CHECK(BootProgramming_Download(0xFFFFFFF8u,16)==0x31);
    CHECK(BootProgramming_Download(base,size+1)==0x31);
    CHECK(BootProgramming_Download(base,0)==0x31);
    CHECK(BootProgramming_Download(base+8,size-8)==0x31);
    CHECK(start()==0);
    CHECK(BootProgramming_Transfer(1,image,3)==0);
    CHECK(BootProgramming_Context.buffered_size==3);
    before=writes; CHECK(BootProgramming_Transfer(1,image,3)==0x73 && before==writes);
    BootProgramming_Abort();
    CHECK(BootProgramming_Transfer(2,image,8)==0x24);
    CHECK(BootProgramming_Download(base,size)==0x24);
    CHECK(flash_memory[BOOT_SLOT_B_HEADER_ADDRESS]==0);
    puts("PASS range overflow, strict duplicate rejection, abort and re-erase requirement");

    init(BOOT_SLOT_ID_A); CHECK(start()==0); CHECK(transfer_all(128)==0);
    CHECK(BootProgramming_Exit()==0); validation=BOOT_IMAGE_ERR_IMAGE_CRC;
    CHECK(BootProgramming_Verify(pending)==0x72 && loads==0 && !BootProgramming_CanReset());
    CHECK(BootProgramming_Context.validation_result==BOOT_IMAGE_ERR_IMAGE_CRC);
    validation=BOOT_IMAGE_ERR_HEADER_CRC; CHECK(BootProgramming_Verify(pending)==0x72);
    validation=BOOT_IMAGE_ERR_VECTOR; CHECK(BootProgramming_Verify(pending)==0x72);
    CHECK(flash_memory[BOOT_SLOT_B_HEADER_ADDRESS]==0);
    puts("PASS integrity failure propagation and no header publication");

    init(BOOT_SLOT_ID_A); CHECK(start()==0); CHECK(transfer_all(128)==0);
    CHECK(BootProgramming_Exit()==0); BootProgramming_Context.active_version=0x03000000u;
    CHECK(BootProgramming_Verify(pending)==0x31 && loads==0);
    puts("PASS non-increasing version rejected before header publication");

    init(BOOT_SLOT_ID_A); flash_fail=true;
    CHECK(BootProgramming_Erase(BOOT_SLOT_B_BASE_ADDRESS,BOOT_SLOT_B_SIZE,pending)==0x72);
    CHECK(!BootProgramming_Context.erase_complete);
    init(BOOT_SLOT_ID_A); CHECK(start()==0); flash_fail=true;
    CHECK(BootProgramming_Transfer(1,image,8)==0x72);
    CHECK(!BootProgramming_Context.download_active && !BootProgramming_CanReset());
    init(BOOT_SLOT_ID_UNKNOWN); CHECK(target==BOOT_SLOT_ID_A && start()==0);
    puts("PASS driver failures fail closed and empty-device recovery chooses A");

    init(BOOT_SLOT_ID_A); BootUds_Init();
    q.length=2;q.data[0]=0x11;q.data[1]=1;
    CHECK(BootUds_ProcessRequest(&q,&r) && r.data[2]==0x22);
    q.data[0]=0x10;q.data[1]=2; CHECK(BootUds_ProcessRequest(&q,&r) && r.data[0]==0x50);
    q.data[0]=0x11;q.data[1]=1; CHECK(BootUds_ProcessRequest(&q,&r) && r.data[2]==0x33);
    q.length=6;q.data[0]=0x27;q.data[1]=2;q.data[2]=0x12;q.data[3]=0x34;q.data[4]=0x56;q.data[5]=0x78;
    CHECK(BootUds_ProcessRequest(&q,&r) && r.data[0]==0x67);
    q.length=2;q.data[0]=0x11;q.data[1]=1;CHECK(BootUds_ProcessRequest(&q,&r) && r.data[2]==0x24);
    q.length=3;q.data[0]=0x22;q.data[1]=0xF1;q.data[2]=1;
    CHECK(BootUds_ProcessRequest(&q,&r) && r.length==4 && r.data[3]==0);
    q.data[2]=3;CHECK(BootUds_ProcessRequest(&q,&r) && r.data[3]==1);
    q.data[2]=2;CHECK(BootUds_ProcessRequest(&q,&r) && r.length==7 && r.data[3]==2);
    q.data[2]=4;CHECK(BootUds_ProcessRequest(&q,&r) && r.length==35 && r.data[3]==1 && r.data[4]==1);
    puts("PASS actual UDS session/security/reset gates and slot/version DIDs");
    puts("PASS Stage3 production C programming/UDS host suite");
    return 0;
}
