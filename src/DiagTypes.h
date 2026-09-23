#ifndef DIAG_TYPES_H_
#define DIAG_TYPES_H_

#include <stdint.h>
#include "DiagConfig.h"

typedef struct
{
    uint16_t length;
    uint8_t data[DIAG_MAX_PDU_LENGTH];
} DiagPdu_t;

#endif /* DIAG_TYPES_H_ */
