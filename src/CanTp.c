#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "DiagConfig.h"
#include "CanTp.h"
#include "PduR.h"

#define CANTP_PCI_TYPE_MASK          0xF0u//ISO-TP 使用 CAN 数据第 0 字节的高 4 位表示帧类型
#define CANTP_PCI_SF                 0x00u
#define CANTP_PCI_FF                 0x10u
#define CANTP_PCI_CF                 0x20u//连续帧
#define CANTP_PCI_FC                 0x30u//流控帧

#define CANTP_PCI_LOW_NIBBLE_MASK    0x0Fu
#define CANTP_SF_MAX_PAYLOAD         7u//第 0 字节要存长度，所以只能携带 7 字节：
#define CANTP_FF_PAYLOAD_LENGTH      6u
#define CANTP_CF_MAX_PAYLOAD         7u//第 0 字节存 PCI 和序号，因此剩余 7 字节放数

#define CANTP_FC_STATUS_CTS          0u//Continue To Send，继续发送
#define CANTP_FC_STATUS_WAIT         1u//Continue To Send，继续发送
#define CANTP_FC_STATUS_OVERFLOW     2u//接收方缓存溢出，停止发送

volatile uint32_t CanTpRxSfCount;
volatile uint32_t CanTpTxSfCount;
volatile uint32_t CanTpRxFfCount;
volatile uint32_t CanTpRxCfCount;
volatile uint32_t CanTpRxFcCount;
volatile uint32_t CanTpTxFfCount;
volatile uint32_t CanTpTxCfCount;
volatile uint32_t CanTpTxFcCount;
volatile uint32_t CanTpErrorCount;
volatile uint32_t CanTpSnErrorCount;
volatile uint32_t CanTpNBsTimeoutCount;
volatile uint32_t CanTpNCrTimeoutCount;
volatile uint32_t CanTpRxPduCount;
volatile uint16_t CanTpLastRxLength;
volatile uint8_t CanTpLastRxData[CANTP_SF_MAX_PAYLOAD];
volatile CanTpRxState_t CanTpRxState;
volatile CanTpTxState_t CanTpTxState;
volatile uint16_t CanTpRxOffset;
volatile uint16_t CanTpTxOffset;

QueueHandle_t g_diagRequestQueue;

static DiagPdu_t CanTpRxPdu;
static DiagPdu_t CanTpTxPdu;
static uint8_t CanTpRxNextSn;
static uint8_t CanTpRxBlockCounter;
static TickType_t CanTpRxLastActivityTick;
static uint8_t CanTpTxNextSn;
static uint8_t CanTpTxBlockSize;
static uint8_t CanTpTxBlockCounter;
static uint8_t CanTpTxStminMs;
static TickType_t CanTpTxLastActivityTick;
static TickType_t CanTpTxLastCfTick;

static void CanTp_ClearFrame(CanFrame_t *frame)//用于初始化一帧诊断响应CAN帧：
{
    uint32_t i;

    frame->id = UDS_RESPONSE_ID;
    frame->dlc = CAN_FRAME_MAX_DLC;
    for (i = 0u; i < CAN_FRAME_MAX_DLC; i++)
    {
        frame->data[i] = 0u;
    }
}

static void CanTp_ResetRx(void)
{
    CanTpRxState = CANTP_RX_IDLE;
    CanTpRxOffset = 0u;
    CanTpRxPdu.length = 0u;
    CanTpRxNextSn = 1u;
    CanTpRxBlockCounter = 0u;
    CanTpRxLastActivityTick = 0u;
}

static void CanTp_ResetTx(void)
{
    CanTpTxState = CANTP_TX_IDLE;
    CanTpTxOffset = 0u;
    CanTpTxPdu.length = 0u;
    CanTpTxNextSn = 1u;
    CanTpTxBlockSize = 0u;
    CanTpTxBlockCounter = 0u;
    CanTpTxStminMs = 0u;
    CanTpTxLastActivityTick = 0u;
    CanTpTxLastCfTick = 0u;
}

static void CanTp_RecordCompletedRxPdu(const DiagPdu_t *pdu)
{
    uint32_t i;
    uint32_t bytes_to_record = pdu->length;

    if (bytes_to_record > CANTP_SF_MAX_PAYLOAD)
    {
        bytes_to_record = CANTP_SF_MAX_PAYLOAD;
    }

    CanTpLastRxLength = pdu->length;
    for (i = 0u; i < CANTP_SF_MAX_PAYLOAD; i++)
    {
        CanTpLastRxData[i] =
            (i < bytes_to_record) ? pdu->data[i] : 0u;
    }
}

