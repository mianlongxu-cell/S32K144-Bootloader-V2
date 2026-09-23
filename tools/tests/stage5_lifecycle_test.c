#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "boot_config.h"
#include "boot_crc32.h"
#include "boot_image.h"
#include "boot_lifecycle.h"
#include "boot_metadata_storage.h"
#include "boot_update.h"

static uint8_t storage[2][2][BOOT_METADATA_RECORD_SIZE];
static unsigned assertions,writes;
static int fail_program=-1;
static uint32_t torn_bytes;
static bool images_valid[2];
static uint32_t versions[2];
#define CHECK(x) do{assertions++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)

BootUpdateJournal BootUpdate_Current;
bool BootUpdate_JournalValid=true,BootUpdate_StorageFault=false;
bool BootUpdate_CompleteActivationHandoff(void){BootUpdate_Current.state=BOOT_UPDATE_IDLE;return true;}
bool BootSlot_IsValidId(BootSlotIdType slot){return slot==BOOT_SLOT_ID_A||slot==BOOT_SLOT_ID_B;}
BootImageValidationResultType BootImage_LoadInfo(BootSlotIdType slot,BootImageInfoType *info)
{
    memset(info,0,sizeof(*info));info->slot_id=slot;info->valid=images_valid[slot];
    info->header.software_version=versions[slot];
    return info->valid?BOOT_IMAGE_VALID:BOOT_IMAGE_ERR_EMPTY;
}
bool BootMetadataStorage_Read(BootSlotIdType slot,uint32_t copy,uint8_t *data,uint32_t size)
{assert(slot<=1&&copy<=1&&size<=BOOT_METADATA_RECORD_SIZE);memcpy(data,storage[slot][copy],size);return true;}
bool BootMetadataStorage_Erase(BootSlotIdType slot,uint32_t copy)
{assert(slot<=1&&copy<=1);memset(storage[slot][copy],0xFF,BOOT_METADATA_RECORD_SIZE);return true;}
bool BootMetadataStorage_Program(BootSlotIdType slot,uint32_t copy,uint32_t offset,const uint8_t *data,uint32_t size)
{
    uint32_t i,n=size;assert(slot<=1&&copy<=1&&offset+size<=BOOT_METADATA_RECORD_SIZE&&size==8);
    if((int)writes==fail_program){n=torn_bytes;}writes++;
    for(i=0;i<n;i++){assert((storage[slot][copy][offset+i]&data[i])==data[i]);storage[slot][copy][offset+i]=data[i];}
    return n==size;
}
uint32_t BootCrc32_Calculate(const uint8_t *data,uint32_t length)
{
    uint32_t crc=0xFFFFFFFFu,i;uint8_t bit;
    for(i=0;i<length;i++){crc^=data[i];for(bit=0;bit<8;bit++){crc=(crc&1u)?((crc>>1)^0xEDB88320u):(crc>>1);}}
    return crc^0xFFFFFFFFu;
}
static void reset_storage(void)
{
    memset(storage,0xFF,sizeof(storage));writes=0;fail_program=-1;torn_bytes=0;
    images_valid[0]=images_valid[1]=true;versions[0]=0x000C0000u;versions[1]=0x000D0000u;
    BootUpdate_JournalValid=true;BootUpdate_StorageFault=false;memset(&BootUpdate_Current,0,sizeof(BootUpdate_Current));
    BootUpdate_Current.state=BOOT_UPDATE_IDLE;BootUpdate_Current.active_slot=0;BootUpdate_Current.target_slot=1;
}
static BootSlotMetadataType record(BootSlotIdType slot,uint32_t version,BootImageState state,uint32_t seq)
{
    BootSlotMetadataType r={0};r.magic=BOOT_SLOT_METADATA_MAGIC;r.format_version=BOOT_SLOT_METADATA_FORMAT;
    r.slot_id=slot;r.image_version=version;r.state=state;r.sequence=seq;
    r.failed_slot=BOOT_SLOT_ID_UNKNOWN;r.rollback_target=BOOT_SLOT_ID_UNKNOWN;
    r.commit_marker=BOOT_SLOT_METADATA_COMMIT_MARKER;
    r.metadata_crc32=BootCrc32_Calculate((const uint8_t*)&r,offsetof(BootSlotMetadataType,metadata_crc32));return r;
}
static bool init_confirmed(BootSlotIdType slot,uint32_t version)
{BootSlotMetadataType r=record(slot,version,BOOT_IMAGE_STATE_CONFIRMED,0);return BootMetadata_Initialize(slot,&r);}

