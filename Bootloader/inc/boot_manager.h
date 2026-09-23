#ifndef BOOT_MANAGER_H_
#define BOOT_MANAGER_H_

#include "boot_policy.h"
#include "boot_types.h"

void BootManager_Init(void);
void BootManager_Run(void);
BootTargetType BootManager_SelectBootTarget(void);
BootSlotIdType BootManager_GetActiveSlot(void);
uint32_t BootManager_GetActiveVersion(void);

/* Debugger-visible snapshot; no transport/logging overhead is added. */
extern BootImageInfoType BootManager_SlotAInfo;
extern BootImageInfoType BootManager_SlotBInfo;
extern volatile BootTargetType BootManager_SelectedTarget;

#endif /* BOOT_MANAGER_H_ */
