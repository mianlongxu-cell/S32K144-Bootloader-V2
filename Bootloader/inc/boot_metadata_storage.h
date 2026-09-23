#ifndef BOOT_METADATA_STORAGE_H
#define BOOT_METADATA_STORAGE_H
#include "boot_types.h"
bool BootMetadataStorage_Read(BootSlotIdType slot, uint32_t copy, uint8_t *data, uint32_t size);
bool BootMetadataStorage_Erase(BootSlotIdType slot, uint32_t copy);
bool BootMetadataStorage_Program(BootSlotIdType slot, uint32_t copy, uint32_t offset,
                                 const uint8_t *data, uint32_t size);
#endif
