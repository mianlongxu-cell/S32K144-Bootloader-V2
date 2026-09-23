#include "device_registers.h"
#include "boot_memory_contract.h"
#include "boot_reset_reason.h"

static BootResetReason captured = BOOT_RESET_UNKNOWN;

static bool retained(uint32_t magic, uint32_t *data)
{
    volatile uint32_t *r = (volatile uint32_t *)BOOT_CONTRACT_REQUEST_ADDRESS;
    if (r[0] != magic || r[1] != ~magic || r[2] != ~r[3]) { return false; }
    if (data != 0) { *data = r[2]; }
    return true;
}

BootResetReason BootResetReason_Capture(void)
{
    uint32_t data;
    const uint32_t srs = IP_RCM->SRS;
    if (retained(BOOT_CONTRACT_FAULT_MAGIC, &data)) { captured = BOOT_RESET_FAULT; }
    else if (retained(BOOT_CONTRACT_NORMALIZE_MAGIC, &data) && data <= BOOT_RESET_DEBUG) {
        captured = (BootResetReason)data;
    }
    else if ((srs & RCM_SRS_WDOG_MASK) != 0u) { captured = BOOT_RESET_WATCHDOG; }
    else if ((srs & RCM_SRS_POR_MASK) != 0u) { captured = BOOT_RESET_POWER_ON; }
    else if ((srs & (RCM_SRS_LOCKUP_MASK | RCM_SRS_SACKERR_MASK)) != 0u) { captured = BOOT_RESET_FAULT; }
    else if ((srs & RCM_SRS_PIN_MASK) != 0u) { captured = BOOT_RESET_EXTERNAL; }
    else if ((srs & RCM_SRS_SW_MASK) != 0u) { captured = BOOT_RESET_SOFTWARE; }
    else if ((srs & RCM_SRS_JTAG_MASK) != 0u) { captured = BOOT_RESET_DEBUG; }
    else if ((srs & RCM_SRS_LVD_MASK) != 0u) { captured = BOOT_RESET_LOW_VOLTAGE; }
    else if ((srs & (RCM_SRS_LOC_MASK | RCM_SRS_LOL_MASK)) != 0u) { captured = BOOT_RESET_CLOCK; }
    else { captured = BOOT_RESET_UNKNOWN; }
    return captured;
}

BootResetReason BootResetReason_Get(void) { return captured; }

void BootResetReason_PreserveForNormalization(BootResetReason reason)
{
    volatile uint32_t *r = (volatile uint32_t *)BOOT_CONTRACT_REQUEST_ADDRESS;
    r[0] = BOOT_CONTRACT_NORMALIZE_MAGIC;
    r[1] = ~BOOT_CONTRACT_NORMALIZE_MAGIC;
    r[2] = (uint32_t)reason;
    r[3] = ~(uint32_t)reason;
    __asm volatile ("dsb\nisb" : : : "memory");
}