static uint8_t CanTp_QueueRxPdu(const DiagPdu_t *pdu)//负责把完整PDU放入g_diagRequestQueue
{
    if ((g_diagRequestQueue == (QueueHandle_t)0) ||
        (pdu->length == 0u) ||
        (pdu->length > DIAG_MAX_PDU_LENGTH) ||
        (PduR_CanTpRxIndication(pdu) == 0u))//PDUR
    {
        CanTpErrorCount++;
        return 0u;
    }

    CanTp_RecordCompletedRxPdu(pdu);
    CanTpRxPduCount++;
    return 1u;
}

static uint8_t CanTp_SendFlowControl(uint8_t flow_status,
                                         uint8_t block_size,
                                         uint8_t stmin)//发送流控帧
{
    CanFrame_t frame;

    CanTp_ClearFrame(&frame);
    frame.data[0] = CANTP_PCI_FC |
                    (flow_status & CANTP_PCI_LOW_NIBBLE_MASK);
    frame.data[1] = block_size;
    frame.data[2] = stmin;

    if (PduR_CanTpTransmitCanFrame(PDUID_CANTP_CAN_TX, &frame) == 0u)//pduR
    {
        CanTpErrorCount++;
        return 0u;
    }

    CanTpTxFcCount++;
    return 1u;
}

static uint8_t CanTp_DecodeStmin(uint8_t stmin)//STmin 解码
{
    if (stmin <= 0x7Fu)
    {
        return stmin;
    }

    if ((stmin >= 0xF1u) && (stmin <= 0xF9u))
    {
        /* This learning implementation maps 100-900 us to 1 ms. */
        return 1u;
    }

    /* Reserved values are conservatively clamped to 127 ms. */
    CanTpErrorCount++;
    return 127u;
}

static void CanTp_HandleSingleFrame(const CanFrame_t *frame)
{
    DiagPdu_t request;
    uint8_t sf_length;
    uint32_t i;

    sf_length = frame->data[0] & CANTP_PCI_LOW_NIBBLE_MASK;//首先提取低 4 位长度
    if ((sf_length == 0u) ||
        (sf_length > CANTP_SF_MAX_PAYLOAD) ||
        (((uint32_t)sf_length + 1u) > frame->dlc))
    {
        CanTpErrorCount++;
        return;
    }

    if (CanTpRxState != CANTP_RX_IDLE)
    {
        CanTp_ResetRx();
        CanTpErrorCount++;
    }

    request.length = sf_length;
    for (i = 0u; i < sf_length; i++)
    {
        request.data[i] = frame->data[i + 1u];
    }

    if (CanTp_QueueRxPdu(&request) != 0u)
    {
        CanTpRxSfCount++;
    }
}

static void CanTp_HandleFirstFrame(const CanFrame_t *frame)
{
    uint16_t total_length;
    uint32_t i;

    if (frame->dlc != CAN_FRAME_MAX_DLC)
    {
        CanTpErrorCount++;
        CanTp_ResetRx();
        return;
    }

    total_length =
        ((uint16_t)(frame->data[0] & CANTP_PCI_LOW_NIBBLE_MASK) << 8u) |
        (uint16_t)frame->data[1];

    if (total_length <= CANTP_SF_MAX_PAYLOAD)
    {
        CanTpErrorCount++;
        CanTp_ResetRx();
        return;
    }

    if ((total_length > DIAG_MAX_PDU_LENGTH) ||
        (g_diagRequestQueue == (QueueHandle_t)0) ||
        (uxQueueSpacesAvailable(g_diagRequestQueue) == 0u))
    {
        (void)CanTp_SendFlowControl(CANTP_FC_STATUS_OVERFLOW,
                                        0u,
                                        0u);
        CanTpErrorCount++;
        CanTp_ResetRx();
        return;
    }

    CanTp_ResetRx();
    CanTpRxPdu.length = total_length;
    for (i = 0u; i < CANTP_FF_PAYLOAD_LENGTH; i++)
    {
        CanTpRxPdu.data[i] = frame->data[i + 2u];
    }

    CanTpRxOffset = CANTP_FF_PAYLOAD_LENGTH;
    CanTpRxNextSn = 1u;
    CanTpRxBlockCounter = 0u;
    CanTpRxLastActivityTick = xTaskGetTickCount();
    CanTpRxState = CANTP_RX_WAIT_CF;//等待连续帧状态

    if (CanTp_SendFlowControl(CANTP_FC_STATUS_CTS,
                                  CANTP_RX_BLOCK_SIZE,
                                  CANTP_RX_STMIN_MS) == 0u)//发送流控桢
    {
        CanTp_ResetRx();
        return;
    }

    CanTpRxFfCount++;
}

