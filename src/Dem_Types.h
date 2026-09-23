#ifndef DEM_TYPES_H_
#define DEM_TYPES_H_

#include <stdint.h>

typedef struct
{
    uint32_t code;
    uint8_t active;
    uint8_t stored;
    uint8_t level;
    uint8_t occurrence_counter;
    uint8_t status;
} DtcInfo_t;

#endif /* DEM_TYPES_H_ */
