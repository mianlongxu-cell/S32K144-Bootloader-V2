#ifndef PDU_TYPES_H_
#define PDU_TYPES_H_

#include <stdint.h>

typedef uint16_t PduIdType;

typedef struct
{
    uint8_t *SduDataPtr;
    uint16_t SduLength;
} PduInfoType;

typedef enum
{
    PDUID_COM_POWERTRAIN_TX = 0u,
    PDUID_COM_BODY_TX,
    PDUID_COM_FAULT_TX,
    PDUID_COM_CONTROL_RX,
    PDUID_DIAG_REQUEST,
    PDUID_DIAG_RESPONSE,
    PDUID_CANTP_CAN_TX
} EcuPduIdType;

#endif /* PDU_TYPES_H_ */
