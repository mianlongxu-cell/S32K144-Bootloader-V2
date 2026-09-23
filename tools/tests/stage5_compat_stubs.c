#include <string.h>
#include "boot_lifecycle.h"
bool BootLifecycle_Init(BootResetReason reason){(void)reason;return true;}
bool BootLifecycle_CanStartUpdate(BootSlotIdType active,BootSlotIdType target)
{return target<=BOOT_SLOT_ID_B&&target!=active;}
bool BootLifecycle_GetMetadata(BootSlotIdType slot,BootSlotMetadataType *record,BootMetadataStatus *status)
{if(slot>BOOT_SLOT_ID_B||record==0||status==0){return false;}memset(record,0,sizeof(*record));*status=BOOT_METADATA_STATUS_EMPTY;return true;}
bool BootLifecycle_StorageFault(void){return false;}
BootLifecycleResult BootLifecycle_GetLastResult(void){return BOOT_LIFECYCLE_NONE;}
BootSlotIdType BootLifecycle_GetFailedSlot(void){return BOOT_SLOT_ID_UNKNOWN;}
BootSlotIdType BootLifecycle_GetRollbackTarget(void){return BOOT_SLOT_ID_UNKNOWN;}
uint32_t BootLifecycle_GetFailedVersion(void){return 0u;}
uint32_t BootLifecycle_GetLastAttemptCount(void){return 0u;}
BootResetReason BootLifecycle_GetResetReason(void){return BOOT_RESET_UNKNOWN;}
