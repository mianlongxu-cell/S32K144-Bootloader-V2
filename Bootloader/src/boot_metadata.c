#include "boot_metadata.h"
#include "boot_metadata_storage.h"
#include "boot_crc32.h"
#include "boot_config.h"
#include "boot_fault.h"
static int32_t latest_copy[2] = {-1, -1};
static bool records_equal(const BootSlotMetadataType *a,const BootSlotMetadataType *b)
{ uint32_t i; for(i=0u;i<sizeof(*a);i++){if(((const uint8_t*)a)[i]!=((const uint8_t*)b)[i]){return false;}} return true; }
bool BootMetadata_SequenceNewer(uint32_t a,uint32_t b)
{ const uint32_t d=a-b; return d!=0u && d<0x80000000u; }
static bool fields_valid(const BootSlotMetadataType *r)
{
    if((r->state==BOOT_IMAGE_STATE_PENDING||r->state==BOOT_IMAGE_STATE_CONFIRMED||
        r->state==BOOT_IMAGE_STATE_VERIFIED)&&r->boot_attempts!=0u){return false;}
    if(r->state==BOOT_IMAGE_STATE_TRIAL&&(r->boot_attempts==0u||
       r->boot_attempts>BOOT_MAX_TRIAL_ATTEMPTS)){return false;}
    return true;
}
bool BootMetadata_Validate(const BootSlotMetadataType *r,BootSlotIdType slot)
{
    return r!=0&&(uint32_t)slot<=1u&&r->magic==BOOT_SLOT_METADATA_MAGIC&&
      r->format_version==BOOT_SLOT_METADATA_FORMAT&&r->slot_id==(uint32_t)slot&&
      r->state<=BOOT_IMAGE_STATE_INVALID&&r->boot_attempts<=BOOT_MAX_TRIAL_ATTEMPTS&&
      r->last_reset_reason<=BOOT_RESET_DEBUG&&r->last_result<=BOOT_LIFECYCLE_METADATA_ERROR&&
      (r->failed_slot<=1u||r->failed_slot==(uint32_t)BOOT_SLOT_ID_UNKNOWN)&&
      (r->rollback_target<=1u||r->rollback_target==(uint32_t)BOOT_SLOT_ID_UNKNOWN)&&
      r->flags==0u&&r->reserved[0]==0u&&r->reserved[1]==0u&&
      r->commit_marker==BOOT_SLOT_METADATA_COMMIT_MARKER&&fields_valid(r)&&
      r->metadata_crc32==BootCrc32_Calculate((const uint8_t*)r,
                          offsetof(BootSlotMetadataType,metadata_crc32));
}
BootMetadataStatus BootMetadata_Load(BootSlotIdType slot,BootSlotMetadataType *record)
{
    BootSlotMetadataType c[2]; bool valid[2],all_erased=true; uint32_t i,j;
    if(record==0||(uint32_t)slot>1u){return BOOT_METADATA_STATUS_INVALID;}
    latest_copy[slot]=-1;
    for(i=0u;i<2u;i++){
        if(!BootMetadataStorage_Read(slot,i,(uint8_t*)&c[i],sizeof(c[i]))){return BOOT_METADATA_STATUS_INVALID;}
        valid[i]=BootMetadata_Validate(&c[i],slot);
        for(j=0u;j<sizeof(c[i]);j++){if(((const uint8_t*)&c[i])[j]!=0xFFu){all_erased=false;}}
    }
    if(valid[0]&&valid[1]){
        if(BootMetadata_SequenceNewer(c[0].sequence,c[1].sequence)){latest_copy[slot]=0;}
        else if(BootMetadata_SequenceNewer(c[1].sequence,c[0].sequence)){latest_copy[slot]=1;}
        else if(records_equal(&c[0],&c[1])){latest_copy[slot]=0;}
        else{return BOOT_METADATA_STATUS_INVALID;}
    }else if(valid[0]){latest_copy[slot]=0;}else if(valid[1]){latest_copy[slot]=1;}
    else{return all_erased?BOOT_METADATA_STATUS_EMPTY:BOOT_METADATA_STATUS_INVALID;}
    *record=c[latest_copy[slot]]; return BOOT_METADATA_STATUS_VALID;
}
static bool transition_allowed(const BootSlotMetadataType *old,const BootSlotMetadataType *next)
{
    if(old->image_version!=next->image_version||old->update_counter!=next->update_counter){return false;}
    if(old->state==next->state){return old->state==BOOT_IMAGE_STATE_TRIAL&&next->boot_attempts==old->boot_attempts+1u;}
    if(old->state==BOOT_IMAGE_STATE_VERIFIED&&next->state==BOOT_IMAGE_STATE_PENDING){return next->boot_attempts==0u;}
    if(old->state==BOOT_IMAGE_STATE_PENDING&&next->state==BOOT_IMAGE_STATE_TRIAL){return next->boot_attempts==1u;}
    if(old->state==BOOT_IMAGE_STATE_TRIAL&&next->state==BOOT_IMAGE_STATE_CONFIRMED){return next->boot_attempts==0u;}
    if(old->state==BOOT_IMAGE_STATE_TRIAL&&next->state==BOOT_IMAGE_STATE_INVALID){return next->boot_attempts==old->boot_attempts;}
    return false;
}
static bool write_record(BootSlotIdType slot,BootSlotMetadataType *record,
                         BootMetadataStatus expected,bool programming)
{
    BootSlotMetadataType old,next,readback; BootMetadataStatus loaded; uint32_t copy,offset;
    if(record==0){return false;} next=*record; loaded=BootMetadata_Load(slot,&old);
    if(loaded!=expected){return false;}
    if(loaded==BOOT_METADATA_STATUS_VALID&&!programming&&!transition_allowed(&old,&next)){return false;}
    if(loaded!=BOOT_METADATA_STATUS_VALID&&!programming&&next.state!=BOOT_IMAGE_STATE_CONFIRMED){return false;}
    copy=loaded==BOOT_METADATA_STATUS_VALID?(uint32_t)(1-latest_copy[slot]):0u;
    next.magic=BOOT_SLOT_METADATA_MAGIC; next.format_version=BOOT_SLOT_METADATA_FORMAT;
    next.slot_id=(uint32_t)slot; next.sequence=loaded==BOOT_METADATA_STATUS_VALID?old.sequence+1u:1u;
    next.commit_marker=BOOT_SLOT_METADATA_COMMIT_MARKER;
    next.metadata_crc32=BootCrc32_Calculate((const uint8_t*)&next,offsetof(BootSlotMetadataType,metadata_crc32));
    if(!BootMetadata_Validate(&next,slot)||!BootMetadataStorage_Erase(slot,copy)){return false;}
    BootFault_Hit(BOOT_FAULT_DURING_METADATA_COMMIT);
    for(offset=0u;offset<sizeof(next);offset+=BOOT_FLASH_PHRASE_SIZE){
        if(!BootMetadataStorage_Program(slot,copy,offset,((const uint8_t*)&next)+offset,BOOT_FLASH_PHRASE_SIZE)){return false;}
        BootFault_Hit(BOOT_FAULT_DURING_METADATA_COMMIT);
    }
    if(!BootMetadataStorage_Read(slot,copy,(uint8_t*)&readback,sizeof(readback))||
       !BootMetadata_Validate(&readback,slot)||!records_equal(&next,&readback)){return false;}
    *record=readback; return true;
}
bool BootMetadata_Initialize(BootSlotIdType slot,BootSlotMetadataType *record)
{
    BootSlotMetadataType old; BootMetadataStatus status=BootMetadata_Load(slot,&old);
    return status!=BOOT_METADATA_STATUS_VALID&&write_record(slot,record,status,
           record!=0&&record->state==BOOT_IMAGE_STATE_VERIFIED);
}
bool BootMetadata_Commit(BootSlotIdType slot,BootSlotMetadataType *record)
{return write_record(slot,record,BOOT_METADATA_STATUS_VALID,false);}
bool BootMetadata_InstallVerified(BootSlotIdType slot,uint32_t version,uint32_t counter,uint32_t reason)
{
    BootSlotMetadataType old,next={0}; BootMetadataStatus status=BootMetadata_Load(slot,&old);
    if(status==BOOT_METADATA_STATUS_VALID&&old.state==BOOT_IMAGE_STATE_VERIFIED&&
       old.image_version==version&&old.update_counter==counter){return true;}
    next.image_version=version; next.state=BOOT_IMAGE_STATE_VERIFIED; next.update_counter=counter;
    next.last_reset_reason=reason; next.failed_slot=(uint32_t)BOOT_SLOT_ID_UNKNOWN;
    next.rollback_target=(uint32_t)BOOT_SLOT_ID_UNKNOWN;
    return write_record(slot,&next,status,true);
}
bool BootMetadata_SetState(BootSlotIdType slot,BootImageState state,uint32_t reason,uint32_t result)
{
    BootSlotMetadataType r;
    if(BootMetadata_Load(slot,&r)!=BOOT_METADATA_STATUS_VALID){return false;}
    if(r.state==(uint32_t)state){return true;}
    r.state=(uint32_t)state; r.last_reset_reason=reason; r.last_result=result;
    if(state==BOOT_IMAGE_STATE_TRIAL){r.boot_attempts=1u;r.last_attempt_count=1u;}
    else if(state==BOOT_IMAGE_STATE_CONFIRMED){r.boot_attempts=0u;r.successful_boots++;r.last_attempt_count=0u;}
    return BootMetadata_Commit(slot,&r);
}
bool BootMetadata_IncrementAttempt(BootSlotIdType slot,uint32_t reason)
{
    BootSlotMetadataType r;
    if(BootMetadata_Load(slot,&r)!=BOOT_METADATA_STATUS_VALID||r.state!=BOOT_IMAGE_STATE_TRIAL||r.boot_attempts>=BOOT_MAX_TRIAL_ATTEMPTS){return false;}
    r.boot_attempts++;r.last_attempt_count=r.boot_attempts;r.last_reset_reason=reason;r.last_result=BOOT_LIFECYCLE_TRIAL_STARTED;
    return BootMetadata_Commit(slot,&r);
}
BootImageState BootMetadata_GetState(BootSlotIdType slot)
{BootSlotMetadataType r;return BootMetadata_Load(slot,&r)==BOOT_METADATA_STATUS_VALID?(BootImageState)r.state:BOOT_IMAGE_STATE_EMPTY;}
