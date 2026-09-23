#include "boot_metadata_storage.h"
#include "boot_config.h"
#include "boot_slot.h"
static uint32_t address(BootSlotIdType slot, uint32_t copy)
{ return BOOT_METADATA_SLOT_A_COPY0_ADDRESS + ((uint32_t)slot * 2u + copy) * BOOT_FLASH_SECTOR_SIZE; }
bool BootMetadataStorage_Read(BootSlotIdType slot, uint32_t copy, uint8_t *data, uint32_t size)
{
    const volatile uint8_t *source;
    uint32_t index;
    if (!BootSlot_IsValidId(slot) || copy > 1u || data == 0 || size > BOOT_METADATA_RECORD_SIZE) { return false; }
    source = (const volatile uint8_t *)address(slot, copy);
    for (index=0; index<size; index++) { data[index]=source[index]; }
    return true;
}
