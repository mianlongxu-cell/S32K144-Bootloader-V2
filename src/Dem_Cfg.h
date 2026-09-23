#ifndef DEM_CFG_H_
#define DEM_CFG_H_

#include <stdint.h>

typedef uint8_t DemEventIdType;

typedef enum
{
    DEM_EVENT_PASSED = 0u,
    DEM_EVENT_FAILED
} DemEventStatusType;

typedef struct
{
    DemEventIdType eventId;
    uint32_t dtc;
    uint8_t failThreshold;
    uint8_t passThreshold;
    uint8_t level;
} DemEventConfigType;

#define DEM_EVENT_COOLANT_OVER_TEMP      0u
#define DEM_EVENT_VEHICLE_SPEED_ABNORMAL 1u
#define DEM_EVENT_CONTROL_TIMEOUT        2u
#define DEM_EVENT_CONFIG_COUNT           3u

extern const DemEventConfigType DemEventConfigs[];
extern const uint8_t DemEventConfigCount;

#endif /* DEM_CFG_H_ */
