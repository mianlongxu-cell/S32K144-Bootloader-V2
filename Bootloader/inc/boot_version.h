#ifndef BOOT_VERSION_H_
#define BOOT_VERSION_H_

#include <stdint.h>

/* V2 encoding: 0xMMmmppbb = major.minor.patch.build, one byte each. */
#define BOOT_VERSION_ENCODE(major, minor, patch, build) \
    ((((uint32_t)(major) & 0xFFu) << 24u) | \
     (((uint32_t)(minor) & 0xFFu) << 16u) | \
     (((uint32_t)(patch) & 0xFFu) << 8u) | \
     ((uint32_t)(build) & 0xFFu))

typedef enum
{
    BOOT_VERSION_LESS = -1,
    BOOT_VERSION_EQUAL = 0,
    BOOT_VERSION_GREATER = 1
} BootVersionCompareResultType;

BootVersionCompareResultType BootVersion_Compare(uint32_t left,
                                                 uint32_t right);
uint32_t BootVersion_FromLegacy(uint32_t legacy_version);

#endif /* BOOT_VERSION_H_ */
