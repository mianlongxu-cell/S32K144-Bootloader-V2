#include "boot_config.h"
#include "boot_crc32.h"
#include "boot_firmware.h"
#include "boot_flash.h"

bool BootFirmware_ReadMetadata(Boot_FirmwareMetadataType *metadata)
{
    const volatile Boot_FirmwareMetadataType *stored =
        (const volatile Boot_FirmwareMetadataType *)METADATA_ADDRESS;

    if (metadata == (Boot_FirmwareMetadataType *)0)
    {
        return false;
    }
    metadata->magic = stored->magic;
    metadata->firmwareSize = stored->firmwareSize;
    metadata->firmwareCrc = stored->firmwareCrc;
    metadata->firmwareVersion = stored->firmwareVersion;
    metadata->validFlag = stored->validFlag;
    metadata->reserved = stored->reserved;
    return true;
}

bool BootFirmware_Invalidate(void)
{
    /* Erasing metadata before Application erase makes a power loss fail-safe:
     * Boot_IsApplicationValid() cannot accept a partially written image. */
    return BootFlash_EraseMetadata();
}

bool BootFirmware_MarkValid(uint32_t size, uint32_t crc, uint32_t version)
{
    Boot_FirmwareMetadataType metadata;

    if ((size == 0u) || (size > (APP_END_ADDRESS - APP_START_ADDRESS)))
    {
        return false;
    }
    metadata.magic = BOOT_METADATA_MAGIC;
    metadata.firmwareSize = size;
    metadata.firmwareCrc = crc;
    metadata.firmwareVersion = version;
    /* The valid flag is written together with the verified size/CRC record and
     * is never programmed during an active transfer. */
    metadata.validFlag = BOOT_METADATA_VALID;
    metadata.reserved = 0xFFFFFFFFu;
    return BootFlash_ProgramMetadata((const uint8_t *)&metadata,
                                     sizeof(metadata));
}

bool BootFirmware_Verify(uint32_t size, uint32_t expected_crc,
                         uint32_t *calculated_crc)
{
    uint32_t crc;

    if ((size == 0u) || (size > (APP_END_ADDRESS - APP_START_ADDRESS)))
    {
        return false;
    }
    crc = BootCrc32_Calculate((const uint8_t *)APP_START_ADDRESS, size);
    if (calculated_crc != (uint32_t *)0)
    {
        *calculated_crc = crc;
    }
    return crc == expected_crc;
}
