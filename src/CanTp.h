#ifndef CAN_TP_H_
#define CAN_TP_H_

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "CanIf.h"
#include "DiagTypes.h"

typedef enum
{
    CANTP_RX_IDLE = 0,
    CANTP_RX_WAIT_CF
} CanTpRxState_t;//表示接收端的状态

typedef enum
{
    CANTP_TX_IDLE = 0,
    CANTP_TX_WAIT_FC,
    CANTP_TX_SEND_CF
} CanTpTxState_t;

extern volatile uint32_t CanTpRxSfCount;//接收单帧数量
extern volatile uint32_t CanTpTxSfCount;//发送首帧数量
extern volatile uint32_t CanTpRxFfCount;
extern volatile uint32_t CanTpRxCfCount;
extern volatile uint32_t CanTpRxFcCount;
extern volatile uint32_t CanTpTxFfCount;
extern volatile uint32_t CanTpTxCfCount;
extern volatile uint32_t CanTpTxFcCount;
extern volatile uint32_t CanTpErrorCount;//错误和超时计数
extern volatile uint32_t CanTpSnErrorCount;
extern volatile uint32_t CanTpNBsTimeoutCount;
extern volatile uint32_t CanTpNCrTimeoutCount;
extern volatile uint32_t CanTpRxPduCount;
extern volatile uint16_t CanTpLastRxLength;
extern volatile uint8_t CanTpLastRxData[7];
extern volatile CanTpRxState_t CanTpRxState;
extern volatile CanTpTxState_t CanTpTxState;
extern volatile uint16_t CanTpRxOffset;
extern volatile uint16_t CanTpTxOffset;
extern QueueHandle_t g_diagRequestQueue;

void CanTp_Init(void);
void CanTp_RxIndication(const CanFrame_t *frame);
uint8_t CanTp_ReadRequest(DiagPdu_t *pdu);
uint8_t CanTp_Transmit(const DiagPdu_t *pdu);
void CanTp_MainFunction(void);

#endif /* CAN_TP_H_ */
