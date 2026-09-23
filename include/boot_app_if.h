#ifndef BOOT_APP_IF_H_
#define BOOT_APP_IF_H_

#include <stdint.h>

typedef enum
{
    BOOT_APP_IF_OK = 0,
    BOOT_APP_IF_NOT_SUPPORTED,
    BOOT_APP_IF_NOT_IMPLEMENTED,
    BOOT_APP_IF_UNKNOWN,
    BOOT_APP_IF_INVALID_ARGUMENT
} BootAppIfResultType;

typedef enum
{
    BOOT_APP_SLOT_A = 0,
    BOOT_APP_SLOT_B = 1,
    BOOT_APP_SLOT_UNKNOWN = 0x7FFFFFFF
} BootAppSlotType;

typedef enum
{
    BOOT_APP_STATUS_UNKNOWN = 0,
    BOOT_APP_STATUS_NORMAL,
    BOOT_APP_STATUS_TRIAL
} BootAppStatusType;

BootAppIfResultType Boot_RequestReprogramming(void);
BootAppIfResultType Boot_GetRunningSlot(BootAppSlotType *slot);
BootAppIfResultType Boot_GetResetReason(uint32_t *reset_reason);
BootAppIfResultType Boot_GetBootStatus(BootAppStatusType *status);
BootAppIfResultType Boot_GetRunningVersion(uint32_t *version);
BootAppIfResultType Boot_ConfirmApplication(void);

/* Application scheduler hook backing the asynchronous reset request. */
void Boot_AppIfMainFunction(void);

#endif /* BOOT_APP_IF_H_ */
