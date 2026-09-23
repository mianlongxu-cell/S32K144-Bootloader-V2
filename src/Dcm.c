#include "FreeRTOS.h"
#include "task.h"
#include "DiagConfig.h"
#include "Dcm_Cfg.h"
#include "Dem.h"
#include "Dcm.h"
#include "boot_app_if.h"

#define UDS_SID_DIAGNOSTIC_SESSION_CONTROL  0x10u
#define UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION 0x14u//清除 DTC
#define UDS_SID_READ_DTC_INFORMATION         0x19u
#define UDS_SID_READ_DATA_BY_IDENTIFIER      0x22u//按 DID 读取数据
#define UDS_SID_TESTER_PRESENT              0x3Eu
#define UDS_POSITIVE_RESPONSE_OFFSET        0x40u  //正响应SID偏移

#define UDS_READ_DTC_BY_STATUS_MASK          0x02u  //0x19 服务的子功能
#define UDS_GROUP_OF_DTC_ALL                  0x00FFFFFFu //表示所有 DTC 组
//NRC否定码 7F 请求SID NRC
#define UDS_NRC_SERVICE_NOT_SUPPORTED       0x11u  //不支持该服务
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED   0x12u  //不支持该子功能
#define UDS_NRC_INCORRECT_LENGTH            0x13u  //请求长度或格式错误
#define UDS_NRC_REQUEST_OUT_OF_RANGE        0x31u  //请求参数超出允许范围

#define DCM_SESSION_REQUEST_LENGTH          2u
#define DCM_CLEAR_DTC_REQUEST_LENGTH        4u
#define DCM_READ_DTC_REQUEST_LENGTH         3u
#define DCM_READ_DID_REQUEST_LENGTH         3u
#define DCM_TESTER_PRESENT_REQUEST_LENGTH   2u

volatile DcmSession_t DcmCurrentSession;//当前诊断会话
volatile uint32_t DcmLastRequestTick; //最近一次刷新 S3 的时间
volatile uint32_t DcmRequestCount;//收到的请求数量
volatile uint32_t DcmNegativeResponseCount;//否定响应数量
volatile uint32_t DcmS3TimeoutCount;//S3 超时次数
volatile uint32_t DcmReadDidCount;//读取 DID 次数
volatile uint16_t DcmLastDid;//最近读取的 DID
volatile uint32_t DcmReadDtcCount;//读取 DTC 次数
volatile uint8_t DcmLastDtcStatusMask;
volatile uint8_t DcmLastReportedDtcCount;
volatile uint32_t DcmClearDtcCount;//清除 DTC 次数
volatile uint32_t DcmLastClearGroup;

static void Dcm_SetNegativeResponse(DiagPdu_t *response,
                                        uint8_t request_sid,
                                        uint8_t nrc)//构造否定响应
{
    response->length = 3u;
    response->data[0] = 0x7Fu;
    response->data[1] = request_sid;
    response->data[2] = nrc;
    DcmNegativeResponseCount++;
}

static void Dcm_RefreshS3Timer(void)//刷新 S3 定时器
{
    DcmLastRequestTick = (uint32_t)xTaskGetTickCount();
}

static void Dcm_ProcessSessionControl(const DiagPdu_t *request,
                                          DiagPdu_t *response)//处理会话控制服务0x10
{
    uint8_t sub_function;
    uint16_t p2_star_units;

    if (request->length != DCM_SESSION_REQUEST_LENGTH)
    {
        Dcm_SetNegativeResponse(response,
                                    UDS_SID_DIAGNOSTIC_SESSION_CONTROL,
                                    UDS_NRC_INCORRECT_LENGTH);
        return;
    }//检查长度

    sub_function = request->data[1];//检查子功能
    if ((sub_function != (uint8_t)DCM_SESSION_DEFAULT) &&
        (sub_function != (uint8_t)DCM_SESSION_PROGRAMMING) &&
        (sub_function != (uint8_t)DCM_SESSION_EXTENDED))
    {
        Dcm_SetNegativeResponse(response,
                                    UDS_SID_DIAGNOSTIC_SESSION_CONTROL,
                                    UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);//不支持其他值时返回0x12
        return;
    }

    DcmCurrentSession = (DcmSession_t)sub_function;//切换会话
    Dcm_RefreshS3Timer();//重启S3计时
//构建正响应
    response->length = 6u;
    response->data[0] = UDS_SID_DIAGNOSTIC_SESSION_CONTROL +
                        UDS_POSITIVE_RESPONSE_OFFSET;
    response->data[1] = sub_function;//子功能

    /* P2ServerMax is encoded in milliseconds: 50 ms = 0x0032. */
    response->data[2] = (uint8_t)(DCM_P2_SERVER_MAX_MS >> 8u);//返回服务器定时参数，p2，单位1ms
    response->data[3] = (uint8_t)(DCM_P2_SERVER_MAX_MS & 0xFFu);

    /* P2StarServerMax uses 10 ms units: 5000 ms / 10 = 0x01F4. */
    p2_star_units = (uint16_t)(DCM_P2_STAR_SERVER_MAX_MS / 10u);//p2*，单位10ms
    response->data[4] = (uint8_t)(p2_star_units >> 8u);
    response->data[5] = (uint8_t)(p2_star_units & 0xFFu);

    if (sub_function == (uint8_t)DCM_SESSION_PROGRAMMING)
    {
        (void)Boot_RequestReprogramming();
    }
}

