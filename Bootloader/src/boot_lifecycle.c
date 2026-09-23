#include "boot_lifecycle.h"
#include "boot_config.h"
#include "boot_fault.h"
#include "boot_image.h"
#include "boot_memory_contract.h"
#include "boot_slot.h"
#include "boot_update.h"
#include "boot_version.h"

static BootSlotMetadataType metadata[2];
static BootMetadataStatus metadata_status[2];
static bool storage_fault;
static BootResetReason current_reset_reason;
static BootLifecycleResult last_result;
static BootSlotIdType failed_slot = BOOT_SLOT_ID_UNKNOWN;
static BootSlotIdType rollback_target = BOOT_SLOT_ID_UNKNOWN;
static uint32_t failed_version;
static uint32_t last_attempt_count;

static BootSlotMetadataType confirmed_record(BootSlotIdType slot,uint32_t version,uint32_t counter)
{
    BootSlotMetadataType r={0};
    r.slot_id=(uint32_t)slot;r.image_version=version;r.state=BOOT_IMAGE_STATE_CONFIRMED;
    r.update_counter=counter;r.last_reset_reason=(uint32_t)current_reset_reason;
    r.failed_slot=(uint32_t)BOOT_SLOT_ID_UNKNOWN;r.rollback_target=(uint32_t)BOOT_SLOT_ID_UNKNOWN;
    return r;
}
static bool reload(void)
{
    metadata_status[0]=BootMetadata_Load(BOOT_SLOT_ID_A,&metadata[0]);
    metadata_status[1]=BootMetadata_Load(BOOT_SLOT_ID_B,&metadata[1]);
    return true;
}
static bool image_valid(BootSlotIdType slot,BootImageInfoType *info)
{ return BootImage_LoadInfo(slot,info)==BOOT_IMAGE_VALID&&info->valid; }
static bool initialize_confirmed(BootSlotIdType slot,const BootImageInfoType *image,uint32_t counter)
{
    BootSlotMetadataType r=confirmed_record(slot,image->header.software_version,counter);
    if(!BootMetadata_Initialize(slot,&r)){storage_fault=true;return false;} return true;
}
static bool reconcile_journal(void)
{
    BootImageInfoType active_image,target_image;
    BootSlotIdType active,target;
    if(!BootUpdate_JournalValid||BootUpdate_StorageFault){storage_fault=true;return false;}
    if(BootUpdate_Current.state==BOOT_UPDATE_PENDING_ACTIVATION){
        active=(BootSlotIdType)BootUpdate_Current.active_slot;
        target=(BootSlotIdType)BootUpdate_Current.target_slot;
        if(!BootSlot_IsValidId(target)||!image_valid(target,&target_image)){return false;}
        reload();
        if(BootSlot_IsValidId(active)){
            if(!image_valid(active,&active_image)){return false;}
            if(metadata_status[active]==BOOT_METADATA_STATUS_EMPTY){
                if(!initialize_confirmed(active,&active_image,BootUpdate_Current.transaction_id)){return false;}
            }else if(metadata_status[active]!=BOOT_METADATA_STATUS_VALID||
                     metadata[active].state!=BOOT_IMAGE_STATE_CONFIRMED){storage_fault=true;return false;}
        }
        if(!(metadata_status[target]==BOOT_METADATA_STATUS_VALID&&
             metadata[target].state==BOOT_IMAGE_STATE_PENDING&&
             metadata[target].image_version==target_image.header.software_version&&
             metadata[target].update_counter==BootUpdate_Current.transaction_id)&&
           (!BootMetadata_InstallVerified(target,target_image.header.software_version,
                                          BootUpdate_Current.transaction_id,(uint32_t)current_reset_reason)||
            !BootMetadata_SetState(target,BOOT_IMAGE_STATE_PENDING,(uint32_t)current_reset_reason,
                                   BOOT_LIFECYCLE_NONE))){storage_fault=true;return false;}
        BootFault_Hit(BOOT_FAULT_AFTER_METADATA_PENDING);
        if(!BootUpdate_CompleteActivationHandoff()){storage_fault=true;return false;}
    }else if(BootUpdate_Current.state==BOOT_UPDATE_ABORTED&&
             BootUpdate_Current.last_result==BOOT_UPDATE_COMMISSIONED&&
             BootSlot_IsValidId((BootSlotIdType)BootUpdate_Current.active_slot)){
        active=(BootSlotIdType)BootUpdate_Current.active_slot; reload();
        if(metadata_status[active]==BOOT_METADATA_STATUS_EMPTY&&image_valid(active,&active_image)){
            if(!initialize_confirmed(active,&active_image,BootUpdate_Current.transaction_id)){return false;}
        }
    }
    return true;
}
static void capture_diagnostics(void)
{
    uint32_t i;int32_t selected=-1;
    last_result=BOOT_LIFECYCLE_NONE;failed_slot=BOOT_SLOT_ID_UNKNOWN;
    rollback_target=BOOT_SLOT_ID_UNKNOWN;failed_version=0u;last_attempt_count=0u;
    for(i=0u;i<2u;i++){
        if(metadata_status[i]==BOOT_METADATA_STATUS_VALID&&
           (selected<0||metadata[i].update_counter>metadata[selected].update_counter||
            (metadata[i].update_counter==metadata[selected].update_counter&&
             BootMetadata_SequenceNewer(metadata[i].sequence,metadata[selected].sequence)))){selected=(int32_t)i;}
    }
    if(selected>=0){
        last_result=(BootLifecycleResult)metadata[selected].last_result;
        failed_slot=(BootSlotIdType)metadata[selected].failed_slot;
        rollback_target=(BootSlotIdType)metadata[selected].rollback_target;
        failed_version=metadata[selected].failed_version;
        last_attempt_count=metadata[selected].last_attempt_count;
    }
}
static bool resolve_exhausted_trials(void)
{
    uint32_t i; BootSlotIdType fallback; BootImageInfoType fallback_image;
    for(i=0u;i<2u;i++){
        if(metadata_status[i]==BOOT_METADATA_STATUS_VALID&&metadata[i].state==BOOT_IMAGE_STATE_TRIAL&&
           metadata[i].boot_attempts>=BOOT_MAX_TRIAL_ATTEMPTS){
            fallback=(BootSlotIdType)(1u-i);
            if(metadata_status[fallback]!=BOOT_METADATA_STATUS_VALID||
               metadata[fallback].state!=BOOT_IMAGE_STATE_CONFIRMED||!image_valid(fallback,&fallback_image)){
                fallback=BOOT_SLOT_ID_UNKNOWN;
            }
            metadata[i].state=BOOT_IMAGE_STATE_INVALID;
            metadata[i].last_reset_reason=(uint32_t)current_reset_reason;
            metadata[i].last_result=fallback==BOOT_SLOT_ID_UNKNOWN?BOOT_LIFECYCLE_TRIAL_FAILED:
                                                           BOOT_LIFECYCLE_ROLLBACK_OCCURRED;
            metadata[i].failed_slot=i;metadata[i].failed_version=metadata[i].image_version;
            metadata[i].last_attempt_count=metadata[i].boot_attempts;
            metadata[i].rollback_target=(uint32_t)fallback;
            BootFault_Hit(BOOT_FAULT_DURING_METADATA_ROLLBACK);
            if(!BootMetadata_Commit((BootSlotIdType)i,&metadata[i])){storage_fault=true;return false;}
        }
    }
    return true;
}
bool BootLifecycle_Init(BootResetReason reason)
{
    current_reset_reason=reason;storage_fault=false;
    if(!reconcile_journal()){reload();capture_diagnostics();return false;}
    reload();
    if(metadata_status[0]==BOOT_METADATA_STATUS_INVALID&&metadata_status[1]==BOOT_METADATA_STATUS_INVALID){
        storage_fault=true;capture_diagnostics();return false;
    }
    if(!resolve_exhausted_trials()){reload();capture_diagnostics();return false;}
    reload();capture_diagnostics();return true;
}
static uint32_t priority(uint32_t state)
{ return state==BOOT_IMAGE_STATE_TRIAL?3u:(state==BOOT_IMAGE_STATE_PENDING?2u:(state==BOOT_IMAGE_STATE_CONFIRMED?1u:0u)); }
BootTargetType BootLifecycle_Select(const BootImageInfoType *a,const BootImageInfoType *b)
{
    BootImageState sa=BOOT_IMAGE_STATE_EMPTY,sb=BOOT_IMAGE_STATE_EMPTY;
    if(a==0||b==0||storage_fault){return BOOT_TARGET_PROGRAMMING;}
    if(metadata_status[0]==BOOT_METADATA_STATUS_VALID){sa=(BootImageState)metadata[0].state;}
    if(metadata_status[1]==BOOT_METADATA_STATUS_VALID){sb=(BootImageState)metadata[1].state;}
    if(BootPolicy_SelectLifecycle(a,sa,b,sb)==BOOT_TARGET_PROGRAMMING){last_result=BOOT_LIFECYCLE_NO_CONFIRMED_IMAGE;}
    return BootPolicy_SelectLifecycle(a,sa,b,sb);
}
bool BootLifecycle_PrepareBoot(BootSlotIdType slot)
{
    if(!BootSlot_IsValidId(slot)){return false;} reload();
    if(metadata_status[slot]!=BOOT_METADATA_STATUS_VALID){return false;}
    if(metadata[slot].state==BOOT_IMAGE_STATE_PENDING){
        if(!BootMetadata_SetState(slot,BOOT_IMAGE_STATE_TRIAL,(uint32_t)current_reset_reason,
                                  BOOT_LIFECYCLE_TRIAL_STARTED)){storage_fault=true;return false;}
        BootFault_Hit(BOOT_FAULT_AFTER_METADATA_TRIAL);
    }else if(metadata[slot].state==BOOT_IMAGE_STATE_TRIAL){
        if(!BootMetadata_IncrementAttempt(slot,(uint32_t)current_reset_reason)){storage_fault=true;return false;}
    }else if(metadata[slot].state!=BOOT_IMAGE_STATE_CONFIRMED){return false;}
    reload();capture_diagnostics();return true;
}
bool BootLifecycle_Confirm(BootSlotIdType slot)
{
    BootImageInfoType a,b;BootTargetType selected;
    if(!BootSlot_IsValidId(slot)){return false;} reload();
    if(metadata_status[slot]!=BOOT_METADATA_STATUS_VALID){return false;}
    if(metadata[slot].state==BOOT_IMAGE_STATE_CONFIRMED){return true;}
    if(metadata[slot].state!=BOOT_IMAGE_STATE_TRIAL){return false;}
    (void)BootImage_LoadInfo(BOOT_SLOT_ID_A,&a);(void)BootImage_LoadInfo(BOOT_SLOT_ID_B,&b);
    selected=BootLifecycle_Select(&a,&b);
    if((slot==BOOT_SLOT_ID_A&&selected!=BOOT_TARGET_SLOT_A)||
       (slot==BOOT_SLOT_ID_B&&selected!=BOOT_TARGET_SLOT_B)){return false;}
    BootFault_Hit(BOOT_FAULT_DURING_METADATA_CONFIRM);
    if(!BootMetadata_SetState(slot,BOOT_IMAGE_STATE_CONFIRMED,(uint32_t)current_reset_reason,
                              BOOT_LIFECYCLE_CONFIRMED)){storage_fault=true;return false;}
    reload();capture_diagnostics();return true;
}
bool BootLifecycle_IsSlotBootable(BootSlotIdType slot)
{ return BootSlot_IsValidId(slot)&&!storage_fault&&metadata_status[slot]==BOOT_METADATA_STATUS_VALID&&priority(metadata[slot].state)!=0u; }
bool BootLifecycle_CanStartUpdate(BootSlotIdType active,BootSlotIdType target)
{
    if(!BootSlot_IsValidId(target)||target==active||storage_fault){return false;}
    if(active==BOOT_SLOT_ID_UNKNOWN){return metadata_status[0]!=BOOT_METADATA_STATUS_VALID&&metadata_status[1]!=BOOT_METADATA_STATUS_VALID;}
    return metadata_status[active]==BOOT_METADATA_STATUS_VALID&&metadata[active].state==BOOT_IMAGE_STATE_CONFIRMED;
}
bool BootLifecycle_GetMetadata(BootSlotIdType slot,BootSlotMetadataType *r,BootMetadataStatus *s)
{if(!BootSlot_IsValidId(slot)||r==0||s==0){return false;}reload();*r=metadata[slot];*s=metadata_status[slot];return true;}
bool BootLifecycle_StorageFault(void){return storage_fault;}
BootLifecycleResult BootLifecycle_GetLastResult(void){return last_result;}
BootSlotIdType BootLifecycle_GetFailedSlot(void){return failed_slot;}
BootSlotIdType BootLifecycle_GetRollbackTarget(void){return rollback_target;}
uint32_t BootLifecycle_GetFailedVersion(void){return failed_version;}
uint32_t BootLifecycle_GetLastAttemptCount(void){return last_attempt_count;}
BootResetReason BootLifecycle_GetResetReason(void){return current_reset_reason;}
void BootLifecycle_WriteAppHandover(BootSlotIdType slot)
{
    volatile uint32_t *r=(volatile uint32_t*)BOOT_CONTRACT_REQUEST_ADDRESS;uint32_t data;
    reload(); data=(uint32_t)slot|((metadata[slot].state&0xFFu)<<BOOT_CONTRACT_STATUS_STATE_SHIFT)|
           (((uint32_t)current_reset_reason&0xFFu)<<BOOT_CONTRACT_STATUS_RESET_SHIFT);
    r[0]=BOOT_CONTRACT_STATUS_MAGIC;r[1]=~BOOT_CONTRACT_STATUS_MAGIC;r[2]=data;r[3]=~data;
#ifndef BOOT_HOST_TEST
    __asm volatile("dsb\nisb" : : : "memory");
#endif
}
