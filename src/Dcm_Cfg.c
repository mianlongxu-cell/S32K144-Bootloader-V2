#include "Dcm_Cfg.h"
#include "DiagConfig.h"
#include "Rte.h"
#include "AppVersion.h"
#include "boot_app_if.h"

static uint8_t Dcm_CfgReadSlot(uint8_t *data, uint16_t *length)
{
    BootAppSlotType slot;
    if (Boot_GetRunningSlot(&slot) != BOOT_APP_IF_OK) { return 0u; }
    data[0] = (uint8_t)slot; *length = 1u; return 1u;
}

static uint8_t Dcm_CfgReadImageVersion(uint8_t *data, uint16_t *length)
{
    uint32_t version;
    if (Boot_GetRunningVersion(&version) != BOOT_APP_IF_OK) { return 0u; }
    data[0]=(uint8_t)(version>>24); data[1]=(uint8_t)(version>>16);
    data[2]=(uint8_t)(version>>8); data[3]=(uint8_t)version;
    *length=4u; return 1u;
}

#define DCM_CFG_SW_VERSION_LENGTH  4u
#define DCM_CFG_VIN_LENGTH         17u

static const uint8_t DcmCfgSoftwareVersion[DCM_CFG_SW_VERSION_LENGTH] =
{
    'V', APP_VERSION_MAJOR_CHAR, '.', APP_VERSION_MINOR_CHAR
};

static const uint8_t DcmCfgVin[DCM_CFG_VIN_LENGTH] =
{
    'T', 'E', 'S', 'T', 'S', '3', '2', 'K', '1',
    '4', '4', '0', '0', '0', '0', '0', '1'
};

static uint8_t Dcm_CfgReadVin(uint8_t *data, uint16_t *length)//VIN 读取函数
{
    uint8_t index;
    for (index = 0u; index < DCM_CFG_VIN_LENGTH; index++)
    {
        data[index] = DcmCfgVin[index];
    }
    *length = DCM_CFG_VIN_LENGTH;
    return 1u;
}

static uint8_t Dcm_CfgReadSoftwareVersion(uint8_t *data, uint16_t *length)//软件版本读取函数
{
    uint8_t index;
    for (index = 0u; index < DCM_CFG_SW_VERSION_LENGTH; index++)
    {
        data[index] = DcmCfgSoftwareVersion[index];
    }
    *length = DCM_CFG_SW_VERSION_LENGTH;
    return 1u;
}

static uint8_t Dcm_CfgReadVehicleSpeed(uint8_t *data, uint16_t *length)//读取车速
{
    const uint16_t value = Rte_Read_VehicleSpeed();//RTE
    data[0] = (uint8_t)(value & 0xFFu);
    data[1] = (uint8_t)(value >> 8u);
    *length = 2u;
    return 1u;
}

static uint8_t Dcm_CfgReadEngineRpm(uint8_t *data, uint16_t *length)//读取发动机转速
{
    const uint16_t value = Rte_Read_EngineRpm();
    data[0] = (uint8_t)(value & 0xFFu);
    data[1] = (uint8_t)(value >> 8u);
    *length = 2u;
    return 1u;
}

static uint8_t Dcm_CfgReadCoolantTemp(uint8_t *data, uint16_t *length)
{
    data[0] = Rte_Read_CoolantTemp();
    *length = 1u;
    return 1u;
}

static uint8_t Dcm_CfgReadGear(uint8_t *data, uint16_t *length)
{
    data[0] = Rte_Read_Gear();
    *length = 1u;
    return 1u;
}

const DcmDidConfigType DcmDidConfigs[] =//DID 配置表
{
    {DID_VIN, Dcm_CfgReadVin},
    {DID_SW_VERSION, Dcm_CfgReadSoftwareVersion},
    {0xF101u, Dcm_CfgReadSlot},
    {0xF102u, Dcm_CfgReadImageVersion},
    {DID_VEHICLE_SPEED, Dcm_CfgReadVehicleSpeed},
    {DID_ENGINE_RPM, Dcm_CfgReadEngineRpm},
    {DID_COOLANT_TEMP, Dcm_CfgReadCoolantTemp},
    {DID_GEAR, Dcm_CfgReadGear}
};

const uint8_t DcmDidConfigCount =
    (uint8_t)(sizeof(DcmDidConfigs) / sizeof(DcmDidConfigs[0]));

const DcmDidConfigType *Dcm_CfgGetDid(uint16_t did)
{
    uint8_t index;
    for (index = 0u; index < DcmDidConfigCount; index++)
    {
        if (DcmDidConfigs[index].did == did)
        {
            return &DcmDidConfigs[index];
        }
    }
    return (const DcmDidConfigType *)0;
}
