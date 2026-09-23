#ifndef VEHICLE_APP_H_
#define VEHICLE_APP_H_

void VehicleApp_Init(void);
void Runnable_VehicleModel_100ms(void);
void Runnable_FaultMonitor_100ms(void);
void Runnable_ControlCommand(void);
uint32_t VehicleApp_GetCycleCount(void);

#endif /* VEHICLE_APP_H_ */
