#ifndef BOOT_SLOT_H_
#define BOOT_SLOT_H_

#include <stdbool.h>
#include <stdint.h>
#include "boot_types.h"

typedef struct
{
    BootSlotIdType id;
    uint32_t base_address;
    uint32_t end_address;
    uint32_t size;
    uint32_t header_address;
    uint32_t vector_address;
    uint32_t payload_end_address;
} BootSlotDescriptorType;

bool BootSlot_GetDescriptor(BootSlotIdType id,
                            BootSlotDescriptorType *descriptor);
bool BootSlot_IsValidId(BootSlotIdType id);
uint32_t BootSlot_GetBaseAddress(BootSlotIdType id);
uint32_t BootSlot_GetEndAddress(BootSlotIdType id);
uint32_t BootSlot_GetSize(BootSlotIdType id);
uint32_t BootSlot_GetHeaderAddress(BootSlotIdType id);
uint32_t BootSlot_GetVectorAddress(BootSlotIdType id);
uint32_t BootSlot_GetPayloadAddress(BootSlotIdType id);
uint32_t BootSlot_GetEntryAddress(BootSlotIdType id);
bool BootSlot_ContainsAddress(BootSlotIdType id, uint32_t address);
bool BootSlot_ContainsRange(BootSlotIdType id, uint32_t address,
                            uint32_t length);
BootSlotIdType BootSlot_GetOtherSlot(BootSlotIdType id);

#endif /* BOOT_SLOT_H_ */
