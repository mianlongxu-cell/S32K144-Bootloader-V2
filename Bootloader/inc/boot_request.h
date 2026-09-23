#ifndef BOOT_REQUEST_H_
#define BOOT_REQUEST_H_

#include <stdbool.h>
#include "boot_types.h"

void Boot_RequestInit(void);
bool Boot_IsBootRequested(void);
bool Boot_WasSoftwareReset(void);
bool Boot_IsSoftwareBootRequested(void);
bool Boot_IsApplicationConfirmRequested(void);
bool Boot_GetRequestedSlot(BootSlotIdType *slot);
void Boot_ClearSoftwareBootRequest(void);

#endif /* BOOT_REQUEST_H_ */
