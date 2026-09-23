#include "Dem_Cfg.h"

const DemEventConfigType DemEventConfigs[] =//故障配置表
{
    {DEM_EVENT_COOLANT_OVER_TEMP, 0x000101u, 3u, 3u, 3u},
    {DEM_EVENT_VEHICLE_SPEED_ABNORMAL, 0x000201u, 3u, 3u, 2u},
    /* The 5 s heartbeat timer already debounces this communication event. */
    {DEM_EVENT_CONTROL_TIMEOUT, 0x000301u, 1u, 1u, 2u}
};

const uint8_t DemEventConfigCount =
    (uint8_t)(sizeof(DemEventConfigs) / sizeof(DemEventConfigs[0]));

typedef char Dem_ConfigCountMustMatchRuntime[
    ((sizeof(DemEventConfigs) / sizeof(DemEventConfigs[0])) ==
     DEM_EVENT_CONFIG_COUNT) ? 1 : -1];
