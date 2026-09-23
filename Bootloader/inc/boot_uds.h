#ifndef BOOT_UDS_H_
#define BOOT_UDS_H_

#include <stdbool.h>
#include "boot_isotp.h"

void BootUds_Init(void);
void BootUds_MainFunction(void);
bool BootUds_ProcessRequest(const BootIsoTp_PduType *request,
                            BootIsoTp_PduType *response);
bool BootUds_IsResetPending(void);
bool BootUds_IsProgrammingActive(void);
void BootUds_AbortDownload(void);

#endif /* BOOT_UDS_H_ */
