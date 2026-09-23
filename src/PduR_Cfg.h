#ifndef PDUR_CFG_H_
#define PDUR_CFG_H_

#include <stdint.h>
#include "PduTypes.h"

typedef enum
{
    PDUR_DESTINATION_COM = 0,
    PDUR_DESTINATION_CANTP,
    PDUR_DESTINATION_DCM,
    PDUR_DESTINATION_CANIF
} PduRDestinationType;

typedef struct
{
    PduIdType pduId;
    uint32_t canId;
    PduRDestinationType destination;
} PduRCanIfRxRouteType;

typedef struct
{
    PduIdType sourcePduId;
    PduRDestinationType destination;
    PduIdType destinationPduId;
} PduRTxRouteType;

extern const PduRCanIfRxRouteType PduRCanIfRxRoutes[];
extern const uint8_t PduRCanIfRxRouteCount;
extern const PduRTxRouteType PduRTxRoutes[];
extern const uint8_t PduRTxRouteCount;
const PduRTxRouteType *PduR_CfgGetTxRoute(PduIdType source_pdu_id,
                                          PduRDestinationType destination);

#endif /* PDUR_CFG_H_ */
