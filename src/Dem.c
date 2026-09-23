#include "FreeRTOS.h"
#include "task.h"
#include "Dem.h"

#define DEM_CONTROL_TIMEOUT_MS  5000u

typedef struct
{
    DtcInfo_t dtc;
    uint8_t failCounter;//连续失败次数
    uint8_t passCounter;//连续正常次数
} DemEventRuntimeType;

static DemEventRuntimeType DemEventRuntime[DEM_EVENT_CONFIG_COUNT];
static TickType_t DemLastControlHeartbeatTick;
static uint8_t DemControlTimeoutMonitoringEnabled;

static int8_t Dem_FindEventIndex(DemEventIdType event_id)
{
    uint8_t index;

    for (index = 0u; index < DemEventConfigCount; index++)
    {
        if (DemEventConfigs[index].eventId == event_id)
        {
            return (int8_t)index;
        }
    }

    return -1;
}

static void Dem_ReportFailed(uint8_t index)
{
    DemEventRuntimeType *const runtime = &DemEventRuntime[index];
    const DemEventConfigType *const config = &DemEventConfigs[index];

    runtime->passCounter = 0u;
    if (runtime->failCounter < config->failThreshold)
    {
        runtime->failCounter++;
    }

    runtime->dtc.level = config->level;
    if (runtime->failCounter < config->failThreshold)
    {
        runtime->dtc.status = DEM_DTC_STATUS_PENDING_DTC;
        return;
    }

    if (runtime->dtc.active == 0u)
    {
        if (runtime->dtc.occurrence_counter < 0xFFu)
        {
            runtime->dtc.occurrence_counter++;
        }
    }

    runtime->dtc.active = 1u;
    runtime->dtc.stored = 1u;
    runtime->dtc.status = DEM_DTC_STATUS_TEST_FAILED |
                          DEM_DTC_STATUS_PENDING_DTC |
                          DEM_DTC_STATUS_CONFIRMED_DTC;
}

static void Dem_ReportPassed(uint8_t index)//处理故障恢复
{
    DemEventRuntimeType *const runtime = &DemEventRuntime[index];
    const DemEventConfigType *const config = &DemEventConfigs[index];

    runtime->failCounter = 0u;
    if (runtime->passCounter < config->passThreshold)
    {
        runtime->passCounter++;
    }

    if (runtime->passCounter < config->passThreshold)
    {
        return;
    }

    runtime->dtc.active = 0u;
    if ((runtime->dtc.status & DEM_DTC_STATUS_CONFIRMED_DTC) != 0u)
    {
        runtime->dtc.stored = 1u;
        runtime->dtc.status = DEM_DTC_STATUS_CONFIRMED_DTC;//当前已恢复，但历史故障记录仍保留
    }
    else
    {
        runtime->dtc.stored = 0u;
        runtime->dtc.status = 0u;
        runtime->dtc.level = 0u;
    }
}

void Dem_Init(void)
{
    uint8_t index;

    for (index = 0u; index < DemEventConfigCount; index++)
    {
        DemEventRuntime[index].dtc.code = DemEventConfigs[index].dtc;
    }
    DemControlTimeoutMonitoringEnabled = 0u;
    DemLastControlHeartbeatTick = xTaskGetTickCount();
    Dem_ClearDTC();
}

void Dem_SetEventStatus(DemEventIdType event_id, DemEventStatusType status)
{
    const int8_t index = Dem_FindEventIndex(event_id);

    if (index < 0)
    {
        return;
    }

    taskENTER_CRITICAL();
    if (status == DEM_EVENT_FAILED)
    {
        Dem_ReportFailed((uint8_t)index);
    }
    else
    {
        Dem_ReportPassed((uint8_t)index);
    }
    taskEXIT_CRITICAL();
}

void Dem_MainFunction(void)
{
    const TickType_t now = xTaskGetTickCount();
    TickType_t last_heartbeat;
    uint8_t monitoring_enabled;

    taskENTER_CRITICAL();
    monitoring_enabled = DemControlTimeoutMonitoringEnabled;
    last_heartbeat = DemLastControlHeartbeatTick;
    taskEXIT_CRITICAL();

    if (monitoring_enabled == 0u)
    {
        return;
    }

    if ((TickType_t)(now - last_heartbeat) >=
        pdMS_TO_TICKS(DEM_CONTROL_TIMEOUT_MS))//判断是否超过 5 秒
    {
        Dem_SetEventStatus(DEM_EVENT_CONTROL_TIMEOUT, DEM_EVENT_FAILED);
    }
    else
    {
        Dem_SetEventStatus(DEM_EVENT_CONTROL_TIMEOUT, DEM_EVENT_PASSED);
    }
}

