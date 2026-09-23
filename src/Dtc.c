#include "FreeRTOS.h"
#include "task.h"
#include "Dem.h"
#include "Dtc.h"

volatile uint32_t Dtc_LastControlRxTick;

void Dtc_Init(void)
{
    Dem_Init();
    Dtc_LastControlRxTick = (uint32_t)xTaskGetTickCount();
}

void Dtc_Update(uint16_t speed, uint8_t coolant_temp)
{
    Dem_SetEventStatus(DEM_EVENT_COOLANT_OVER_TEMP,
        (coolant_temp > 105u) ? DEM_EVENT_FAILED : DEM_EVENT_PASSED);
    Dem_SetEventStatus(DEM_EVENT_VEHICLE_SPEED_ABNORMAL,
        (speed > 180u) ? DEM_EVENT_FAILED : DEM_EVENT_PASSED);
    Dem_MainFunction();
}

void Dtc_ControlRxIndication(void)
{
    Dtc_LastControlRxTick = (uint32_t)xTaskGetTickCount();
    Dem_ControlCommandReceived();
}

void Dtc_ClearAll(void)
{
    Dem_ClearDTC();
}

uint8_t Dtc_HasActiveFault(void)
{
    return Dem_HasActiveDTC();
}

DtcInfo_t Dtc_GetPrimaryDtc(void)
{
    return Dem_GetPrimaryDTC();
}

uint8_t Dtc_GetCount(void)
{
    return Dem_GetNumberOfDTC();
}

uint8_t Dtc_GetDtcByIndex(uint8_t index, DtcInfo_t *dtc)
{
    return Dem_GetDTCByIndex(index, dtc);
}