static void Dcm_ProcessTesterPresent(const DiagPdu_t *request,
                                         DiagPdu_t *response)//保持诊断会话0x3E
{
    if (request->length != DCM_TESTER_PRESENT_REQUEST_LENGTH)
    {
        Dcm_SetNegativeResponse(response,
                                    UDS_SID_TESTER_PRESENT,
                                    UDS_NRC_INCORRECT_LENGTH);
        return;
    }

    if (request->data[1] != 0x00u)//目前只支持子功能 0x00。不支持时返回
    {
        Dcm_SetNegativeResponse(response,
                                    UDS_SID_TESTER_PRESENT,
                                    UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
        return;
    }

    Dcm_RefreshS3Timer();//重启S3计时
    response->length = 2u;
    response->data[0] = UDS_SID_TESTER_PRESENT +
                        UDS_POSITIVE_RESPONSE_OFFSET;
    response->data[1] = 0x00u;
}

static void Dcm_ProcessReadDid(const DiagPdu_t *request,
                                   DiagPdu_t *response)//读取 DID
{
    const DcmDidConfigType *config;
    uint16_t did;
    uint16_t data_length;

    if (request->length != DCM_READ_DID_REQUEST_LENGTH)
    {
        Dcm_SetNegativeResponse(response,
                                    UDS_SID_READ_DATA_BY_IDENTIFIER,
                                    UDS_NRC_INCORRECT_LENGTH);
        return;
    }//检查长度

    did = ((uint16_t)request->data[1] << 8u) |
          (uint16_t)request->data[2];//合并Did
    DcmLastDid = did;
    DcmReadDidCount++;
    Dcm_RefreshS3Timer();

    response->data[0] = UDS_SID_READ_DATA_BY_IDENTIFIER +
                        UDS_POSITIVE_RESPONSE_OFFSET;
    response->data[1] = request->data[1];
    response->data[2] = request->data[2];//3字节响应头

    config = Dcm_CfgGetDid(did);
    if ((config == (const DcmDidConfigType *)0) ||
        (config->readFunction(&response->data[3], &data_length) == 0u) ||
        (((uint32_t)data_length + 3u) > DIAG_MAX_PDU_LENGTH))
    {
        Dcm_SetNegativeResponse(response,
                                    UDS_SID_READ_DATA_BY_IDENTIFIER,
                                    UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    response->length = (uint16_t)(3u + data_length);
}

static void Dcm_ProcessReadDtcInformation(const DiagPdu_t *request,
                                               DiagPdu_t *response)//读取 DTC 19 02 statusMask
{
    DtcInfo_t dtc;
    uint8_t cursor;
    uint8_t status_mask;

    if (request->length != DCM_READ_DTC_REQUEST_LENGTH)
    {
        Dcm_SetNegativeResponse(response,
                                    UDS_SID_READ_DTC_INFORMATION,
                                    UDS_NRC_INCORRECT_LENGTH);
        return;
    }

    if (request->data[1] != UDS_READ_DTC_BY_STATUS_MASK)
    {
        Dcm_SetNegativeResponse(response,
                                    UDS_SID_READ_DTC_INFORMATION,
                                    UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
        return;
    }

    status_mask = request->data[2];
    DcmReadDtcCount++;
    DcmLastDtcStatusMask = status_mask;
    DcmLastReportedDtcCount = 0u;//当前响应已加入的 DTC 数量
    Dcm_RefreshS3Timer();

    response->length = 3u;
    response->data[0] = UDS_SID_READ_DTC_INFORMATION +
                        UDS_POSITIVE_RESPONSE_OFFSET;
    response->data[1] = UDS_READ_DTC_BY_STATUS_MASK;
    response->data[2] = DEM_DTC_STATUS_AVAILABILITY;

    cursor = 0u;
    while (Dem_GetDTCByStatusMask(status_mask, &cursor, &dtc) != 0u)
    {
        if (((uint32_t)response->length + 4u) > DIAG_MAX_PDU_LENGTH)//写入DTC,每个DTC占4字节DTC高字节 DTC中字节 DTC低字节 DTC状态
        {
            Dcm_SetNegativeResponse(response,
                                        UDS_SID_READ_DTC_INFORMATION,
                                        UDS_NRC_REQUEST_OUT_OF_RANGE);
            return;
        }

        response->data[response->length] =
            (uint8_t)((dtc.code >> 16u) & 0xFFu);
        response->data[response->length + 1u] =
            (uint8_t)((dtc.code >> 8u) & 0xFFu);
        response->data[response->length + 2u] =
            (uint8_t)(dtc.code & 0xFFu);
        response->data[response->length + 3u] = dtc.status;
        response->length = (uint16_t)(response->length + 4u);
        DcmLastReportedDtcCount++;
    }
}

static void Dcm_ProcessClearDiagnosticInformation(
    const DiagPdu_t *request,
    DiagPdu_t *response)//清除 DTC 14 groupOfDTC高字节 groupOfDTC中字节 groupOfDTC低字节
{
    uint32_t group_of_dtc;

    if (request->length != DCM_CLEAR_DTC_REQUEST_LENGTH)
    {
        Dcm_SetNegativeResponse(response,
                                    UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION,
                                    UDS_NRC_INCORRECT_LENGTH);
        return;
    }

    group_of_dtc = ((uint32_t)request->data[1] << 16u) |
                   ((uint32_t)request->data[2] << 8u) |
                   (uint32_t)request->data[3];
    DcmLastClearGroup = group_of_dtc;

    if (group_of_dtc != UDS_GROUP_OF_DTC_ALL)
    {
        Dcm_SetNegativeResponse(response,
                                    UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION,
                                    UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    Dem_ClearDTC();
    DcmClearDtcCount++;
    Dcm_RefreshS3Timer();

    response->length = 1u;
    response->data[0] = UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION +
                        UDS_POSITIVE_RESPONSE_OFFSET;
}

void Dcm_Init(void)
{
    DcmCurrentSession = DCM_SESSION_DEFAULT;
    DcmLastRequestTick = (uint32_t)xTaskGetTickCount();
    DcmRequestCount = 0u;
    DcmNegativeResponseCount = 0u;
    DcmS3TimeoutCount = 0u;
    DcmReadDidCount = 0u;
    DcmLastDid = 0u;
    DcmReadDtcCount = 0u;
    DcmLastDtcStatusMask = 0u;
    DcmLastReportedDtcCount = 0u;
    DcmClearDtcCount = 0u;
    DcmLastClearGroup = 0u;
}

uint8_t Dcm_ProcessRequest(const DiagPdu_t *request,
                              DiagPdu_t *response)//解析并分发一条完整的 UDS 请求,总请求分发函数
{
    uint8_t sid;

    if ((request == (const DiagPdu_t *)0) ||
        (response == (DiagPdu_t *)0) ||
        (request->length == 0u) ||
        (request->length > DIAG_MAX_PDU_LENGTH))
    {
        return 0u;
    }

    response->length = 0u;
    sid = request->data[0];
    DcmRequestCount++;//请求计数器加一

    switch (sid)
    {
        case UDS_SID_DIAGNOSTIC_SESSION_CONTROL:
            Dcm_ProcessSessionControl(request, response);
            break;

        case UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION:
            Dcm_ProcessClearDiagnosticInformation(request, response);
            break;

        case UDS_SID_TESTER_PRESENT://保持会话
            Dcm_ProcessTesterPresent(request, response);
            break;

        case UDS_SID_READ_DTC_INFORMATION:
            Dcm_ProcessReadDtcInformation(request, response);
            break;

        case UDS_SID_READ_DATA_BY_IDENTIFIER:
            Dcm_ProcessReadDid(request, response);
            break;

        default:
            Dcm_SetNegativeResponse(response,
                                        sid,
                                        UDS_NRC_SERVICE_NOT_SUPPORTED);//sid不在支持列表，返回nrc
            break;
    }

    return (response->length != 0u) ? 1u : 0u;
}

void Dcm_MainFunction(void)//监控扩展诊断会话的 S3 超时,每 1 ms 调用一次
{
    const TickType_t now = xTaskGetTickCount();

    Boot_AppIfMainFunction();//处理 Bootloader 请求

    if ((DcmCurrentSession == DCM_SESSION_EXTENDED) &&
        ((TickType_t)(now - (TickType_t)DcmLastRequestTick) >=
         pdMS_TO_TICKS(DCM_S3_TIMEOUT_MS)))//ECU 当前处于扩展诊断会话，距离上一次诊断请求已经达到 DCM_S3_TIMEOUT_MS
    {
        DcmCurrentSession = DCM_SESSION_DEFAULT;//返回默认会话
        DcmLastRequestTick = (uint32_t)now;
        DcmS3TimeoutCount++;//s3超时计数器加一
    }
}
