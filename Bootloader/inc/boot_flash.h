#ifndef BOOT_FLASH_H_
#define BOOT_FLASH_H_

#include <stdbool.h>
#include <stdint.h>
#include "boot_types.h"

void BootFlash_Init(void);
bool BootFlash_ConfigureSlots(BootSlotIdType active, BootSlotIdType target);
uint32_t BootFlash_GetDriverVersion(void);
bool BootFlash_IsRangeValid(uint32_t address, uint32_t length);
bool BootFlash_Erase(uint32_t address, uint32_t length);
bool BootFlash_EraseMetadata(void);
bool BootFlash_Program(uint32_t address, const uint8_t *data, uint32_t length);
bool BootFlash_ProgramMetadata(const uint8_t *data, uint32_t length);
bool BootFlash_Verify(uint32_t address, const uint8_t *data, uint32_t length);
bool BootJournalStorage_Erase(uint32_t copy);
bool BootJournalStorage_Program(uint32_t copy, uint32_t offset,
                                const uint8_t *data, uint32_t size);
bool BootMetadataStorage_Erase(BootSlotIdType slot, uint32_t copy);
bool BootMetadataStorage_Program(BootSlotIdType slot, uint32_t copy,
                                 uint32_t offset, const uint8_t *data,
                                 uint32_t size);

#endif /* BOOT_FLASH_H_ */