static void CanTp_HandleConsecutiveFrame(const CanFrame_t *frame)//接收连续帧
{
    uint8_t sequence_number;
    uint32_t available_bytes;
    uint32_t remaining_bytes;
    uint32_t bytes_to_copy;
    uint32_t i;

    if ((CanTpRxState != CANTP_RX_WAIT_CF) ||
        (frame->dlc <= 1u))
    {
        CanTpErrorCount++;
        return;
    }

    sequence_number = frame->data[0] & CANTP_PCI_LOW_NIBBLE_MASK;
    if (sequence_number != CanTpRxNextSn)//丢帧
    {
        CanTpSnErrorCount++;
        CanTpErrorCount++;
        CanTp_ResetRx();
        return;
    }

    available_bytes = (uint32_t)frame->dlc - 1u;
    remaining_bytes = (uint32_t)CanTpRxPdu.length -
                      CanTpRxOffset;//计算本帧实际要复制多少字节，避免最后一帧越界
    bytes_to_copy = (available_bytes < remaining_bytes) ?
                    available_bytes : remaining_bytes;

    for (i = 0u; i < bytes_to_copy; i++)
    {
        CanTpRxPdu.data[CanTpRxOffset + i] =
            frame->data[i + 1u];
    }

    CanTpRxOffset = (uint16_t)(CanTpRxOffset + bytes_to_copy);
    CanTpRxNextSn = (uint8_t)((CanTpRxNextSn + 1u) &
                                  CANTP_PCI_LOW_NIBBLE_MASK);
    CanTpRxLastActivityTick = xTaskGetTickCount();
    CanTpRxCfCount++;

    if (CanTpRxOffset >= CanTpRxPdu.length)
    {
        (void)CanTp_QueueRxPdu(&CanTpRxPdu);
        CanTp_ResetRx();
        return;
    }

#if CANTP_RX_BLOCK_SIZE != 0u
    CanTpRxBlockCounter++;
    if (CanTpRxBlockCounter >= CANTP_RX_BLOCK_SIZE)
    {
        CanTpRxBlockCounter = 0u;
        if (CanTp_SendFlowControl(CANTP_FC_STATUS_CTS,
                                     CANTP_RX_BLOCK_SIZE,
                                     CANTP_RX_STMIN_MS) == 0u)
        {
            CanTp_ResetRx();
        }
    }
#endif
}



static void CanTp_HandleFlowControl(const CanFrame_t *frame)//处理诊断仪发来的Flow Control
{
    uint8_t flow_status;
    const TickType_t now = xTaskGetTickCount();

    if ((frame->dlc < 3u) ||
        (CanTpTxState != CANTP_TX_WAIT_FC))
    {
        CanTpErrorCount++;
        return;
    }

    flow_status = frame->data[0] & CANTP_PCI_LOW_NIBBLE_MASK;//取出Flow Status：
    CanTpRxFcCount++;

    switch (flow_status)
    {
        case CANTP_FC_STATUS_CTS://CTS
            CanTpTxBlockSize = frame->data[1];
            CanTpTxBlockCounter = 0u;
            CanTpTxStminMs = CanTp_DecodeStmin(frame->data[2]);
            CanTpTxLastActivityTick = now;
            CanTpTxLastCfTick = now;
            CanTpTxState = CANTP_TX_SEND_CF;//表示允许ECU继续发送CF。
            break;

        case CANTP_FC_STATUS_WAIT://暂时等待，不改变发送状态
            CanTpTxLastActivityTick = now;
            break;

        case CANTP_FC_STATUS_OVERFLOW:
            CanTpErrorCount++;
            CanTp_ResetTx();
            break;

        default:
            CanTpErrorCount++;
            CanTp_ResetTx();
            break;
    }
}

static uint8_t CanTp_SendSingleFrame(const DiagPdu_t *pdu)//用于发送长度不超过7字节的诊断PDU。
{
    CanFrame_t frame;
    uint32_t i;

    CanTp_ClearFrame(&frame);
    frame.data[0] = (uint8_t)pdu->length;
    for (i = 0u; i < pdu->length; i++)
    {
        frame.data[i + 1u] = pdu->data[i];
    }

    if (PduR_CanTpTransmitCanFrame(PDUID_CANTP_CAN_TX, &frame) == 0u)
    {
        return 0u;
    }

    CanTpTxSfCount++;
    return 1u;
}

