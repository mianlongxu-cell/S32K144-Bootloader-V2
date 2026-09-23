#include "boot_version.h"

BootVersionCompareResultType BootVersion_Compare(uint32_t left,
                                                 uint32_t right)
{
    uint32_t shift;

    for (shift = 24u;; shift -= 8u)
    {
        const uint32_t left_part = (left >> shift) & 0xFFu;
        const uint32_t right_part = (right >> shift) & 0xFFu;
        if (left_part < right_part)
        {
            return BOOT_VERSION_LESS;
        }
        if (left_part > right_part)
        {
            return BOOT_VERSION_GREATER;
        }
        if (shift == 0u)
        {
            break;
        }
    }
    return BOOT_VERSION_EQUAL;
}

uint32_t BootVersion_FromLegacy(uint32_t legacy_version)
{
    const uint32_t major = (legacy_version >> 16u) & 0xFFu;
    const uint32_t minor = (legacy_version >> 8u) & 0xFFu;
    const uint32_t patch = legacy_version & 0xFFu;
    return BOOT_VERSION_ENCODE(major, minor, patch, 0u);
}
