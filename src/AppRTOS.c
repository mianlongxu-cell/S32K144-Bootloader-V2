#include "S32K144.h"
#include "FreeRTOS.h"//FreeRTOS基础类型和配置
#include "queue.h"//队列API
#include "task.h"//任务创建、延时、临界区等API
#include "AppRTOS.h"
#include "CanIf.h"// CAN接口与接收队列
#include "Com.h"//CAN应用协议
#include "DiagConfig.h"
#include "CanTp.h"
#include "Dcm.h"
#include "PduR.h"
#include "PduTypes.h"
#include "VehicleApp.h"
#include "AppHealth.h"

#define GREEN_LED_PIN            16u
//任务栈配置 S32K144是32位Cortex-M4，所以一个栈单元通常为4字节
#define APP_LED_STACK_WORDS      128u  //128个栈单元，单位StackType_t，512B字节
#define APP_VEHICLE_STACK_WORDS  160u
#define APP_DTC_STACK_WORDS      160u
#define APP_CAN_TX_STACK_WORDS   192u
#define APP_CAN_RX_STACK_WORDS   256u
#define APP_DIAG_STACK_WORDS     320u
#define APP_HEALTH_STACK_WORDS   128u
#define APP_CAN_RX_QUEUE_LENGTH  16u  //接收队列长度，最多保存16桢完整的CanFrame_t
#define APP_CAN_TX_QUEUE_LENGTH  24u
#define APP_DIAG_QUEUE_LENGTH    2u
//任务优先级，数字越大，优先级越高
#define APP_LED_PRIORITY         1u
#define APP_PERIODIC_PRIORITY    2u  //车辆/DTC/CAN-TX
#define APP_CAN_RX_PRIORITY      3u
#define APP_DIAG_PRIORITY        3u
#define APP_HEALTH_PRIORITY      1u

volatile uint32_t AppRTOSTxCount;  //统计成功提交给Flexcan发送MB的报文数量

static void Task_CanRx(void *parameters) //CAN接收任务
{
    CanFrame_t frame;

    (void)parameters;//当前没有使用传入参数

    for (;;)
    {
        if (CanIf_ReadBlocking(&frame, portMAX_DELAY) != 0u)//阻塞等待

        {
            PduR_CanIfRxIndication(&frame);
        }
    }
}

static void Task_Diag(void *parameters)//每1 ms运行一次，把已经收到的诊断请求交给Dcm处理，再把响应交回PduR发送。
{
    TickType_t last_wake_time = xTaskGetTickCount();
    DiagPdu_t request;//保存接收到的完整UDS请求
    DiagPdu_t response;//保存DCM生成的完整UDS响应

    (void)parameters;

    for (;;)
    {
        while (PduR_DcmReadRequest(&request) != 0u)//从诊断请求队列中取出一条完整PDU
        {
            if (Dcm_ProcessRequest(&request, &response) != 0u)//把完整请求交给DCM
            {
                (void)PduR_DcmTransmitResponse(&response);
            }
        }

        CanTp_MainFunction();//推进CanTp状态机，接收连续帧超时，等待流控制超时，等待流控制超时
        Dcm_MainFunction();//维护DCM会话状态
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1u));//保持1 ms周期
    }
}

static void Task_Vehicle(void *parameters)//车辆模型任务，100ms
{
    TickType_t last_wake_time = xTaskGetTickCount();//    记录任务开始运行时的FreeRTOS Tick

    (void)parameters;

    for (;;)
    {
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(100u));//阻塞到下一个100ms时间点
        taskENTER_CRITICAL();//进入临界区，不会发生相关任务切换
        Runnable_VehicleModel_100ms();
        taskEXIT_CRITICAL();
    }
}

static void Task_DTC(void *parameters)//故障检测任务,100 ms
{
    TickType_t last_wake_time = xTaskGetTickCount();

    (void)parameters;

    for (;;)
    {
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(100u));
        taskENTER_CRITICAL();
        Runnable_FaultMonitor_100ms();
        taskEXIT_CRITICAL();
    }
}

