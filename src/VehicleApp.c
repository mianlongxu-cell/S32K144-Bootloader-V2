#include "Rte.h"
#include "VehicleApp.h"
#include "VehicleModel.h"
static volatile uint32_t VehicleAppCycleCount;

static void VehicleApp_PublishSignals(void)//RTE保存车辆信号快照
{
    VehiclePowertrainStatus_t powertrain;

    VehicleModel_GetPowertrainStatus(&powertrain);
    Rte_Write_VehicleSpeed(powertrain.vehicle_speed);
    Rte_Write_EngineRpm(powertrain.engine_rpm);
    Rte_Write_CoolantTemp(powertrain.coolant_temp);
    Rte_Write_Gear(powertrain.gear);
    Rte_Write_PowertrainStatus(powertrain.powertrain_status);
    Rte_Write_LightStatus(VehicleBodyStatus.light_status);
    Rte_Write_DoorStatus(VehicleBodyStatus.door_status);
    Rte_Write_WiperStatus(VehicleBodyStatus.wiper_status);
    Rte_Write_TurnSignalStatus(VehicleBodyStatus.turn_signal_status);
    Rte_Write_LockStatus(VehicleBodyStatus.lock_status);//车辆模型数据写入 RTE：
}

void VehicleApp_Init(void)
{
    VehicleAppCycleCount = 0u;
    VehicleModel_Init();
    VehicleApp_PublishSignals();
}

void Runnable_VehicleModel_100ms(void)//车辆模型更新
{
    VehicleModel_Update100ms();
    VehicleApp_PublishSignals();
    VehicleAppCycleCount++;
}

uint32_t VehicleApp_GetCycleCount(void) { return VehicleAppCycleCount; }

void Runnable_FaultMonitor_100ms(void)//故障监控
{
    Rte_Call_Dem_SetEventStatus(DEM_EVENT_COOLANT_OVER_TEMP,
        (Rte_Read_CoolantTemp() > 105u) ? DEM_EVENT_FAILED :
        DEM_EVENT_PASSED);//温度 > 105℃ → 事件失败
    Rte_Call_Dem_SetEventStatus(DEM_EVENT_VEHICLE_SPEED_ABNORMAL,
        (Rte_Read_VehicleSpeed() > 180u) ? DEM_EVENT_FAILED :
        DEM_EVENT_PASSED);
    Rte_Call_Dem_MainFunction();//让DEM 真正处理事件状态
}

void Runnable_ControlCommand(void)//处理 CAN 控制命令
{
    uint8_t command;
    uint8_t argument;

    if (Rte_Read_ControlCommand(&command, &argument) == 0u)
    {
        return;
    }

    switch (command)
    {
        case CMD_START_TX:
            Rte_SetCyclicTxEnabled(1u);
            Rte_Call_Dem_ControlTimeoutStart();//启动周期发送
            break;

        case CMD_STOP_TX:
            Rte_SetCyclicTxEnabled(0u);
            Rte_Call_Dem_ControlTimeoutStop();
            break;

        case CMD_RESET_SPEED:
            VehicleModel_ResetSpeed();
            break;

        case CMD_INJECT_OVERTEMP:
            VehicleModel_SetHighTemperature();
            break;

        case CMD_CLEAR_DTC:
            VehicleModel_ClearFaultConditions();//清理车辆模型中的异常条件
            Rte_Call_Dem_ClearDTC();//清除 DEM 中已存储的故障
            break;

        case CMD_TOGGLE_LIGHT:
            VehicleModel_ToggleLight();
            break;

        case CMD_TOGGLE_DOOR:
            VehicleModel_ToggleDoor();
            break;

        case CMD_TOGGLE_WIPER:
            VehicleModel_ToggleWiper();
            break;

        case CMD_TOGGLE_LOCK:
            VehicleModel_ToggleLock();
            break;

        case CMD_SET_TURN_SIGNAL:
            VehicleModel_SetTurnSignal(argument);
            break;

        case CMD_CONTROL_HEARTBEAT:
            Rte_Call_Dem_ControlHeartbeatReceived();
            break;

        default:
            break;
    }

    VehicleApp_PublishSignals();
}
