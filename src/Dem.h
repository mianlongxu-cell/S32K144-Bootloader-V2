#ifndef DEM_H_
#define DEM_H_

#include "Dem_Cfg.h"
#include "Dem_Types.h"

#define DEM_DTC_STATUS_TEST_FAILED    0x01u//当前故障条件仍然存在
#define DEM_DTC_STATUS_PENDING_DTC    0x04u//表示故障已经被检测到，但还没有达到确认门限
#define DEM_DTC_STATUS_CONFIRMED_DTC  0x08u//表示故障已经正式确认，并且通常已经存储
#define DEM_DTC_STATUS_AVAILABILITY   0x0Du

void Dem_Init(void);
void Dem_SetEventStatus(DemEventIdType event_id, DemEventStatusType status);
void Dem_MainFunction(void);
void Dem_ControlTimeoutStart(void);
void Dem_ControlTimeoutStop(void);
void Dem_ControlHeartbeatReceived(void);
/* Compatibility entry point: a control Rx indication now means heartbeat. */
void Dem_ControlCommandReceived(void);
void Dem_ClearDTC(void);
uint8_t Dem_HasActiveDTC(void);
DtcInfo_t Dem_GetPrimaryDTC(void);
uint8_t Dem_GetNumberOfDTC(void);
uint8_t Dem_GetDTCByIndex(uint8_t index, DtcInfo_t *dtc);
uint8_t Dem_GetDTCByStatusMask(uint8_t status_mask,
                               uint8_t *cursor,
                               DtcInfo_t *dtc);

#endif /* DEM_H_ */