static void Task_CanTx(void *parameters)//CAN发送任务,1 ms
{
    TickType_t last_wake_time = xTaskGetTickCount();
    TickType_t last_powertrain_time = last_wake_time;
    TickType_t last_body_time = last_wake_time;
    TickType_t last_fault_time = last_wake_time;
    uint8_t powertrain_pending = 1u;
    uint8_t body_pending = 1u;
    uint8_t fault_pending = 1u;
    uint8_t previous_tx_enabled = Com_IsCyclicTxEnabled();//保存上一次循环的发送使能状态，用来检测“停止→启动”的变化。

    (void)parameters;

    for (;;)
    {
        const TickType_t now = xTaskGetTickCount();
        const uint8_t tx_enabled = Com_IsCyclicTxEnabled();//这个发送开关可以被CAN控制命令修改

        if (tx_enabled == 0u)
        {
            powertrain_pending = 0u;
            body_pending = 0u;
            fault_pending = 0u;
        }
        else
        {
            if (previous_tx_enabled == 0u)//重新启动
            {
                powertrain_pending = 1u;
                body_pending = 1u;
                fault_pending = 1u;
                last_powertrain_time = now;
                last_body_time = now;
                last_fault_time = now;
            }

            if ((TickType_t)(now - last_powertrain_time) >=
                pdMS_TO_TICKS(Com_GetTxCycleMs(PDUID_COM_POWERTRAIN_TX)))
            {
                last_powertrain_time += pdMS_TO_TICKS(
                    Com_GetTxCycleMs(PDUID_COM_POWERTRAIN_TX));
                powertrain_pending = 1u;//每100 ms把动力报文标记为待发送。
            }

            if ((TickType_t)(now - last_body_time) >=
                pdMS_TO_TICKS(Com_GetTxCycleMs(PDUID_COM_BODY_TX)))
            {
                last_body_time += pdMS_TO_TICKS(
                    Com_GetTxCycleMs(PDUID_COM_BODY_TX));
                body_pending = 1u;
            }

            if ((TickType_t)(now - last_fault_time) >=
                pdMS_TO_TICKS(Com_GetTxCycleMs(PDUID_COM_FAULT_TX)))
            {
                last_fault_time += pdMS_TO_TICKS(
                    Com_GetTxCycleMs(PDUID_COM_FAULT_TX));
                fault_pending = 1u;
            }

            if (powertrain_pending != 0u)//发送优先级
            {
                if (Com_Transmit(PDUID_COM_POWERTRAIN_TX) != 0u)//生成/打包某类应用报文→ 放入g_canTxQueue
                {
                    powertrain_pending = 0u;//清除待发送标志
                }
            }
            else if (body_pending != 0u)
            {
                if (Com_Transmit(PDUID_COM_BODY_TX) != 0u)
                {
                    body_pending = 0u;
                }
            }
            else if (fault_pending != 0u)
            {
                if (Com_Transmit(PDUID_COM_FAULT_TX) != 0u)
                {
                    fault_pending = 0u;
                }
            }
        }

        previous_tx_enabled = tx_enabled;

        /* Only this task is allowed to submit a frame to FlexCAN MB0. */
        if (CanIf_TxMainFunction() != 0u)
        {
            AppRTOSTxCount++;
        }
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1u));
    }
}

static void Task_LED(void *parameters)//运行指示灯任务
{
    TickType_t last_wake_time = xTaskGetTickCount();

    (void)parameters;

    for (;;)
    {
        IP_PTD->PTOR = (1u << GREEN_LED_PIN);
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(500u));//每500 ms翻转一次，所以完整的亮灭周期是1秒
    }
}

static void Task_AppHealth(void *parameters)
{
    TickType_t last=xTaskGetTickCount();(void)parameters;
    for(;;){vTaskDelayUntil(&last,pdMS_TO_TICKS(100u));AppHealth_RunTrialAction();}
}

uint8_t AppRTOS_Init(void)//
{
    BaseType_t result;

    AppRTOSTxCount = 0u;
    g_canRxQueue = xQueueCreate(APP_CAN_RX_QUEUE_LENGTH,
                                sizeof(CanFrame_t));//创建一个队列，队列长度，元素
    if (g_canRxQueue == (QueueHandle_t)0)
    {
        return 0u;
    }

    g_canTxQueue = xQueueCreate(APP_CAN_TX_QUEUE_LENGTH,
                                sizeof(CanFrame_t));
    if (g_canTxQueue == (QueueHandle_t)0)
    {
        return 0u;
    }

    g_diagRequestQueue = xQueueCreate(APP_DIAG_QUEUE_LENGTH,
                                      sizeof(DiagPdu_t));
    if (g_diagRequestQueue == (QueueHandle_t)0)
    {
        return 0u;
    }
//创建任务
    result = xTaskCreate(
        Task_CanRx,//任务函数
        "CAN-Rx",//任务名称
        APP_CAN_RX_STACK_WORDS,//栈大小
        (void *)0,//任务参数
        APP_CAN_RX_PRIORITY,//优先级
        (TaskHandle_t *)0);// 不保存任务句柄
    if (result != pdPASS)
    {
        return 0u;
    }

    result = xTaskCreate(
        Task_Diag,
        "Diag",
        APP_DIAG_STACK_WORDS,
        (void *)0,
        APP_DIAG_PRIORITY,
        (TaskHandle_t *)0);
    if (result != pdPASS)
    {
        return 0u;
    }

    result = xTaskCreate(
        Task_LED,
        "LED",
        APP_LED_STACK_WORDS,
        (void *)0,
        APP_LED_PRIORITY,
        (TaskHandle_t *)0);
    if (result != pdPASS)
    {
        return 0u;
    }

    result = xTaskCreate(
        Task_Vehicle,
        "Vehicle",
        APP_VEHICLE_STACK_WORDS,
        (void *)0,
        APP_PERIODIC_PRIORITY,
        (TaskHandle_t *)0);
    if (result != pdPASS)
    {
        return 0u;
    }

    result = xTaskCreate(
        Task_DTC,
        "DTC",
        APP_DTC_STACK_WORDS,
        (void *)0,
        APP_PERIODIC_PRIORITY,
        (TaskHandle_t *)0);
    if (result != pdPASS)
    {
        return 0u;
    }

    result = xTaskCreate(
        Task_CanTx,
        "CAN-Tx",
        APP_CAN_TX_STACK_WORDS,
        (void *)0,
        APP_PERIODIC_PRIORITY,
        (TaskHandle_t *)0);
    if (result != pdPASS) { return 0u; }
    result = xTaskCreate(Task_AppHealth,"AppHealth",APP_HEALTH_STACK_WORDS,
                         (void *)0,APP_HEALTH_PRIORITY,(TaskHandle_t *)0);

    return (result == pdPASS) ? 1u : 0u;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)//栈溢出
{
    (void)task;
    (void)task_name;
    taskDISABLE_INTERRUPTS();//如果创建任务、队列等操作申请FreeRTOS堆内存失败，就会进入这里
    AppHealth_ReportFatal();

    for (;;)
    {
    }
}

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    AppHealth_ReportFatal();

    for (;;)
    {
    }
}
