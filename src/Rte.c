#include "Rte.h"
#include "Com.h"
#include "VehicleApp.h"

static volatile RtePowertrainStatusType RtePowertrainStatus;
static volatile RteBodyStatusType RteBodyStatus;
static uint8_t RteControlCommand;
static uint8_t RteControlArgument;
static uint8_t RteControlCommandPending;

void Rte_Init(void)
{
    RtePowertrainStatus.vehicleSpeed = 0u;
    RtePowertrainStatus.engineRpm = 0u;
    RtePowertrainStatus.coolantTemp = 0u;
    RtePowertrainStatus.gear = 0u;
    RtePowertrainStatus.powertrainStatus = 0u;
    RteBodyStatus.lightStatus = 0u;
    RteBodyStatus.doorStatus = 0u;
    RteBodyStatus.wiperStatus = 0u;
    RteBodyStatus.turnSignalStatus = 0u;
    RteBodyStatus.lockStatus = 0u;
    RteControlCommand = 0u;
    RteControlArgument = 0u;
    RteControlCommandPending = 0u;
}

void Rte_Write_VehicleSpeed(uint16_t value)
{
    RtePowertrainStatus.vehicleSpeed = value;
}

uint16_t Rte_Read_VehicleSpeed(void)
{
    return RtePowertrainStatus.vehicleSpeed;
}

void Rte_Write_EngineRpm(uint16_t value)
{
    RtePowertrainStatus.engineRpm = value;
}

uint16_t Rte_Read_EngineRpm(void)
{
    return RtePowertrainStatus.engineRpm;
}

void Rte_Write_CoolantTemp(uint8_t value)
{
    RtePowertrainStatus.coolantTemp = value;
}

uint8_t Rte_Read_CoolantTemp(void)
{
    return RtePowertrainStatus.coolantTemp;
}

void Rte_Write_Gear(uint8_t value)
{
    RtePowertrainStatus.gear = value;
}

uint8_t Rte_Read_Gear(void)
{
    return RtePowertrainStatus.gear;
}

void Rte_Write_PowertrainStatus(uint8_t value)
{
    RtePowertrainStatus.powertrainStatus = value;
}

uint8_t Rte_Read_PowertrainStatus(void)
{
    return RtePowertrainStatus.powertrainStatus;
}

void Rte_Write_LightStatus(uint8_t value)
{
    RteBodyStatus.lightStatus = value;
}

uint8_t Rte_Read_LightStatus(void)
{
    return RteBodyStatus.lightStatus;
}

void Rte_Write_DoorStatus(uint8_t value)
{
    RteBodyStatus.doorStatus = value;
}

uint8_t Rte_Read_DoorStatus(void)
{
    return RteBodyStatus.doorStatus;
}

void Rte_Write_WiperStatus(uint8_t value)
{
    RteBodyStatus.wiperStatus = value;
}

uint8_t Rte_Read_WiperStatus(void)
{
    return RteBodyStatus.wiperStatus;
}

void Rte_Write_TurnSignalStatus(uint8_t value)
{
    RteBodyStatus.turnSignalStatus = value;
}

uint8_t Rte_Read_TurnSignalStatus(void)
{
    return RteBodyStatus.turnSignalStatus;
}

void Rte_Write_LockStatus(uint8_t value)
{
    RteBodyStatus.lockStatus = value;
}

uint8_t Rte_Read_LockStatus(void)
{
    return RteBodyStatus.lockStatus;
}

void Rte_Read_PowertrainSnapshot(RtePowertrainStatusType *status)//一次性读取整个动力系统状态
{
    if (status == (RtePowertrainStatusType *)0)
    {
        return;
    }

    status->vehicleSpeed = RtePowertrainStatus.vehicleSpeed;
    status->engineRpm = RtePowertrainStatus.engineRpm;
    status->coolantTemp = RtePowertrainStatus.coolantTemp;
    status->gear = RtePowertrainStatus.gear;
    status->powertrainStatus = RtePowertrainStatus.powertrainStatus;
}

void Rte_Read_BodySnapshot(RteBodyStatusType *status)
{
    if (status == (RteBodyStatusType *)0)
    {
        return;
    }

    status->lightStatus = RteBodyStatus.lightStatus;
    status->doorStatus = RteBodyStatus.doorStatus;
    status->wiperStatus = RteBodyStatus.wiperStatus;
    status->turnSignalStatus = RteBodyStatus.turnSignalStatus;
    status->lockStatus = RteBodyStatus.lockStatus;
}

uint8_t Rte_Read_HasActiveDTC(void)//RTE 读取 DEM 数据
{
    return Dem_HasActiveDTC();
}

DtcInfo_t Rte_Read_PrimaryDTC(void)//读取主要 DTC：
{
    return Dem_GetPrimaryDTC();
}

void Rte_SetCyclicTxEnabled(uint8_t enabled)//RTE 调用 COM
{
    Com_SetCyclicTxEnabled(enabled);
}

void Rte_Write_ControlCommand(uint8_t command, uint8_t argument)//控制命令写入和处理
{
    RteControlCommand = command;
    RteControlArgument = argument;
    RteControlCommandPending = 1u;
    Runnable_ControlCommand();
}

uint8_t Rte_Read_ControlCommand(uint8_t *command, uint8_t *argument)
{
    if ((command == (uint8_t *)0) || (argument == (uint8_t *)0) ||
        (RteControlCommandPending == 0u))
    {
        return 0u;
    }

    *command = RteControlCommand;
    *argument = RteControlArgument;
    RteControlCommandPending = 0u;
    return 1u;
}

void Rte_Call_Dem_SetEventStatus(DemEventIdType event_id,
                                 DemEventStatusType status)//RTE 调用 DEM 的包装函数,VehicleApp 只依赖 RTE，不需要直接包含和调用 DEM 的底层接口。
{
    Dem_SetEventStatus(event_id, status);
}

void Rte_Call_Dem_ControlCommandReceived(void)
{
    Dem_ControlCommandReceived();
}

void Rte_Call_Dem_ControlTimeoutStart(void)
{
    Dem_ControlTimeoutStart();
}

void Rte_Call_Dem_ControlTimeoutStop(void)
{
    Dem_ControlTimeoutStop();
}

void Rte_Call_Dem_ControlHeartbeatReceived(void)
{
    Dem_ControlHeartbeatReceived();
}

void Rte_Call_Dem_ClearDTC(void)
{
    Dem_ClearDTC();
}

void Rte_Call_Dem_MainFunction(void)
{
    Dem_MainFunction();
}
