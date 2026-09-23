#ifndef BOOT_FIRMWARE_H_
#define BOOT_FIRMWARE_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t magic;
    uint32_t firmwareSize;
    uint32_t firmwareCrc;
    uint32_t firmwareVersion;
    uint32_t validFlag;
    uint32_t reserved;
} Boot_FirmwareMetadataType;

bool BootFirmware_ReadMetadata(Boot_FirmwareMetadataType *metadata);
bool BootFirmware_Invalidate(void);
bool BootFirmware_MarkValid(uint32_t size, uint32_t crc, uint32_t version);
bool BootFirmware_Verify(uint32_t size, uint32_t expected_crc,
                         uint32_t *calculated_crc);

#endif /* BOOT_FIRMWARE_H_ */
