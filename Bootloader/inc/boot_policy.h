#ifndef BOOT_POLICY_H_
#define BOOT_POLICY_H_

#include "boot_types.h"

typedef enum
{
    BOOT_TARGET_PROGRAMMING = 0,
    BOOT_TARGET_SLOT_A,
    BOOT_TARGET_SLOT_B,
    BOOT_TARGET_NONE
} BootTargetType;

BootTargetType BootPolicy_Select(const BootImageInfoType *slot_a,
                                 const BootImageInfoType *slot_b);
BootTargetType BootPolicy_SelectLifecycle(const BootImageInfoType *slot_a,
                                          BootImageState state_a,
                                          const BootImageInfoType *slot_b,
                                          BootImageState state_b);

#endif /* BOOT_POLICY_H_ */
