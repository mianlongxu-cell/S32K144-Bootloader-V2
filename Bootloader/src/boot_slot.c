#include "boot_config.h"
#include "boot_slot.h"

static const BootSlotDescriptorType BootSlot_Descriptors[] =
{
    {
        BOOT_SLOT_ID_A,
        BOOT_SLOT_A_BASE_ADDRESS,
        BOOT_SLOT_A_BASE_ADDRESS + BOOT_SLOT_A_SIZE,
        BOOT_SLOT_A_SIZE,
        BOOT_SLOT_A_HEADER_ADDRESS,
        BOOT_SLOT_A_VECTOR_ADDRESS,
        BOOT_SLOT_A_PAYLOAD_END_ADDRESS
    },
    {
        BOOT_SLOT_ID_B,
        BOOT_SLOT_B_BASE_ADDRESS,
        BOOT_SLOT_B_BASE_ADDRESS + BOOT_SLOT_B_SIZE,
        BOOT_SLOT_B_SIZE,
        BOOT_SLOT_B_HEADER_ADDRESS,
        BOOT_SLOT_B_VECTOR_ADDRESS,
        BOOT_SLOT_B_PAYLOAD_END_ADDRESS
    }
};

bool BootSlot_GetDescriptor(BootSlotIdType id,
                            BootSlotDescriptorType *descriptor)
{
    uint32_t index;

    if (descriptor == (BootSlotDescriptorType *)0)
    {
        return false;
    }
    for (index = 0u;
         index < (sizeof(BootSlot_Descriptors) / sizeof(BootSlot_Descriptors[0]));
         index++)
    {
        if (BootSlot_Descriptors[index].id == id)
        {
            *descriptor = BootSlot_Descriptors[index];
            return true;
        }
    }
    return false;
}

bool BootSlot_IsValidId(BootSlotIdType id)
{
    return (id == BOOT_SLOT_ID_A) || (id == BOOT_SLOT_ID_B);
}

uint32_t BootSlot_GetBaseAddress(BootSlotIdType id)
{
    BootSlotDescriptorType descriptor;
    return BootSlot_GetDescriptor(id, &descriptor) ? descriptor.base_address : 0u;
}

uint32_t BootSlot_GetEndAddress(BootSlotIdType id)
{
    BootSlotDescriptorType descriptor;
    return BootSlot_GetDescriptor(id, &descriptor) ? descriptor.end_address : 0u;
}

uint32_t BootSlot_GetSize(BootSlotIdType id)
{
    BootSlotDescriptorType descriptor;
    return BootSlot_GetDescriptor(id, &descriptor) ? descriptor.size : 0u;
}

uint32_t BootSlot_GetHeaderAddress(BootSlotIdType id)
{
    BootSlotDescriptorType descriptor;
    return BootSlot_GetDescriptor(id, &descriptor) ? descriptor.header_address : 0u;
}

uint32_t BootSlot_GetVectorAddress(BootSlotIdType id)
{
    BootSlotDescriptorType descriptor;
    return BootSlot_GetDescriptor(id, &descriptor) ? descriptor.vector_address : 0u;
}

uint32_t BootSlot_GetPayloadAddress(BootSlotIdType id)
{
    return BootSlot_GetVectorAddress(id);
}

uint32_t BootSlot_GetEntryAddress(BootSlotIdType id)
{
    const uint32_t vector_address = BootSlot_GetVectorAddress(id);
    const volatile uint32_t *vectors;

    if (vector_address == 0u)
    {
        return 0u;
    }
    vectors = (const volatile uint32_t *)vector_address;
    return vectors[1];
}

bool BootSlot_ContainsAddress(BootSlotIdType id, uint32_t address)
{
    BootSlotDescriptorType descriptor;
    return BootSlot_GetDescriptor(id, &descriptor) &&
           (address >= descriptor.base_address) &&
           (address < descriptor.end_address);
}

bool BootSlot_ContainsRange(BootSlotIdType id, uint32_t address,
                            uint32_t length)
{
    BootSlotDescriptorType descriptor;

    if (!BootSlot_GetDescriptor(id, &descriptor) || (length == 0u) ||
        (address < descriptor.base_address) ||
        (address >= descriptor.end_address))
    {
        return false;
    }
    return length <= (descriptor.end_address - address);
}

BootSlotIdType BootSlot_GetOtherSlot(BootSlotIdType id)
{
    if (id == BOOT_SLOT_ID_A)
    {
        return BOOT_SLOT_ID_B;
    }
    if (id == BOOT_SLOT_ID_B)
    {
        return BOOT_SLOT_ID_A;
    }
    return BOOT_SLOT_ID_UNKNOWN;
}
