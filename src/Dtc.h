#ifndef DTC_H_
#define DTC_H_

#include <stdint.h>
#include "Dem_Types.h"

#define DTC_CODE_MASK             0x00FFFFFFu
#define DTC_CODE_COOLANT_HIGH     0x000101u//冷却液温度过高
#define DTC_CODE_SPEED_ABNORMAL   0x000201u//车速异常
#define DTC_CODE_COMMAND_TIMEOUT  0x000301u//控制命令超时

/* Simplified UDS-style status, not a complete AUTOSAR DEM status model. */
#define DTC_STATUS_TEST_FAILED     0x01u//当前故障存在
#define DTC_STATUS_CONFIRMED_DTC   0x08u//故障已确认并存储
#define DTC_STATUS_AVAILABILITY    0x0Du//支持testFailed、pendingDTC、confirmedDTC
#define DTC_STATUS_ACTIVE          DTC_STATUS_TEST_FAILED
#define DTC_STATUS_STORED          DTC_STATUS_CONFIRMED_DTC

extern volatile uint32_t Dtc_LastControlRxTick;

void Dtc_Init(void);
void Dtc_Update(uint16_t speed, uint8_t coolant_temp);
void Dtc_ControlRxIndication(void);
void Dtc_ClearAll(void);
uint8_t Dtc_HasActiveFault(void);
DtcInfo_t Dtc_GetPrimaryDtc(void);
uint8_t Dtc_GetCount(void);
uint8_t Dtc_GetDtcByIndex(uint8_t index, DtcInfo_t *dtc);

#endif /* DTC_H_ */
