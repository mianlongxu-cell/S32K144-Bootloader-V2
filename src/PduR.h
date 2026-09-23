#ifndef PDUR_H_
#define PDUR_H_

#include <stdint.h>
#include "CanIf.h"
#include "DiagTypes.h"
#include "PduTypes.h"

void PduR_Init(void);
void PduR_CanIfRxIndication(const CanFrame_t *frame);
uint8_t PduR_ComTransmit(PduIdType pdu_id, const CanFrame_t *frame);
uint8_t PduR_CanTpTransmitCanFrame(PduIdType pdu_id,
                                    const CanFrame_t *frame);
uint8_t PduR_CanTpRxIndication(const DiagPdu_t *pdu);
uint8_t PduR_DcmReadRequest(DiagPdu_t *pdu);
uint8_t PduR_DcmTransmitResponse(const DiagPdu_t *pdu);

#endif /* PDUR_H_ */
