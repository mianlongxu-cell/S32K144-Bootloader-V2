#include "device_registers.h"
#include "boot_app_if.h"
#include "AppBootRequest.h"
#include "AppVersion.h"
#include "boot_memory_contract.h"

BootAppIfResultType Boot_RequestReprogramming(void)
{
    AppBootRequest_RequestProgrammingReset();
    return BOOT_APP_IF_OK;
}

BootAppIfResultType Boot_GetRunningSlot(BootAppSlotType *slot)
{
    if (slot == (BootAppSlotType *)0)
    {
        return BOOT_APP_IF_INVALID_ARGUMENT;
    }
    /* Same source, explicit build identity rather than address guessing. */
#if defined(APP_BUILD_SLOT) && (APP_BUILD_SLOT == 1)
    *slot = BOOT_APP_SLOT_B;
#else
    *slot = BOOT_APP_SLOT_A;
#endif
    return BOOT_APP_IF_OK;
}

BootAppIfResultType Boot_GetResetReason(uint32_t *reset_reason)
{
    if (reset_reason == (uint32_t *)0)
    {
        return BOOT_APP_IF_INVALID_ARGUMENT;
    }
    *reset_reason = IP_RCM->SRS;
    return BOOT_APP_IF_OK;
}

BootAppIfResultType Boot_GetBootStatus(BootAppStatusType *status)
{
    const volatile uint32_t *record=(const volatile uint32_t*)BOOT_CONTRACT_REQUEST_ADDRESS;
    uint32_t state;
    if (status == (BootAppStatusType *)0)
    {
        return BOOT_APP_IF_INVALID_ARGUMENT;
    }
    if(record[0]!=BOOT_CONTRACT_STATUS_MAGIC||record[1]!=~BOOT_CONTRACT_STATUS_MAGIC||
       record[2]!=~record[3]){*status=BOOT_APP_STATUS_UNKNOWN;return BOOT_APP_IF_UNKNOWN;}
    state=(record[2]&BOOT_CONTRACT_STATUS_STATE_MASK)>>BOOT_CONTRACT_STATUS_STATE_SHIFT;
    *status=state==4u?BOOT_APP_STATUS_TRIAL:(state==5u?BOOT_APP_STATUS_NORMAL:BOOT_APP_STATUS_UNKNOWN);
    return *status==BOOT_APP_STATUS_UNKNOWN?BOOT_APP_IF_UNKNOWN:BOOT_APP_IF_OK;
}

BootAppIfResultType Boot_GetRunningVersion(uint32_t *version)
{
    BootAppSlotType slot;
    const volatile uint32_t *header;
    if (version == (uint32_t *)0)
    {
        return BOOT_APP_IF_INVALID_ARGUMENT;
    }
    (void)Boot_GetRunningSlot(&slot);
    header = (const volatile uint32_t *)(slot == BOOT_APP_SLOT_B ?
             BOOT_CONTRACT_SLOT_B_HEADER : BOOT_CONTRACT_SLOT_A_HEADER);
    *version = header[0] == BOOT_CONTRACT_IMAGE_MAGIC && header[1] == 1u ?
               header[4] : APP_FIRMWARE_VERSION;
    return BOOT_APP_IF_OK;
}

BootAppIfResultType Boot_ConfirmApplication(void)
{
    BootAppStatusType status;
    if(Boot_GetBootStatus(&status)!=BOOT_APP_IF_OK){return BOOT_APP_IF_UNKNOWN;}
    if(status==BOOT_APP_STATUS_NORMAL){return BOOT_APP_IF_OK;}
    if(status!=BOOT_APP_STATUS_TRIAL){return BOOT_APP_IF_UNKNOWN;}
    AppBootRequest_RequestConfirmReset();
    return BOOT_APP_IF_OK;
}

void Boot_AppIfMainFunction(void)
{
    AppBootRequest_MainFunction();
}
