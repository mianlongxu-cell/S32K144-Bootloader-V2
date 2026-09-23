#include "boot_journal_storage.h"
#include "boot_config.h"
/* Writes are implemented by the SRAM driver facade in boot_flash.c. */
bool BootJournalStorage_Read(uint32_t copy, uint8_t *data, uint32_t size)
{
    uint32_t i;
    const volatile uint8_t *source;
    if (copy > 1u || data == 0 || size > BOOT_JOURNAL_RECORD_SIZE) { return false; }
    source = (const volatile uint8_t *)(BOOT_JOURNAL_COPY0_ADDRESS + copy * BOOT_FLASH_SECTOR_SIZE);
    for (i = 0; i < size; i++) { data[i] = source[i]; }
    return true;
}
