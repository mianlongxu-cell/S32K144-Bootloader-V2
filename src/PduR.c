#include "FreeRTOS.h"
#include "queue.h"
#include "Com.h"
#include "CanTp.h"
#include "PduR.h"
#include "PduR_Cfg.h"

void PduR_Init(void)
{
}

void PduR_CanIfRxIndication(const CanFrame_t *frame)
{
    uint8_t index;

    if (frame == (const CanFrame_t *)0)
    {
        return;
    }

    for (index = 0u; index < PduRCanIfRxRouteCount; index++)
    {
        if (PduRCanIfRxRoutes[index].canId != frame->id)
        {
            continue;
        }

        if (PduRCanIfRxRoutes[index].destination == PDUR_DESTINATION_COM)
        {
            Com_RxIndication(frame);
        }
        else if (PduRCanIfRxRoutes[index].destination ==
                 PDUR_DESTINATION_CANTP)
        {
            CanTp_RxIndication(frame);
        }
        return;
    }
}

uint8_t PduR_ComTransmit(PduIdType pdu_id, const CanFrame_t *frame)
{
    if ((PduR_CfgGetTxRoute(pdu_id, PDUR_DESTINATION_CANIF) ==
         (const PduRTxRouteType *)0) ||
        (frame == (const CanFrame_t *)0))
    {
        return 0u;
    }

    return CanIf_Transmit(frame);
}

uint8_t PduR_CanTpTransmitCanFrame(PduIdType pdu_id,
                                    const CanFrame_t *frame)
{
    if ((PduR_CfgGetTxRoute(pdu_id, PDUR_DESTINATION_CANIF) ==
         (const PduRTxRouteType *)0) ||
        (frame == (const CanFrame_t *)0))
    {
        return 0u;
    }

    return CanIf_Transmit(frame);
}

uint8_t PduR_CanTpRxIndication(const DiagPdu_t *pdu)
{
    if ((pdu == (const DiagPdu_t *)0) ||
        (g_diagRequestQueue == (QueueHandle_t)0))
    {
        return 0u;
    }

    return (xQueueSend(g_diagRequestQueue, pdu, 0u) == pdPASS) ? 1u : 0u;
}

uint8_t PduR_DcmReadRequest(DiagPdu_t *pdu)
{
    if ((pdu == (DiagPdu_t *)0) ||
        (g_diagRequestQueue == (QueueHandle_t)0))
    {
        return 0u;
    }

    return (xQueueReceive(g_diagRequestQueue, pdu, 0u) == pdPASS) ? 1u : 0u;
}

uint8_t PduR_DcmTransmitResponse(const DiagPdu_t *pdu)
{
    if ((PduR_CfgGetTxRoute(PDUID_DIAG_RESPONSE,
                            PDUR_DESTINATION_CANTP) ==
         (const PduRTxRouteType *)0) ||
        (pdu == (const DiagPdu_t *)0))
    {
        return 0u;
    }

    return CanTp_Transmit(pdu);
}
