#ifndef BOOT_ISOTP_H_
#define BOOT_ISOTP_H_

#include <stdbool.h>
#include <stdint.h>
#include "boot_protocol_cfg.h"

typedef struct
{
    uint16_t length;
    uint8_t data[BOOT_ISOTP_MAX_PDU_LENGTH];
} BootIsoTp_PduType;

void BootIsoTp_Init(void);
bool BootIsoTp_MainFunction(BootIsoTp_PduType *request);
bool BootIsoTp_Transmit(const BootIsoTp_PduType *response);
void BootIsoTp_Abort(void);

#endif /* BOOT_ISOTP_H_ */
