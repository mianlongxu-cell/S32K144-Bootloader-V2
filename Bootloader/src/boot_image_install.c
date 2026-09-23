#include "boot_image_install.h"
#include "boot_image.h"
#include "boot_flash.h"
#include "boot_slot.h"
#include "boot_config.h"
#include "boot_version.h"
bool BootImageInstall_ValidateAndPublish(BootSlotIdType active, BootSlotIdType target,
                                       const BootImageHeaderType *header)
{
    BootImageInfoType candidate, old;
    uint32_t address = BootSlot_GetHeaderAddress(target);
    if (active == target || !BootSlot_IsValidId(target)) { return false; }
    candidate.header = *header;
    if (BootImage_Validate(target, &candidate) != BOOT_IMAGE_VALID) { return false; }
    if (BootSlot_IsValidId(active)) {
        (void)BootImage_LoadInfo(active, &old);
        if (old.valid && BootVersion_Compare(header->software_version, old.header.software_version) <= 0) { return false; }
    }
    /* Already-published valid header: no repeat erase on every pending boot. */
    if (BootImage_LoadInfo(target, &candidate) == BOOT_IMAGE_VALID &&
        candidate.header.header_crc32 == header->header_crc32 &&
        candidate.header.software_version == header->software_version &&
        candidate.header.image_crc32 == header->image_crc32) { return true; }
    if (!BootFlash_ConfigureSlots(active, target) || !BootFlash_Erase(address, BOOT_FLASH_SECTOR_SIZE) ||
        !BootFlash_Program(address, (const uint8_t *)header, sizeof(*header))) { return false; }
    return BootImage_LoadInfo(target, &candidate) == BOOT_IMAGE_VALID;
}
