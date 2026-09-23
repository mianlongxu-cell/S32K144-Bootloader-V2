#ifndef BOOT_UPDATE_H
#define BOOT_UPDATE_H
#include "boot_journal.h"
extern BootUpdateJournal BootUpdate_Current;
extern bool BootUpdate_JournalValid, BootUpdate_StorageFault;
bool BootUpdate_LoadRecoveryState(void);
bool BootUpdate_InitializeJournal(BootSlotIdType known_active);
bool BootUpdate_Begin(BootSlotIdType active, BootSlotIdType target, uint32_t size);
bool BootUpdate_TransitionAllowed(uint32_t from, uint32_t to);
bool BootUpdate_SetState(BootUpdateState state);
bool BootUpdate_SetProgress(uint32_t programmed, uint8_t sequence);
bool BootUpdate_MarkTransferComplete(const BootImageHeaderType *header, uint8_t sequence);
bool BootUpdate_MarkVerified(bool recovered);
bool BootUpdate_CompleteActivationHandoff(void);
bool BootUpdate_Abort(BootUpdateResult reason);
bool BootUpdate_IsSlotBootable(BootSlotIdType slot);
bool BootUpdate_CanContinue(void);
#endif
