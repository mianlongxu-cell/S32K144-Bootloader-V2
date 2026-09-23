#ifndef VEHICLE_MODEL_H_
#define VEHICLE_MODEL_H_

#include <stdint.h>

typedef struct
{
    uint16_t vehicle_speed;
    uint16_t engine_rpm;
    uint8_t coolant_temp;
    uint8_t gear;
    uint8_t powertrain_status;
} VehiclePowertrainStatus_t;

typedef struct
{
    uint8_t light_status;
    uint8_t door_status;
    uint8_t wiper_status;
    uint8_t turn_signal_status;
    uint8_t lock_status;
} VehicleBodyStatus_t;

extern volatile VehiclePowertrainStatus_t VehiclePowertrainStatus;
extern volatile VehicleBodyStatus_t VehicleBodyStatus;

void VehicleModel_Init(void);
void VehicleModel_Update100ms(void);
void VehicleModel_GetPowertrainStatus(VehiclePowertrainStatus_t *status);
void VehicleModel_ResetSpeed(void);
void VehicleModel_SetHighTemperature(void);
void VehicleModel_ClearFaultConditions(void);

void VehicleModel_ToggleLight(void);
void VehicleModel_ToggleDoor(void);
void VehicleModel_ToggleWiper(void);
void VehicleModel_ToggleLock(void);
void VehicleModel_SetTurnSignal(uint8_t turn_signal_status);

#endif /* VEHICLE_MODEL_H_ */
