#include "boot_recovery_policy.h"
BootRecoveryAction BootRecoveryPolicy_Action(bool valid, uint32_t state)
{
    if (!valid) { return BOOT_RECOVERY_FAIL_SAFE; }
    switch (state) {
    case BOOT_UPDATE_IDLE: case BOOT_UPDATE_ABORTED: return BOOT_RECOVERY_NORMAL;
    case BOOT_UPDATE_PREPARING: case BOOT_UPDATE_ERASING:
    case BOOT_UPDATE_DOWNLOAD_READY: case BOOT_UPDATE_PROGRAMMING: return BOOT_RECOVERY_ABORT;
    case BOOT_UPDATE_TRANSFER_COMPLETE: case BOOT_UPDATE_VERIFYING:
    case BOOT_UPDATE_VERIFIED: return BOOT_RECOVERY_REVALIDATE;
    case BOOT_UPDATE_PENDING_ACTIVATION: return BOOT_RECOVERY_PENDING;
    default: return BOOT_RECOVERY_FAIL_SAFE;
    }
}
bool BootRecoveryPolicy_SlotAllowed(const BootUpdateJournal *r, BootSlotIdType slot)
{
    if ((uint32_t)slot > 1u) { return false; }
    if (r->state == BOOT_UPDATE_IDLE) { return true; }
    if ((uint32_t)slot == r->active_slot) { return true; }
    return (uint32_t)slot == r->target_slot && r->state == BOOT_UPDATE_PENDING_ACTIVATION;
}
