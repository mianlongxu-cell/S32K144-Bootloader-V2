#include <stddef.h>
#include "boot_config.h"
#include "boot_flash.h"
#include "boot_slot.h"
#include "flash_driver_api.h"
extern const uint8_t __flash_driver_blob_start[], __flash_driver_blob_end[];
static bool ready;
static uint32_t image_length;
static BootSlotIdType active_slot = BOOT_SLOT_ID_UNKNOWN;
static BootSlotIdType target_slot = BOOT_SLOT_ID_UNKNOWN;
static const FlashDriverApi *const api = (const FlashDriverApi *)BOOT_FLASH_DRIVER_RAM_ADDRESS;
static bool function_valid(uintptr_t pointer)
{
    uint32_t address = (uint32_t)(pointer & ~(uintptr_t)1u);
    return (pointer & 1u) != 0u &&
           address >= BOOT_FLASH_DRIVER_RAM_ADDRESS + sizeof(FlashDriverApi) &&
           address < BOOT_FLASH_DRIVER_RAM_ADDRESS + image_length;
}
static bool api_valid(void)
{
    return ready && api->magic == FLASH_DRIVER_MAGIC &&
           api->api_version == FLASH_DRIVER_API_VERSION &&
           function_valid((uintptr_t)api->init) && function_valid((uintptr_t)api->erase) &&
           function_valid((uintptr_t)api->program) && function_valid((uintptr_t)api->verify) &&
           function_valid((uintptr_t)api->get_version) &&
           function_valid((uintptr_t)api->journal_erase) &&
           function_valid((uintptr_t)api->journal_program) &&
           function_valid((uintptr_t)api->metadata_erase) &&
           function_valid((uintptr_t)api->metadata_program);
}
void BootFlash_Init(void)
{
    uint32_t i;
    volatile uint8_t *ram = (volatile uint8_t *)BOOT_FLASH_DRIVER_RAM_ADDRESS;
    ready = false;
    active_slot = BOOT_SLOT_ID_UNKNOWN;
    target_slot = BOOT_SLOT_ID_UNKNOWN;
    image_length = (uint32_t)(__flash_driver_blob_end - __flash_driver_blob_start);
    if (image_length < sizeof(FlashDriverApi) || image_length > BOOT_FLASH_DRIVER_RAM_SIZE) { return; }
    for (i = 0; i < BOOT_FLASH_DRIVER_RAM_SIZE; i++) { ram[i] = 0u; }
    for (i = 0; i < image_length; i++) { ram[i] = __flash_driver_blob_start[i]; }
    for (i = 0; i < image_length; i++) {
        if (ram[i] != __flash_driver_blob_start[i]) { return; }
    }
    __asm volatile ("dsb\nisb" : : : "memory");
    ready = true;
    ready = api_valid();
    if (ready) { ready = api->init((uint32_t)BOOT_SLOT_ID_UNKNOWN, 0u); }
}
bool BootFlash_ConfigureSlots(BootSlotIdType active, BootSlotIdType target)
{
    active_slot = BOOT_SLOT_ID_UNKNOWN;
    target_slot = BOOT_SLOT_ID_UNKNOWN;
    if (!api_valid() || !BootSlot_IsValidId(target) || active == target ||
        (!BootSlot_IsValidId(active) && active != BOOT_SLOT_ID_UNKNOWN)) { return false; }
    if (!api->init((uint32_t)active, (uint32_t)target)) { return false; }
    active_slot = active;
    target_slot = target;
    return true;
}
bool BootFlash_IsRangeValid(uint32_t address, uint32_t size)
{
    return target_slot != active_slot && BootSlot_IsValidId(target_slot) &&
           BootSlot_ContainsRange(target_slot, address, size);
}
bool BootFlash_Erase(uint32_t address, uint32_t size)
{
    return api_valid() && BootFlash_IsRangeValid(address, size) &&
           size == BOOT_FLASH_SECTOR_SIZE &&
           (address & (BOOT_FLASH_SECTOR_SIZE - 1u)) == 0u && api->erase(address, size);
}
bool BootFlash_Program(uint32_t address, const uint8_t *data, uint32_t size)
{
    return api_valid() && BootFlash_IsRangeValid(address, size) && data != NULL &&
           (address & (BOOT_FLASH_PHRASE_SIZE - 1u)) == 0u &&
           (size & (BOOT_FLASH_PHRASE_SIZE - 1u)) == 0u && api->program(address, data, size);
}
bool BootFlash_Verify(uint32_t address, const uint8_t *data, uint32_t size)
{
    return api_valid() && BootFlash_IsRangeValid(address, size) &&
           data != NULL && api->verify(address, data, size);
}
uint32_t BootFlash_GetDriverVersion(void) { return api_valid() ? api->get_version() : 0u; }
bool BootJournalStorage_Erase(uint32_t copy)
{ return api_valid() && copy < 2u && api->journal_erase(copy); }
bool BootJournalStorage_Program(uint32_t copy, uint32_t offset, const uint8_t *data, uint32_t size)
{ return api_valid() && copy < 2u && api->journal_program(copy, offset, data, size); }
bool BootMetadataStorage_Erase(BootSlotIdType slot, uint32_t copy)
{ return api_valid() && BootSlot_IsValidId(slot) && copy < 2u && api->metadata_erase((uint32_t)slot, copy); }
bool BootMetadataStorage_Program(BootSlotIdType slot, uint32_t copy, uint32_t offset,
                                 const uint8_t *data, uint32_t size)
{ return api_valid() && BootSlot_IsValidId(slot) && copy < 2u &&
         api->metadata_program((uint32_t)slot, copy, offset, data, size); }
/* Legacy metadata remains read-only; only the two dedicated Journal sectors write. */
bool BootFlash_EraseMetadata(void) { return false; }
bool BootFlash_ProgramMetadata(const uint8_t *data, uint32_t length)
{ (void)data; (void)length; return false; }
