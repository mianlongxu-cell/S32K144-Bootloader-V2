#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "CanIf.h"
#include "CanIf_Cfg.h"
#include "FlexCAN.h"

QueueHandle_t g_canRxQueue;
QueueHandle_t g_canTxQueue;
volatile uint32_t CanIfRxDropCount;
volatile uint32_t CanIfTxDropCount;
static volatile uint8_t CanIfInitialized;

void CanIf_Init(void)
{
    CanIfRxDropCount = 0u;
    CanIfTxDropCount = 0u;
    CanIfInitialized = 0u;
    FLEXCAN0_init(CanIfCfg_ControlRxCanId, CanIfCfg_DiagRxCanId);
    CanIfInitialized = 1u;
}

uint8_t CanIf_IsInitialized(void) { return CanIfInitialized; }

uint8_t CanIf_Transmit(const CanFrame_t *frame)//而是把报文放入发送队列
{
    if ((frame == (const CanFrame_t *)0) ||
        (frame->id > 0x7FFu) ||
        (frame->dlc > CAN_FRAME_MAX_DLC))
    {
        return 0u;
    }

    if ((g_canTxQueue == (QueueHandle_t)0) ||
        (xQueueSend(g_canTxQueue, frame, 0u) != pdPASS))//g_canTxQueue
    {
        CanIfTxDropCount++;
        return 0u;
    }

    return 1u;
}

uint8_t CanIf_TxMainFunction(void)//发送队列取出报文并提交给 FlexCAN 硬件
{
    CanFrame_t frame;

    if ((g_canTxQueue == (QueueHandle_t)0) ||
        (xQueuePeek(g_canTxQueue, &frame, 0u) != pdPASS))
    {
        return 0u;
    }

    if (FLEXCAN0_transmit_msg(frame.id, frame.dlc, frame.data) == 0u)
    {
        return 0u;
    }

    (void)xQueueReceive(g_canTxQueue, &frame, 0u);//发送成功后删除，Receive 删除队首
    return 1u;
}

uint8_t CanIf_Read(CanFrame_t *frame)
{
    if ((frame == (CanFrame_t *)0) ||
        (g_canRxQueue == (QueueHandle_t)0))
    {
        return 0u;
    }

    return (xQueueReceive(g_canRxQueue, frame, 0u) == pdPASS) ? 1u : 0u;//非阻塞读取
}

uint8_t CanIf_ReadBlocking(CanFrame_t *frame, TickType_t wait_ticks)//portMAX_DELAY阻塞读取
{
    if ((frame == (CanFrame_t *)0) ||
        (g_canRxQueue == (QueueHandle_t)0))
    {
        return 0u;
    }

    return (xQueueReceive(g_canRxQueue, frame, wait_ticks) == pdPASS) ?
           1u : 0u;
}

void CanIf_RxIndication(const CanFrame_t *frame)
{
    BaseType_t higher_priority_task_woken = pdFALSE;//定义任务唤醒变量，有更高级的任务会变成pdtrue

    if ((frame == (const CanFrame_t *)0) ||
        (g_canRxQueue == (QueueHandle_t)0))
    {
        CanIfRxDropCount++;
        return;
    }

    if (xQueueSendFromISR(g_canRxQueue,
                          frame,
                          &higher_priority_task_woken) != pdPASS)//将can中断的报文放入队列
    {
        CanIfRxDropCount++;
    }

    portYIELD_FROM_ISR(higher_priority_task_woken);//请求中断退出后切换任务
}
