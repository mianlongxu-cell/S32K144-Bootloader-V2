#include "PduR_Cfg.h"
#include "CanIf_Cfg.h"

const PduRCanIfRxRouteType PduRCanIfRxRoutes[] =
{
    {PDUID_COM_CONTROL_RX, CANIF_CFG_CONTROL_RX_CAN_ID, PDUR_DESTINATION_COM},
    {PDUID_DIAG_REQUEST, CANIF_CFG_DIAG_RX_CAN_ID, PDUR_DESTINATION_CANTP}
};

const uint8_t PduRCanIfRxRouteCount =
    (uint8_t)(sizeof(PduRCanIfRxRoutes) / sizeof(PduRCanIfRxRoutes[0]));

const PduRTxRouteType PduRTxRoutes[] =
{
    {PDUID_COM_POWERTRAIN_TX, PDUR_DESTINATION_CANIF,
     PDUID_COM_POWERTRAIN_TX},
    {PDUID_COM_BODY_TX, PDUR_DESTINATION_CANIF, PDUID_COM_BODY_TX},
    {PDUID_COM_FAULT_TX, PDUR_DESTINATION_CANIF, PDUID_COM_FAULT_TX},
    {PDUID_DIAG_RESPONSE, PDUR_DESTINATION_CANTP, PDUID_DIAG_RESPONSE},
    {PDUID_CANTP_CAN_TX, PDUR_DESTINATION_CANIF, PDUID_CANTP_CAN_TX}
};

const uint8_t PduRTxRouteCount =
    (uint8_t)(sizeof(PduRTxRoutes) / sizeof(PduRTxRoutes[0]));

const PduRTxRouteType *PduR_CfgGetTxRoute(
    PduIdType source_pdu_id,
    PduRDestinationType destination)
{
    uint8_t index;

    for (index = 0u; index < PduRTxRouteCount; index++)
    {
        if ((PduRTxRoutes[index].sourcePduId == source_pdu_id) &&
            (PduRTxRoutes[index].destination == destination))
        {
            return &PduRTxRoutes[index];
        }
    }

    return (const PduRTxRouteType *)0;
}
