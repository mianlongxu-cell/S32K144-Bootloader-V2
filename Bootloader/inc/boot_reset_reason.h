#ifndef BOOT_RESET_REASON_H
#define BOOT_RESET_REASON_H
#include <stdint.h>
#include "boot_metadata.h"
BootResetReason BootResetReason_Capture(void);
BootResetReason BootResetReason_Get(void);
void BootResetReason_PreserveForNormalization(BootResetReason reason);
#endif
