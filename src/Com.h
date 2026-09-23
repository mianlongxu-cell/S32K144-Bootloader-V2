#ifndef COM_H_
#define COM_H_

#include <stdint.h>
#include "CanIf.h"
#include "ControlCommand_Cfg.h"
#include "PduTypes.h"

extern volatile uint8_t ComCyclicTxEnabled;//周期发送开关

void Com_Init(void);
void Com_PackPowertrain(CanFrame_t *frame);//动力报文打包
void Com_PackBodyStatus(CanFrame_t *frame);
void Com_PackFaultStatus(CanFrame_t *frame);//故障报文打包
void Com_RxIndication(const CanFrame_t *frame);//接收并解释控制报文
uint8_t Com_IsCyclicTxEnabled(void);
void Com_SetCyclicTxEnabled(uint8_t enabled);
uint8_t Com_Transmit(PduIdType pdu_id);
uint16_t Com_GetTxCycleMs(PduIdType pdu_id);

#endif /* COM_H_ */
