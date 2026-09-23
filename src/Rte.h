#ifndef RTE_H_
#define RTE_H_

#include <stdint.h>
#include "ControlCommand_Cfg.h"
#include "Dem.h"

typedef struct
{
    uint16_t vehicleSpeed;
    uint16_t engineRpm;
    uint8_t coolantTemp;
    uint8_t gear;
    uint8_t powertrainStatus;
} RtePowertrainStatusType;

typedef struct
{
    uint8_t lightStatus;
    uint8_t doorStatus;
    uint8_t wiperStatus;
    uint8_t turnSignalStatus;
    uint8_t lockStatus;
} RteBodyStatusType;

void Rte_Init(void);
void Rte_Write_VehicleSpeed(uint16_t value);
uint16_t Rte_Read_VehicleSpeed(void);
void Rte_Write_EngineRpm(uint16_t value);
uint16_t Rte_Read_EngineRpm(void);
void Rte_Write_CoolantTemp(uint8_t value);
uint8_t Rte_Read_CoolantTemp(void);
void Rte_Write_Gear(uint8_t value);
uint8_t Rte_Read_Gear(void);
void Rte_Write_PowertrainStatus(uint8_t value);
uint8_t Rte_Read_PowertrainStatus(void);
void Rte_Write_LightStatus(uint8_t value);
uint8_t Rte_Read_LightStatus(void);
void Rte_Write_DoorStatus(uint8_t value);
uint8_t Rte_Read_DoorStatus(void);
void Rte_Write_WiperStatus(uint8_t value);
uint8_t Rte_Read_WiperStatus(void);
void Rte_Write_TurnSignalStatus(uint8_t value);
uint8_t Rte_Read_TurnSignalStatus(void);
void Rte_Write_LockStatus(uint8_t value);
uint8_t Rte_Read_LockStatus(void);
void Rte_Read_PowertrainSnapshot(RtePowertrainStatusType *status);
void Rte_Read_BodySnapshot(RteBodyStatusType *status);
uint8_t Rte_Read_HasActiveDTC(void);
DtcInfo_t Rte_Read_PrimaryDTC(void);
void Rte_Write_ControlCommand(uint8_t command, uint8_t argument);
uint8_t Rte_Read_ControlCommand(uint8_t *command, uint8_t *argument);
void Rte_SetCyclicTxEnabled(uint8_t enabled);
void Rte_Call_Dem_SetEventStatus(DemEventIdType event_id,
                                 DemEventStatusType status);
void Rte_Call_Dem_ControlTimeoutStart(void);
void Rte_Call_Dem_ControlTimeoutStop(void);
void Rte_Call_Dem_ControlHeartbeatReceived(void);
void Rte_Call_Dem_ControlCommandReceived(void);
void Rte_Call_Dem_ClearDTC(void);
void Rte_Call_Dem_MainFunction(void);

#endif /* RTE_H_ */