static uint8_t CanTp_StartMultiFrameTransmit(const DiagPdu_t *pdu)//启动多帧发送，不会一次把所有帧都发完
{
    CanFrame_t frame;
    uint32_t i;

    for (i = 0u; i < pdu->length; i++)
    {
        CanTpTxPdu.data[i] = pdu->data[i];//保存完整PDU
    }
    CanTpTxPdu.length = pdu->length;

    CanTp_ClearFrame(&frame);
    frame.data[0] = CANTP_PCI_FF |
                    (uint8_t)((pdu->length >> 8u) &
                              CANTP_PCI_LOW_NIBBLE_MASK);
    frame.data[1] = (uint8_t)(pdu->length & 0xFFu);  //FF格式：Byte 0：0x1 + PDU长度高4位 Byte 1：PDU长度低8位 Byte 2~7：前6字节有效数据
    for (i = 0u; i < CANTP_FF_PAYLOAD_LENGTH; i++)
    {
        frame.data[i + 2u] = CanTpTxPdu.data[i];
    }

    if (PduR_CanTpTransmitCanFrame(PDUID_CANTP_CAN_TX, &frame) == 0u)//会把CAN帧放入g_canTxQueue
    {
        CanTpTxPdu.length = 0u;
        return 0u;
    }

    CanTpTxOffset = CANTP_FF_PAYLOAD_LENGTH;
    CanTpTxNextSn = 1u;//下一帧CF的序号为1
    CanTpTxBlockSize = 0u;
    CanTpTxBlockCounter = 0u;
    CanTpTxStminMs = 0u;
    CanTpTxLastActivityTick = xTaskGetTickCount();
    CanTpTxState = CANTP_TX_WAIT_FC;
    CanTpTxFfCount++;
    return 1u;
}

static void CanTp_SendNextConsecutiveFrame(TickType_t now)//这个函数每次只发送一帧Consecutive Frame。
{
    CanFrame_t frame;
    uint32_t remaining_bytes;
    uint32_t bytes_to_copy;
    uint32_t i;

    remaining_bytes = (uint32_t)CanTpTxPdu.length -
                      CanTpTxOffset;
    bytes_to_copy = (remaining_bytes < CANTP_CF_MAX_PAYLOAD) ?
                    remaining_bytes : CANTP_CF_MAX_PAYLOAD;//计算本帧最多发送多少字节，一次最多发7字节

    CanTp_ClearFrame(&frame);
    frame.data[0] = CANTP_PCI_CF |
                    (CanTpTxNextSn & CANTP_PCI_LOW_NIBBLE_MASK);//生成CF控制字节 Byte 0：0x2 + 4位序号 Byte 1~7：最多7字节PDU数据
    for (i = 0u; i < bytes_to_copy; i++)
    {
        frame.data[i + 1u] =
            CanTpTxPdu.data[CanTpTxOffset + i];
    }

    if (PduR_CanTpTransmitCanFrame(PDUID_CANTP_CAN_TX, &frame) == 0u)
    {
        return;//提交CAN发送
    }

    CanTpTxOffset = (uint16_t)(CanTpTxOffset + bytes_to_copy);
    CanTpTxNextSn = (uint8_t)((CanTpTxNextSn + 1u) &
                                  CANTP_PCI_LOW_NIBBLE_MASK);
    CanTpTxBlockCounter++;
    CanTpTxLastCfTick = now;
    CanTpTxLastActivityTick = now;
    CanTpTxCfCount++;

    if (CanTpTxOffset >= CanTpTxPdu.length)
    {
        CanTp_ResetTx();//判断是否发送完成
        return;
    }

    if ((CanTpTxBlockSize != 0u) &&
        (CanTpTxBlockCounter >= CanTpTxBlockSize))//Block Size = 1表示ECU每发送1个CF，就必须重新等待下一条FC。
    {
        CanTpTxBlockCounter = 0u;
        CanTpTxLastActivityTick = now;
        CanTpTxState = CANTP_TX_WAIT_FC;
    }
}