int main(void)
{
    BootSlotMetadataType r,a,b;BootMetadataStatus status;unsigned before,i,j;
    BootImageInfoType ia={0},ib={0};
    CHECK(BootCrc32_Calculate((const uint8_t*)"123456789",9)==0xCBF43926u);
    reset_storage();r=record(BOOT_SLOT_ID_A,versions[0],BOOT_IMAGE_STATE_PENDING,0);
    CHECK(!BootMetadata_Initialize(BOOT_SLOT_ID_A,&r));
    CHECK(init_confirmed(BOOT_SLOT_ID_A,versions[0]));
    CHECK(BootMetadata_Load(BOOT_SLOT_ID_A,&r)==BOOT_METADATA_STATUS_VALID&&r.state==BOOT_IMAGE_STATE_CONFIRMED);
    CHECK(!BootMetadata_SetState(BOOT_SLOT_ID_A,BOOT_IMAGE_STATE_TRIAL,BOOT_RESET_SOFTWARE,0));

    reset_storage();CHECK(BootMetadata_InstallVerified(BOOT_SLOT_ID_B,versions[1],1,BOOT_RESET_SOFTWARE));
    CHECK(BootMetadata_SetState(BOOT_SLOT_ID_B,BOOT_IMAGE_STATE_PENDING,BOOT_RESET_SOFTWARE,BOOT_LIFECYCLE_NONE));
    CHECK(!BootMetadata_SetState(BOOT_SLOT_ID_B,BOOT_IMAGE_STATE_CONFIRMED,BOOT_RESET_SOFTWARE,BOOT_LIFECYCLE_CONFIRMED));
    CHECK(BootMetadata_SetState(BOOT_SLOT_ID_B,BOOT_IMAGE_STATE_TRIAL,BOOT_RESET_POWER_ON,BOOT_LIFECYCLE_TRIAL_STARTED));
    CHECK(BootMetadata_IncrementAttempt(BOOT_SLOT_ID_B,BOOT_RESET_WATCHDOG));
    CHECK(BootMetadata_Load(BOOT_SLOT_ID_B,&r)==BOOT_METADATA_STATUS_VALID&&r.boot_attempts==2&&r.last_reset_reason==BOOT_RESET_WATCHDOG);
    CHECK(BootMetadata_SetState(BOOT_SLOT_ID_B,BOOT_IMAGE_STATE_CONFIRMED,BOOT_RESET_SOFTWARE,BOOT_LIFECYCLE_CONFIRMED));
    before=writes;CHECK(BootMetadata_SetState(BOOT_SLOT_ID_B,BOOT_IMAGE_STATE_CONFIRMED,BOOT_RESET_SOFTWARE,BOOT_LIFECYCLE_CONFIRMED));CHECK(writes==before);

    reset_storage();CHECK(BootMetadata_InstallVerified(BOOT_SLOT_ID_B,versions[1],1,0));
    CHECK(BootMetadata_SetState(BOOT_SLOT_ID_B,BOOT_IMAGE_STATE_PENDING,0,0));
    fail_program=(int)writes;torn_bytes=4;
    CHECK(!BootMetadata_SetState(BOOT_SLOT_ID_B,BOOT_IMAGE_STATE_TRIAL,0,BOOT_LIFECYCLE_TRIAL_STARTED));
    fail_program=-1;CHECK(BootMetadata_Load(BOOT_SLOT_ID_B,&r)==BOOT_METADATA_STATUS_VALID&&r.state==BOOT_IMAGE_STATE_PENDING);
    storage[1][0][0]^=1;CHECK(BootMetadata_Load(BOOT_SLOT_ID_B,&r)==BOOT_METADATA_STATUS_VALID&&r.state==BOOT_IMAGE_STATE_PENDING);
    storage[1][1][0]^=1;CHECK(BootMetadata_Load(BOOT_SLOT_ID_B,&r)==BOOT_METADATA_STATUS_INVALID);

    /* Every Metadata phrase boundary and every torn prefix retains old PENDING. */
    for(i=0;i<10;i++){for(j=0;j<8;j++){
        reset_storage();CHECK(BootMetadata_InstallVerified(BOOT_SLOT_ID_B,versions[1],1,0));
        CHECK(BootMetadata_SetState(BOOT_SLOT_ID_B,BOOT_IMAGE_STATE_PENDING,0,0));
        fail_program=(int)(writes+i);torn_bytes=j;
        CHECK(!BootMetadata_SetState(BOOT_SLOT_ID_B,BOOT_IMAGE_STATE_TRIAL,0,BOOT_LIFECYCLE_TRIAL_STARTED));
        fail_program=-1;CHECK(BootMetadata_Load(BOOT_SLOT_ID_B,&r)==BOOT_METADATA_STATUS_VALID&&r.state==BOOT_IMAGE_STATE_PENDING);
    }}

    reset_storage();a=record(BOOT_SLOT_ID_A,versions[0],BOOT_IMAGE_STATE_CONFIRMED,0xFFFFFFFFu);
    b=record(BOOT_SLOT_ID_A,versions[0],BOOT_IMAGE_STATE_CONFIRMED,0u);
    memcpy(storage[0][0],&a,sizeof(a));memcpy(storage[0][1],&b,sizeof(b));
    CHECK(BootMetadata_SequenceNewer(0u,0xFFFFFFFFu));
    CHECK(BootMetadata_Load(BOOT_SLOT_ID_A,&r)==BOOT_METADATA_STATUS_VALID&&r.sequence==0u);

    ia.valid=ib.valid=true;ia.header.software_version=versions[0];ib.header.software_version=versions[1];
    CHECK(BootPolicy_SelectLifecycle(&ia,BOOT_IMAGE_STATE_CONFIRMED,&ib,BOOT_IMAGE_STATE_CONFIRMED)==BOOT_TARGET_SLOT_B);
    ib.header.software_version=ia.header.software_version;
    CHECK(BootPolicy_SelectLifecycle(&ia,BOOT_IMAGE_STATE_CONFIRMED,&ib,BOOT_IMAGE_STATE_CONFIRMED)==BOOT_TARGET_SLOT_A);
    ib.header.software_version=0x00010000u;
    CHECK(BootPolicy_SelectLifecycle(&ia,BOOT_IMAGE_STATE_CONFIRMED,&ib,BOOT_IMAGE_STATE_PENDING)==BOOT_TARGET_SLOT_B);
    ib.header.software_version=0x00FF0000u;
    CHECK(BootPolicy_SelectLifecycle(&ia,BOOT_IMAGE_STATE_CONFIRMED,&ib,BOOT_IMAGE_STATE_INVALID)==BOOT_TARGET_SLOT_A);

    reset_storage();CHECK(init_confirmed(BOOT_SLOT_ID_A,versions[0]));
    BootUpdate_Current.state=BOOT_UPDATE_PENDING_ACTIVATION;BootUpdate_Current.transaction_id=9;
    CHECK(BootLifecycle_Init(BOOT_RESET_SOFTWARE));
    CHECK(BootUpdate_Current.state==BOOT_UPDATE_IDLE);
    CHECK(BootLifecycle_GetMetadata(BOOT_SLOT_ID_B,&r,&status)&&status==BOOT_METADATA_STATUS_VALID&&r.state==BOOT_IMAGE_STATE_PENDING);
    CHECK(BootLifecycle_PrepareBoot(BOOT_SLOT_ID_B));
    CHECK(BootLifecycle_GetMetadata(BOOT_SLOT_ID_B,&r,&status)&&r.state==BOOT_IMAGE_STATE_TRIAL&&r.boot_attempts==1);
    CHECK(BootLifecycle_PrepareBoot(BOOT_SLOT_ID_B));
    CHECK(BootLifecycle_Confirm(BOOT_SLOT_ID_B));before=writes;CHECK(BootLifecycle_Confirm(BOOT_SLOT_ID_B));CHECK(writes==before);

    reset_storage();CHECK(init_confirmed(BOOT_SLOT_ID_A,versions[0]));
    BootUpdate_Current.state=BOOT_UPDATE_PENDING_ACTIVATION;BootUpdate_Current.transaction_id=10;
    CHECK(BootLifecycle_Init(BOOT_RESET_SOFTWARE));
    CHECK(BootLifecycle_PrepareBoot(BOOT_SLOT_ID_B));CHECK(BootLifecycle_PrepareBoot(BOOT_SLOT_ID_B));CHECK(BootLifecycle_PrepareBoot(BOOT_SLOT_ID_B));
    CHECK(BootLifecycle_Init(BOOT_RESET_WATCHDOG));
    CHECK(BootLifecycle_GetMetadata(BOOT_SLOT_ID_B,&r,&status)&&r.state==BOOT_IMAGE_STATE_INVALID&&r.boot_attempts==3);
    ia.valid=ib.valid=true;ia.header.software_version=versions[0];ib.header.software_version=versions[1];
    CHECK(BootLifecycle_Select(&ia,&ib)==BOOT_TARGET_SLOT_A);
    CHECK(BootLifecycle_GetLastResult()==BOOT_LIFECYCLE_ROLLBACK_OCCURRED);
    CHECK(BootLifecycle_GetFailedSlot()==BOOT_SLOT_ID_B&&BootLifecycle_GetRollbackTarget()==BOOT_SLOT_ID_A);
    CHECK(BootMetadata_InstallVerified(BOOT_SLOT_ID_B,0x000E0000u,11,BOOT_RESET_SOFTWARE));
    CHECK(BootMetadata_SetState(BOOT_SLOT_ID_B,BOOT_IMAGE_STATE_PENDING,0,0));

    /* A torn CONFIRMED commit recovers TRIAL; retry confirms. */
    CHECK(BootMetadata_SetState(BOOT_SLOT_ID_B,BOOT_IMAGE_STATE_TRIAL,0,BOOT_LIFECYCLE_TRIAL_STARTED));
    fail_program=(int)(writes+4u);torn_bytes=3u;
    CHECK(!BootLifecycle_Confirm(BOOT_SLOT_ID_B));fail_program=-1;
    CHECK(BootMetadata_Load(BOOT_SLOT_ID_B,&r)==BOOT_METADATA_STATUS_VALID&&r.state==BOOT_IMAGE_STATE_TRIAL);
    CHECK(BootLifecycle_Init(BOOT_RESET_SOFTWARE)&&BootLifecycle_Confirm(BOOT_SLOT_ID_B));

    /* A torn INVALID commit recovers TRIAL, then the next boot finishes rollback. */
    reset_storage();CHECK(init_confirmed(BOOT_SLOT_ID_A,versions[0]));
    BootUpdate_Current.state=BOOT_UPDATE_PENDING_ACTIVATION;BootUpdate_Current.transaction_id=12;
    CHECK(BootLifecycle_Init(BOOT_RESET_SOFTWARE));
    CHECK(BootLifecycle_PrepareBoot(BOOT_SLOT_ID_B));CHECK(BootLifecycle_PrepareBoot(BOOT_SLOT_ID_B));CHECK(BootLifecycle_PrepareBoot(BOOT_SLOT_ID_B));
    fail_program=(int)(writes+7u);torn_bytes=2u;
    CHECK(!BootLifecycle_Init(BOOT_RESET_POWER_ON));fail_program=-1;
    CHECK(BootMetadata_Load(BOOT_SLOT_ID_B,&r)==BOOT_METADATA_STATUS_VALID&&r.state==BOOT_IMAGE_STATE_TRIAL);
    CHECK(BootLifecycle_Init(BOOT_RESET_POWER_ON));
    CHECK(BootMetadata_Load(BOOT_SLOT_ID_B,&r)==BOOT_METADATA_STATUS_VALID&&r.state==BOOT_IMAGE_STATE_INVALID);

    storage[0][0][0]^=1;storage[0][1][0]^=1;storage[1][0][0]^=1;storage[1][1][0]^=1;
    CHECK(!BootLifecycle_Init(BOOT_RESET_POWER_ON)&&BootLifecycle_StorageFault());
    puts("PASS Stage5 state transitions, CRC, dual copies, torn writes, wrap, trial/confirm/rollback policy");
    printf("PASS Stage5 production C: %u assertions\n",assertions);
    return 0;
}
