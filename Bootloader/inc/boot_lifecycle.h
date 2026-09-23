#ifndef BOOT_LIFECYCLE_H
#define BOOT_LIFECYCLE_H
#include "boot_metadata.h"
#include "boot_policy.h"
bool BootLifecycle_Init(BootResetReason reset_reason);
BootTargetType BootLifecycle_Select(const BootImageInfoType *slot_a,
                                    const BootImageInfoType *slot_b);
bool BootLifecycle_PrepareBoot(BootSlotIdType slot);
bool BootLifecycle_Confirm(BootSlotIdType slot);
bool BootLifecycle_IsSlotBootable(BootSlotIdType slot);
bool BootLifecycle_CanStartUpdate(BootSlotIdType active, BootSlotIdType target);
bool BootLifecycle_GetMetadata(BootSlotIdType slot, BootSlotMetadataType *record,
                               BootMetadataStatus *status);
bool BootLifecycle_StorageFault(void);
BootLifecycleResult BootLifecycle_GetLastResult(void);
BootSlotIdType BootLifecycle_GetFailedSlot(void);
BootSlotIdType BootLifecycle_GetRollbackTarget(void);
uint32_t BootLifecycle_GetFailedVersion(void);
uint32_t BootLifecycle_GetLastAttemptCount(void);
BootResetReason BootLifecycle_GetResetReason(void);
void BootLifecycle_WriteAppHandover(BootSlotIdType slot);
#endif
