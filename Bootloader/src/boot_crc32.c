#include "boot_crc32.h"
#include "device_registers.h"

#define BOOT_CRC32_WDOG_REFRESH_INTERVAL 1024u

static void BootCrc32_RefreshWatchdog(uint32_t index)
{
    if (((index & (BOOT_CRC32_WDOG_REFRESH_INTERVAL - 1u)) == 0u) &&
        ((IP_WDOG->CS & (WDOG_CS_EN_MASK | WDOG_CS_CMD32EN_MASK)) ==
         (WDOG_CS_EN_MASK | WDOG_CS_CMD32EN_MASK)))
    {
        IP_WDOG->CNT = 0xB480A602u;
    }
}

uint32_t BootCrc32_Calculate(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t index;
    uint8_t bit;

    for (index = 0u; index < length; index++)
    {
        BootCrc32_RefreshWatchdog(index);
        crc ^= data[index];
        for (bit = 0u; bit < 8u; bit++)
        {
            crc = ((crc & 1u) != 0u) ?
                ((crc >> 1u) ^ 0xEDB88320u) : (crc >> 1u);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}
