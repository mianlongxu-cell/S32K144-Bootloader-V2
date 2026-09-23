#ifndef COM_CFG_H_
#define COM_CFG_H_

#include <stdint.h>
#include "CanIf.h"
#include "PduTypes.h"

typedef void (*ComTxPackFncType)(CanFrame_t *frame);//定义函数指针类型，接收frame参数，没有返回值

typedef struct
{
    PduIdType pduId;
    uint32_t canId;
    uint16_t cycleMs;//发送周期
    ComTxPackFncType packFunction;//打包函数的地址
} ComTxPduConfigType;

extern const ComTxPduConfigType ComTxPduConfigs[];
extern const uint8_t ComTxPduConfigCount;
const ComTxPduConfigType *Com_CfgGetTxPdu(PduIdType pdu_id);

#endif /* COM_CFG_H_ */
