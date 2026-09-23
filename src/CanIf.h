#ifndef CAN_IF_H_
#define CAN_IF_H_

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"

#define CAN_FRAME_MAX_DLC  8u

typedef struct
{
    uint32_t id;
    uint8_t dlc;
    uint8_t data[CAN_FRAME_MAX_DLC];
} CanFrame_t;

extern QueueHandle_t g_canRxQueue;
extern QueueHandle_t g_canTxQueue;
extern volatile uint32_t CanIfRxDropCount;
extern volatile uint32_t CanIfTxDropCount;

void CanIf_Init(void);
uint8_t CanIf_IsInitialized(void);
uint8_t CanIf_Transmit(const CanFrame_t *frame);
uint8_t CanIf_TxMainFunction(void);
uint8_t CanIf_Read(CanFrame_t *frame);
uint8_t CanIf_ReadBlocking(CanFrame_t *frame, TickType_t wait_ticks);
void CanIf_RxIndication(const CanFrame_t *frame);

#endif /* CAN_IF_H_ */
