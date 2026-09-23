#include "Com.h"
#include "Com_Cfg.h"
#include "PduR.h"
#include "Rte.h"

volatile uint8_t ComCyclicTxEnabled;

static void Com_ClearFrameData(CanFrame_t *frame)
{
    uint32_t i;

    for (i = 0u; i < CAN_FRAME_MAX_DLC; i++)
    {
        frame->data[i] = 0u;
    }
}

void Com_Init(void)
{
    ComCyclicTxEnabled = 1u;//上电后默认开启周期发送：
}

void Com_PackPowertrain(CanFrame_t *frame)//小端格式
{
    RtePowertrainStatusType powertrain;
    uint16_t speed;
    uint16_t rpm;

    Rte_Read_PowertrainSnapshot(&powertrain);
    speed = powertrain.vehicleSpeed;
    rpm = powertrain.engineRpm;

    frame->dlc = CAN_FRAME_MAX_DLC;
    frame->data[0] = (uint8_t)(speed & 0xFFu);
    frame->data[1] = (uint8_t)(speed >> 8u);
    frame->data[2] = (uint8_t)(rpm & 0xFFu);
    frame->data[3] = (uint8_t)(rpm >> 8u);
    frame->data[4] = powertrain.coolantTemp;
    frame->data[5] = powertrain.gear;
    frame->data[6] = powertrain.powertrainStatus;
    frame->data[7] = 0u;
}

void Com_PackBodyStatus(CanFrame_t *frame)
{
    RteBodyStatusType body;

    Rte_Read_BodySnapshot(&body);
    frame->dlc = CAN_FRAME_MAX_DLC;
    frame->data[0] = body.lightStatus;
    frame->data[1] = body.doorStatus;
    frame->data[2] = body.wiperStatus;
    frame->data[3] = body.turnSignalStatus;
    frame->data[4] = body.lockStatus;
    frame->data[5] = 0u;
    frame->data[6] = 0u;
    frame->data[7] = 0u;
}

void Com_PackFaultStatus(CanFrame_t *frame)
{
    const DtcInfo_t primary_dtc = Rte_Read_PrimaryDTC();

    frame->dlc = CAN_FRAME_MAX_DLC;
    Com_ClearFrameData(frame);
    frame->data[0] = Rte_Read_HasActiveDTC();//是否有活动故障
    frame->data[1] = (uint8_t)(primary_dtc.code & 0xFFu);//DTC 低字节
    /* Legacy 0x102 layout carries the low 16 bits of the 24-bit DTC. */
    frame->data[2] = (uint8_t)((primary_dtc.code >> 8u) & 0xFFu);//DTC 高字节
    frame->data[3] = primary_dtc.level;//故障等级
    frame->data[4] = primary_dtc.occurrence_counter;//发生次数
    frame->data[5] = primary_dtc.status;//状态
}

void Com_RxIndication(const CanFrame_t *frame)
{
    if (frame == (const CanFrame_t *)0)
    {
        return;
    }

    if (frame->dlc == 0u)
    {
        return;
    }

    Rte_Write_ControlCommand(frame->data[0],
        (frame->dlc >= 2u) ? frame->data[1] : 0u);
}

uint8_t Com_Transmit(PduIdType pdu_id)
{
    CanFrame_t frame;
    const ComTxPduConfigType *const config = Com_CfgGetTxPdu(pdu_id);

    if (config == (const ComTxPduConfigType *)0)
    {
        return 0u;
    }

    if (config->packFunction == (ComTxPackFncType)0)
    {
        return 0u;
    }

    config->packFunction(&frame);//调用打包函数
    frame.id = config->canId;
    return PduR_ComTransmit(pdu_id, &frame);
}

uint16_t Com_GetTxCycleMs(PduIdType pdu_id)//根据PDU ID查询对应CAN发送报文的周期
{
    const ComTxPduConfigType *const config = Com_CfgGetTxPdu(pdu_id);//第一个const表示不能通过这个指针修改结构体内容，第二个表示指针不能重新指向别的地址

    return (config != (const ComTxPduConfigType *)0) ?
           config->cycleMs : 0u;//返回周期
}

uint8_t Com_IsCyclicTxEnabled(void)
{
    return ComCyclicTxEnabled;
}

void Com_SetCyclicTxEnabled(uint8_t enabled)
{
    ComCyclicTxEnabled = (enabled != 0u) ? 1u : 0u;
}
