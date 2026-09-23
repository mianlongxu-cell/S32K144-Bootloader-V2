#ifndef BOOT_RECOVERY_POLICY_H
#define BOOT_RECOVERY_POLICY_H
#include "boot_journal.h"
typedef enum { BOOT_RECOVERY_FAIL_SAFE, BOOT_RECOVERY_NORMAL, BOOT_RECOVERY_ABORT,
               BOOT_RECOVERY_REVALIDATE, BOOT_RECOVERY_PENDING } BootRecoveryAction;
BootRecoveryAction BootRecoveryPolicy_Action(bool journal_valid, uint32_t state);
bool BootRecoveryPolicy_SlotAllowed(const BootUpdateJournal *record, BootSlotIdType slot);
#endif
