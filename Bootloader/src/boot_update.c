#include "boot_update.h"
#include "boot_recovery_policy.h"
#include "boot_slot.h"
#include "boot_image.h"
#include "boot_image_install.h"
#include "boot_fault.h"
BootUpdateJournal BootUpdate_Current;
bool BootUpdate_JournalValid, BootUpdate_StorageFault;
static bool target_revalidated;
static bool save(BootUpdateJournal *next)
{
    if (BootUpdate_StorageFault || !BootUpdate_JournalValid) { return false; }
    if (!BootJournal_Commit(next)) {
        BootUpdate_StorageFault = true;
        return false; /* Do not claim or execute a state change without durability. */
    }
    BootUpdate_Current = *next;
    return true;
}
bool BootUpdate_CanContinue(void) { return BootUpdate_JournalValid && !BootUpdate_StorageFault; }
bool BootUpdate_TransitionAllowed(uint32_t from, uint32_t to)
{
    if (from == BOOT_UPDATE_PENDING_ACTIVATION && to == BOOT_UPDATE_IDLE) { return true; }
    if (to == BOOT_UPDATE_PREPARING) {
        return from == BOOT_UPDATE_IDLE || from == BOOT_UPDATE_ABORTED || from == BOOT_UPDATE_PENDING_ACTIVATION;
    }
    if (to == BOOT_UPDATE_ABORTED) { return from >= BOOT_UPDATE_PREPARING && from <= BOOT_UPDATE_ABORTED; }
    return from >= BOOT_UPDATE_PREPARING && from < BOOT_UPDATE_PENDING_ACTIVATION && to == from + 1u;
}
bool BootUpdate_CompleteActivationHandoff(void)
{
    BootUpdateJournal next = BootUpdate_Current;
    if (!BootUpdate_CanContinue() || next.state != BOOT_UPDATE_PENDING_ACTIVATION) {
        return false;
    }
    next.state = BOOT_UPDATE_IDLE;
    return save(&next);
}
bool BootUpdate_SetState(BootUpdateState state)
{
    BootUpdateJournal next = BootUpdate_Current;
    if (!BootUpdate_TransitionAllowed(next.state, (uint32_t)state)) { return false; }
    next.state = (uint32_t)state;
    return save(&next);
}
bool BootUpdate_InitializeJournal(BootSlotIdType active)
{
    BootUpdateJournal existing, next = {0};
    BootImageInfoType info;
    if (BootJournal_LoadLatest(&existing) == BOOT_JOURNAL_VALID) { return false; }
    if (BootSlot_IsValidId(active)) {
        (void)BootImage_LoadInfo(active, &info);
        if (!info.valid) { return false; }
    } else {
        if (active != BOOT_SLOT_ID_UNKNOWN) { return false; }
        (void)BootImage_LoadInfo(BOOT_SLOT_ID_A, &info);
        if (info.valid) { return false; }
        (void)BootImage_LoadInfo(BOOT_SLOT_ID_B, &info);
        if (info.valid) { return false; }
    }
    next.active_slot = (uint32_t)active;
    next.target_slot = active == BOOT_SLOT_ID_A ? 1u : 0u;
    next.state = BOOT_UPDATE_ABORTED;
    next.last_result = BOOT_UPDATE_COMMISSIONED;
    if (!BootJournal_Initialize(&next)) { BootUpdate_StorageFault = true; return false; }
    BootUpdate_Current = next;
    BootUpdate_JournalValid = true; BootUpdate_StorageFault = false; target_revalidated = false;
    return true;
}
bool BootUpdate_Begin(BootSlotIdType active, BootSlotIdType target, uint32_t size)
{
    BootUpdateJournal next = {0};
    if (!BootUpdate_CanContinue() || !BootUpdate_TransitionAllowed(BootUpdate_Current.state, BOOT_UPDATE_PREPARING) ||
        !BootSlot_IsValidId(target) || target == active || size != BootSlot_GetSize(target) ||
        (BootSlot_IsValidId(active) && !BootUpdate_IsSlotBootable(active)) ||
        (active == BOOT_SLOT_ID_UNKNOWN && BootUpdate_Current.active_slot != (uint32_t)BOOT_SLOT_ID_UNKNOWN)) { return false; }
    next.transaction_id = BootUpdate_Current.transaction_id + 1u;
    next.active_slot = (uint32_t)active; next.target_slot = (uint32_t)target;
    next.state = BOOT_UPDATE_PREPARING; next.expected_size = size;
    if (!save(&next)) { return false; }
    target_revalidated = false;
    BootFault_Hit(BOOT_FAULT_AFTER_PREPARING);
    return true;
}
bool BootUpdate_SetProgress(uint32_t programmed, uint8_t sequence)
{
    BootUpdateJournal next = BootUpdate_Current;
    uint32_t durable = (programmed / BOOT_UPDATE_CHECKPOINT) * BOOT_UPDATE_CHECKPOINT;
    if (!BootUpdate_CanContinue() || next.state != BOOT_UPDATE_PROGRAMMING || programmed > next.expected_size) { return false; }
    /* Header is RAM-only until TransferExit. Never checkpoint it as durable. */
    if (durable >= next.expected_size) { durable -= BOOT_UPDATE_CHECKPOINT; }
    if (durable <= next.committed_size) { return true; }
    next.committed_size = durable; next.last_block_sequence = sequence;
    return save(&next);
}
bool BootUpdate_MarkTransferComplete(const BootImageHeaderType *header, uint8_t sequence)
{
    BootUpdateJournal next = BootUpdate_Current;
    if (!BootUpdate_CanContinue() || next.state != BOOT_UPDATE_PROGRAMMING || header == NULL) { return false; }
    next.cached_header = *header; next.flags = BOOT_UPDATE_HEADER_KNOWN;
    next.target_version = header->software_version; next.expected_crc = header->image_crc32;
    next.committed_size = next.expected_size; next.last_block_sequence = sequence;
    next.state = BOOT_UPDATE_TRANSFER_COMPLETE;
    return save(&next);
}
bool BootUpdate_MarkVerified(bool recovered)
{
    BootUpdateJournal next;
    if (BootUpdate_Current.state == BOOT_UPDATE_VERIFYING && !BootUpdate_SetState(BOOT_UPDATE_VERIFIED)) { return false; }
    if (BootUpdate_Current.state != BOOT_UPDATE_VERIFIED) { return false; }
    BootFault_Hit(BOOT_FAULT_AFTER_VERIFY);
    next = BootUpdate_Current; next.state = BOOT_UPDATE_PENDING_ACTIVATION;
    next.last_result = recovered ? BOOT_UPDATE_POWER_LOSS_RECOVERED : BOOT_UPDATE_SUCCESS;
    if (!save(&next)) { return false; }
    target_revalidated = true;
    return true;
}
bool BootUpdate_Abort(BootUpdateResult reason)
{
    BootUpdateJournal next = BootUpdate_Current;
    target_revalidated = false;
    if (!BootUpdate_TransitionAllowed(next.state, BOOT_UPDATE_ABORTED)) { return false; }
    next.state = BOOT_UPDATE_ABORTED; next.last_result = (uint32_t)reason;
    return save(&next);
}
bool BootUpdate_IsSlotBootable(BootSlotIdType slot)
{
    if (!BootUpdate_JournalValid || !BootRecoveryPolicy_SlotAllowed(&BootUpdate_Current, slot)) { return false; }
    if ((uint32_t)slot == BootUpdate_Current.target_slot && BootUpdate_Current.state != BOOT_UPDATE_IDLE) {
        return target_revalidated && !BootUpdate_StorageFault;
    }
    return true;
}
bool BootUpdate_LoadRecoveryState(void)
{
    BootRecoveryAction action;
    BootUpdate_Current = (BootUpdateJournal){0};
    BootUpdate_Current.active_slot = (uint32_t)BOOT_SLOT_ID_UNKNOWN;
    BootUpdate_Current.target_slot = (uint32_t)BOOT_SLOT_ID_UNKNOWN;
    BootUpdate_StorageFault = false; target_revalidated = false;
    BootUpdate_JournalValid = BootJournal_LoadLatest(&BootUpdate_Current) == BOOT_JOURNAL_VALID;
    action = BootRecoveryPolicy_Action(BootUpdate_JournalValid, BootUpdate_Current.state);
    if (action == BOOT_RECOVERY_FAIL_SAFE) { return false; }
    if (action == BOOT_RECOVERY_ABORT) {
        return BootUpdate_Abort(BootUpdate_Current.state == BOOT_UPDATE_PROGRAMMING ?
                                BOOT_UPDATE_INTERRUPTED_PROGRAMMING : BOOT_UPDATE_INTERRUPTED);
    }
    if (action == BOOT_RECOVERY_REVALIDATE || action == BOOT_RECOVERY_PENDING) {
        if (BootUpdate_Current.state == BOOT_UPDATE_TRANSFER_COMPLETE &&
            !BootUpdate_SetState(BOOT_UPDATE_VERIFYING)) { return false; }
        BootFault_Hit(BOOT_FAULT_DURING_VERIFY);
        if (!BootImageInstall_ValidateAndPublish((BootSlotIdType)BootUpdate_Current.active_slot,
              (BootSlotIdType)BootUpdate_Current.target_slot, &BootUpdate_Current.cached_header)) {
            (void)BootUpdate_Abort(BOOT_UPDATE_VERIFY_FAILED); return false;
        }
        if (action == BOOT_RECOVERY_PENDING) { target_revalidated = true; return true; }
        return BootUpdate_MarkVerified(true);
    }
    return true;
}