void CanTp_Init(void)
{
    uint32_t i;

    CanTpRxSfCount = 0u;
    CanTpTxSfCount = 0u;
    CanTpRxFfCount = 0u;
    CanTpRxCfCount = 0u;
    CanTpRxFcCount = 0u;
    CanTpTxFfCount = 0u;
    CanTpTxCfCount = 0u;
    CanTpTxFcCount = 0u;
    CanTpErrorCount = 0u;
    CanTpSnErrorCount = 0u;
    CanTpNBsTimeoutCount = 0u;
    CanTpNCrTimeoutCount = 0u;
    CanTpRxPduCount = 0u;
    CanTpLastRxLength = 0u;

    for (i = 0u; i < CANTP_SF_MAX_PAYLOAD; i++)
    {
        CanTpLastRxData[i] = 0u;
    }

    CanTp_ResetRx();
    CanTp_ResetTx();
}

void CanTp_RxIndication(const CanFrame_t *frame)//这个函数处理收到的单帧CAN诊断报文,接收总入口
{
    uint8_t pci_type;

    if ((frame == (const CanFrame_t *)0) ||
        (frame->id != UDS_REQUEST_ID) ||
        (frame->dlc == 0u) ||
        (frame->dlc > CAN_FRAME_MAX_DLC))
    {
        CanTpErrorCount++;
        return;
    }

    pci_type = frame->data[0] & CANTP_PCI_TYPE_MASK;//识别ISO-TP帧类型，ISO-TP使用数据第一个字节的高4位表示帧类型：

    taskENTER_CRITICAL();
    switch (pci_type)
    {
        case CANTP_PCI_SF:
            CanTp_HandleSingleFrame(frame);
            break;

        case CANTP_PCI_FF:
            CanTp_HandleFirstFrame(frame);
            break;

        case CANTP_PCI_CF:
            CanTp_HandleConsecutiveFrame(frame);
            break;

        case CANTP_PCI_FC:
            CanTp_HandleFlowControl(frame);
            break;

        default:
            CanTpErrorCount++;
            break;
    }
    taskEXIT_CRITICAL();
}

uint8_t CanTp_ReadRequest(DiagPdu_t *pdu)//读取已经组装完成的完整诊断PDU
{
    return PduR_DcmReadRequest(pdu);
}

uint8_t CanTp_Transmit(const DiagPdu_t *pdu)//这个函数负责启动一条诊断响应的ISO-TP发送
{
    uint8_t result;

    if ((pdu == (const DiagPdu_t *)0) ||
        (pdu->length == 0u) ||
        (pdu->length > DIAG_MAX_PDU_LENGTH))
    {
        CanTpErrorCount++;
        return 0u;
    }

    taskENTER_CRITICAL();
    if (CanTpTxState != CANTP_TX_IDLE)
    {
        CanTpErrorCount++;
        taskEXIT_CRITICAL();
        return 0u;
    }

    if (pdu->length <= CANTP_SF_MAX_PAYLOAD)
    {
        result = CanTp_SendSingleFrame(pdu);//单帧发送
    }
    else
    {
        result = CanTp_StartMultiFrameTransmit(pdu);//多帧发送，而不是在这里一次性发送完
    }
    taskEXIT_CRITICAL();

    return result;
}

void CanTp_MainFunction(void)
{
    const TickType_t now = xTaskGetTickCount();

    taskENTER_CRITICAL();

    if ((CanTpRxState == CANTP_RX_WAIT_CF) &&
        ((TickType_t)(now - CanTpRxLastActivityTick) >=
         pdMS_TO_TICKS(CANTP_N_CR_TIMEOUT_MS)))//当前正在等待连续帧CF，并且距离上次接收数据已经超过N_Cr时间
    {
        CanTpNCrTimeoutCount++;//N_Cr超时次数加1
        CanTpErrorCount++;//总错误次数加1
        CanTp_ResetRx();//放弃当前PDU接收，清空接收状态
    }

    if ((CanTpTxState == CANTP_TX_WAIT_FC) &&
        ((TickType_t)(now - CanTpTxLastActivityTick) >=
         pdMS_TO_TICKS(CANTP_N_BS_TIMEOUT_MS)))//正在等待诊断仪回复Flow Control，但等待时间超过N_Bs
    {
        CanTpNBsTimeoutCount++;
        CanTpErrorCount++;
        CanTp_ResetTx();
    }

    if ((CanTpTxState == CANTP_TX_SEND_CF) &&
        ((TickType_t)(now - CanTpTxLastCfTick) >=
         pdMS_TO_TICKS(CanTpTxStminMs)))//. 按STmin发送连续帧
    {
        CanTp_SendNextConsecutiveFrame(now);
    }

    taskEXIT_CRITICAL();
}
