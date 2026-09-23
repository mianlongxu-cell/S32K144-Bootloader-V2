#include "VehicleModel.h"

#define VEHICLE_MAX_SIMULATED_SPEED        120u
#define VEHICLE_IDLE_RPM                   800u
#define VEHICLE_RPM_PER_KMH                30u
#define VEHICLE_NORMAL_COOLANT_LIMIT       90u
#define VEHICLE_HIGH_TEMPERATURE           110u
#define COOLANT_UPDATE_PERIOD_100MS_TICKS  10u

volatile VehiclePowertrainStatus_t VehiclePowertrainStatus;
volatile VehicleBodyStatus_t VehicleBodyStatus;

static uint32_t CoolantUpdateTicks;

static uint8_t VehicleModel_CalculateGear(uint16_t speed)
{
    uint8_t gear;

    if (speed < 20u)
    {
        gear = 1u;
    }
    else if (speed < 40u)
    {
        gear = 2u;
    }
    else if (speed < 70u)
    {
        gear = 3u;
    }
    else if (speed < 100u)
    {
        gear = 4u;
    }
    else
    {
        gear = 5u;
    }

    return gear;
}

void VehicleModel_Init(void)
{
    VehiclePowertrainStatus.vehicle_speed = 0u;
    VehiclePowertrainStatus.engine_rpm = VEHICLE_IDLE_RPM;
    VehiclePowertrainStatus.coolant_temp = 40u;
    VehiclePowertrainStatus.gear = 1u;
    VehiclePowertrainStatus.powertrain_status = 0u;

    VehicleBodyStatus.light_status = 0u;
    VehicleBodyStatus.door_status = 0u;
    VehicleBodyStatus.wiper_status = 0u;
    VehicleBodyStatus.turn_signal_status = 0u;
    VehicleBodyStatus.lock_status = 0u;

    CoolantUpdateTicks = 0u;
}

void VehicleModel_Update100ms(void)
{
    if (VehiclePowertrainStatus.vehicle_speed >=
        VEHICLE_MAX_SIMULATED_SPEED)
    {
        VehiclePowertrainStatus.vehicle_speed = 0u;
    }
    else
    {
        VehiclePowertrainStatus.vehicle_speed++;
    }

    VehiclePowertrainStatus.engine_rpm =
        (uint16_t)(VEHICLE_IDLE_RPM +
        (VehiclePowertrainStatus.vehicle_speed * VEHICLE_RPM_PER_KMH));
    VehiclePowertrainStatus.gear = VehicleModel_CalculateGear(
        VehiclePowertrainStatus.vehicle_speed);

    if (VehiclePowertrainStatus.coolant_temp <
        VEHICLE_NORMAL_COOLANT_LIMIT)
    {
        CoolantUpdateTicks++;
        if (CoolantUpdateTicks >= COOLANT_UPDATE_PERIOD_100MS_TICKS)
        {
            VehiclePowertrainStatus.coolant_temp++;
            CoolantUpdateTicks = 0u;
        }
    }
}

void VehicleModel_GetPowertrainStatus(VehiclePowertrainStatus_t *status)
{
    if (status == (VehiclePowertrainStatus_t *)0)
    {
        return;
    }

    status->vehicle_speed = VehiclePowertrainStatus.vehicle_speed;
    status->engine_rpm = VehiclePowertrainStatus.engine_rpm;
    status->coolant_temp = VehiclePowertrainStatus.coolant_temp;
    status->gear = VehiclePowertrainStatus.gear;
    status->powertrain_status =
        VehiclePowertrainStatus.powertrain_status;
}

void VehicleModel_ResetSpeed(void)
{
    VehiclePowertrainStatus.vehicle_speed = 0u;
    VehiclePowertrainStatus.engine_rpm = VEHICLE_IDLE_RPM;
    VehiclePowertrainStatus.gear = 1u;
}

void VehicleModel_SetHighTemperature(void)
{
    VehiclePowertrainStatus.coolant_temp = VEHICLE_HIGH_TEMPERATURE;
    CoolantUpdateTicks = 0u;
}

void VehicleModel_ClearFaultConditions(void)
{
    if (VehiclePowertrainStatus.coolant_temp > 100u)
    {
        VehiclePowertrainStatus.coolant_temp =
            VEHICLE_NORMAL_COOLANT_LIMIT;
        CoolantUpdateTicks = 0u;
    }

    if (VehiclePowertrainStatus.vehicle_speed > 180u)
    {
        VehicleModel_ResetSpeed();
    }
}

void VehicleModel_ToggleLight(void)
{
    VehicleBodyStatus.light_status ^= 1u;
}

void VehicleModel_ToggleDoor(void)
{
    VehicleBodyStatus.door_status ^= 1u;
}

void VehicleModel_ToggleWiper(void)
{
    VehicleBodyStatus.wiper_status ^= 1u;
}

void VehicleModel_ToggleLock(void)
{
    VehicleBodyStatus.lock_status ^= 1u;
}

void VehicleModel_SetTurnSignal(uint8_t turn_signal_status)
{
    if (turn_signal_status <= 3u)
    {
        VehicleBodyStatus.turn_signal_status = turn_signal_status;
    }
}
