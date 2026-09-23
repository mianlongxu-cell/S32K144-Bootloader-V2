#ifndef DCM_H_
#define DCM_H_

#include <stdint.h>
#include "DiagTypes.h"

typedef enum
{
    DCM_SESSION_DEFAULT = 1,
    DCM_SESSION_PROGRAMMING = 2,
    DCM_SESSION_EXTENDED = 3
} DcmSession_t;//定义 DCM 当前会话类型

extern volatile DcmSession_t DcmCurrentSession;
extern volatile uint32_t DcmLastRequestTick;//记录最近一次收到有效诊断请求的 FreeRTOS Tick 时间
extern volatile uint32_t DcmRequestCount;
extern volatile uint32_t DcmNegativeResponseCount;
extern volatile uint32_t DcmS3TimeoutCount;
extern volatile uint32_t DcmReadDidCount;
extern volatile uint16_t DcmLastDid;
extern volatile uint32_t DcmReadDtcCount;
extern volatile uint8_t DcmLastDtcStatusMask;
extern volatile uint8_t DcmLastReportedDtcCount;
extern volatile uint32_t DcmClearDtcCount;
extern volatile uint32_t DcmLastClearGroup;

void Dcm_Init(void);
uint8_t Dcm_ProcessRequest(const DiagPdu_t *request,
                           DiagPdu_t *response);
void Dcm_MainFunction(void);

#endif /* DCM_H_ */