void Dem_ControlTimeoutStart(void)//开始监控
{
    taskENTER_CRITICAL();
    DemLastControlHeartbeatTick = xTaskGetTickCount();
    DemControlTimeoutMonitoringEnabled = 1u;
    taskEXIT_CRITICAL();
    Dem_SetEventStatus(DEM_EVENT_CONTROL_TIMEOUT, DEM_EVENT_PASSED);
}

void Dem_ControlTimeoutStop(void)//停止监控
{
    taskENTER_CRITICAL();
    DemControlTimeoutMonitoringEnabled = 0u;
    taskEXIT_CRITICAL();
    Dem_SetEventStatus(DEM_EVENT_CONTROL_TIMEOUT, DEM_EVENT_PASSED);
}

void Dem_ControlHeartbeatReceived(void)//收到心跳
{
    taskENTER_CRITICAL();
    if (DemControlTimeoutMonitoringEnabled != 0u)
    {
        DemLastControlHeartbeatTick = xTaskGetTickCount();
    }
    taskEXIT_CRITICAL();
}

void Dem_ControlCommandReceived(void)
{
    Dem_ControlHeartbeatReceived();
}

void Dem_ClearDTC(void)//清除所有 DTC
{
    uint8_t index;

    taskENTER_CRITICAL();
    for (index = 0u; index < DemEventConfigCount; index++)
    {
        DemEventRuntime[index].dtc.code = DemEventConfigs[index].dtc;
        DemEventRuntime[index].dtc.active = 0u;
        DemEventRuntime[index].dtc.stored = 0u;
        DemEventRuntime[index].dtc.level = 0u;
        DemEventRuntime[index].dtc.occurrence_counter = 0u;
        DemEventRuntime[index].dtc.status = 0u;
        DemEventRuntime[index].failCounter = 0u;
        DemEventRuntime[index].passCounter = 0u;
    }
    taskEXIT_CRITICAL();
}

uint8_t Dem_HasActiveDTC(void)//是否存在活动故障
{
    uint8_t index;
    uint8_t result = 0u;

    taskENTER_CRITICAL();
    for (index = 0u; index < DemEventConfigCount; index++)
    {
        if (DemEventRuntime[index].dtc.active != 0u)
        {
            result = 1u;
            break;
        }
    }
    taskEXIT_CRITICAL();
    return result;
}

DtcInfo_t Dem_GetPrimaryDTC(void)//取主要 DTC
{
    uint8_t index;
    DtcInfo_t empty_dtc = {0u, 0u, 0u, 0u, 0u, 0u};
    DtcInfo_t result = empty_dtc;

    taskENTER_CRITICAL();
    for (index = 0u; index < DemEventConfigCount; index++)
    {
        if (DemEventRuntime[index].dtc.active != 0u)
        {
            result = DemEventRuntime[index].dtc;
            taskEXIT_CRITICAL();
            return result;
        }
    }
    for (index = 0u; index < DemEventConfigCount; index++)
    {
        if (DemEventRuntime[index].dtc.stored != 0u)
        {
            result = DemEventRuntime[index].dtc;
            taskEXIT_CRITICAL();
            return result;
        }
    }
    taskEXIT_CRITICAL();
    return result;
}

uint8_t Dem_GetNumberOfDTC(void)
{
    return DemEventConfigCount;
}

uint8_t Dem_GetDTCByIndex(uint8_t index, DtcInfo_t *dtc)
{
    if ((dtc == (DtcInfo_t *)0) || (index >= DemEventConfigCount))
    {
        return 0u;
    }

    taskENTER_CRITICAL();
    *dtc = DemEventRuntime[index].dtc;
    taskEXIT_CRITICAL();
    return 1u;
}

uint8_t Dem_GetDTCByStatusMask(uint8_t status_mask,
                               uint8_t *cursor,
                               DtcInfo_t *dtc)//按状态筛选
{
    uint8_t index;

    if ((cursor == (uint8_t *)0) || (dtc == (DtcInfo_t *)0))
    {
        return 0u;
    }

    taskENTER_CRITICAL();
    for (index = *cursor; index < DemEventConfigCount; index++)
    {
        if ((DemEventRuntime[index].dtc.status & status_mask) != 0u)
        {
            *dtc = DemEventRuntime[index].dtc;
            *cursor = (uint8_t)(index + 1u);
            taskEXIT_CRITICAL();
            return 1u;
        }
    }
    *cursor = DemEventConfigCount;
    taskEXIT_CRITICAL();
    return 0u;
}
