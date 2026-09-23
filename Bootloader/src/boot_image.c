#include <stddef.h>
#include <stdint.h>
#include "boot_check.h"
#include "boot_config.h"
#include "boot_crc32.h"
#include "boot_firmware.h"
#include "boot_image.h"
#include "boot_slot.h"
#include "boot_version.h"

_Static_assert(offsetof(BootImageHeaderType, header_crc32) ==
               BOOT_IMAGE_HEADER_CRC_LENGTH,
               "Header CRC range must cover the first 60 bytes");
_Static_assert(sizeof(BootImageHeaderType) <= BOOT_IMAGE_HEADER_REGION_SIZE,
               "Image header must fit in its reserved sector");

static bool BootImage_HeaderIsErased(const BootImageHeaderType *header)
{
    const uint32_t *words = (const uint32_t *)header;
    uint32_t index;

    for (index = 0u; index < (sizeof(*header) / sizeof(uint32_t)); index++)
    {
        if (words[index] != 0xFFFFFFFFu)
        {
            return false;
        }
    }
    return true;
}

static bool BootImage_HeaderIsZero(const BootImageHeaderType *header)
{
    const uint32_t *words = (const uint32_t *)header;
    uint32_t index;

    for (index = 0u; index < (sizeof(*header) / sizeof(uint32_t)); index++)
    {
        if (words[index] != 0u)
        {
            return false;
        }
    }
    return true;
}

static void BootImage_ClearInfo(BootImageInfoType *info)
{
    uint8_t *bytes = (uint8_t *)info;
    uint32_t index;

    for (index = 0u; index < sizeof(*info); index++)
    {
        bytes[index] = 0u;
    }
    info->slot_id = BOOT_SLOT_ID_UNKNOWN;
    info->validation_result = BOOT_IMAGE_ERR_INVALID_ARGUMENT;
}

static BootImageValidationResultType BootImage_LoadLegacySlotA(
    BootImageInfoType *info)
{
    Boot_FirmwareMetadataType metadata;
    const volatile uint32_t *vectors =
        (const volatile uint32_t *)BOOT_SLOT_A_VECTOR_ADDRESS;
    uint32_t reset_address;

    if (!BootFirmware_ReadMetadata(&metadata))
    {
        info->validation_result = BOOT_IMAGE_ERR_EMPTY;
        return BOOT_IMAGE_ERR_EMPTY;
    }
    if ((metadata.firmwareSize == 0u) ||
        (metadata.firmwareSize >
         (BOOT_SLOT_A_PAYLOAD_END_ADDRESS - BOOT_SLOT_A_VECTOR_ADDRESS)))
    {
        info->validation_result = BOOT_IMAGE_ERR_SIZE;
        return BOOT_IMAGE_ERR_SIZE;
    }
    reset_address = vectors[1] & ~1u;
    if (reset_address >= BOOT_SLOT_A_PAYLOAD_END_ADDRESS)
    {
        info->validation_result = BOOT_IMAGE_ERR_RESET_HANDLER;
        return BOOT_IMAGE_ERR_RESET_HANDLER;
    }
    if (!Boot_IsApplicationValid())
    {
        info->validation_result = BOOT_IMAGE_ERR_IMAGE_CRC;
        return BOOT_IMAGE_ERR_IMAGE_CRC;
    }

    info->header.magic = BOOT_METADATA_MAGIC;
    info->header.header_version = 0u;
    info->header.image_size = metadata.firmwareSize;
    info->header.image_crc32 = metadata.firmwareCrc;
    info->header.software_version =
        BootVersion_FromLegacy(metadata.firmwareVersion);
    info->header.build_id = 0u;
    info->header.vector_address = BOOT_SLOT_A_VECTOR_ADDRESS;
    info->header.entry_address = vectors[1];
    info->initial_msp = vectors[0];
    info->reset_handler = vectors[1];
    info->validation_result = BOOT_IMAGE_VALID_LEGACY;
    info->valid = true;
    info->legacy_format = true;
    return BOOT_IMAGE_VALID_LEGACY;
}

BootImageValidationResultType BootImage_ReadHeader(
    BootSlotIdType slot, BootImageHeaderType *header)
{
    BootSlotDescriptorType descriptor;
    const volatile uint32_t *source;
    uint32_t *destination;
    uint32_t index;

    if (header == (BootImageHeaderType *)0)
    {
        return BOOT_IMAGE_ERR_INVALID_ARGUMENT;
    }
    if (!BootSlot_GetDescriptor(slot, &descriptor))
    {
        return BOOT_IMAGE_ERR_SLOT_ID;
    }
    if (!BootSlot_ContainsRange(slot, descriptor.header_address,
                                sizeof(*header)) ||
        (descriptor.header_address < descriptor.payload_end_address))
    {
        return BOOT_IMAGE_ERR_HEADER_ADDRESS;
    }

    source = (const volatile uint32_t *)descriptor.header_address;
    destination = (uint32_t *)header;
    for (index = 0u; index < (sizeof(*header) / sizeof(uint32_t)); index++)
    {
        destination[index] = source[index];
    }
    return BOOT_IMAGE_VALID;
}

BootImageValidationResultType BootImage_ValidateHeader(
    BootSlotIdType slot, const BootImageHeaderType *header)
{
    BootSlotDescriptorType descriptor;
    uint32_t maximum_payload_size;
    uint32_t calculated_crc;

    if (header == (const BootImageHeaderType *)0)
    {
        return BOOT_IMAGE_ERR_INVALID_ARGUMENT;
    }
    if (!BootSlot_GetDescriptor(slot, &descriptor))
    {
        return BOOT_IMAGE_ERR_SLOT_ID;
    }
    if (!BootSlot_ContainsRange(slot, descriptor.header_address,
                                sizeof(*header)) ||
        (descriptor.header_address < descriptor.payload_end_address))
    {
        return BOOT_IMAGE_ERR_HEADER_ADDRESS;
    }
    if (BootImage_HeaderIsErased(header) || BootImage_HeaderIsZero(header))
    {
        return BOOT_IMAGE_ERR_EMPTY;
    }
    if (header->magic != BOOT_IMAGE_HEADER_MAGIC)
    {
        return BOOT_IMAGE_ERR_MAGIC;
    }
    if (header->header_version != BOOT_IMAGE_HEADER_VERSION)
    {
        return BOOT_IMAGE_ERR_HEADER_VERSION;
    }

    maximum_payload_size = descriptor.payload_end_address -
                           descriptor.vector_address;
    if ((header->image_size == 0u) ||
        (header->image_size > maximum_payload_size))
    {
        return BOOT_IMAGE_ERR_SIZE;
    }

    calculated_crc = BootCrc32_Calculate(
        (const uint8_t *)header, BOOT_IMAGE_HEADER_CRC_LENGTH);
    if (calculated_crc != header->header_crc32)
    {
        return BOOT_IMAGE_ERR_HEADER_CRC;
    }
    if ((header->vector_address != descriptor.vector_address) ||
        ((header->vector_address & (BOOT_VECTOR_ALIGNMENT - 1u)) != 0u) ||
        !BootSlot_ContainsRange(slot, header->vector_address,
                                2u * sizeof(uint32_t)) ||
        (header->vector_address >= descriptor.payload_end_address))
    {
        return BOOT_IMAGE_ERR_VECTOR;
    }

    /* Size was bounded before this addition, so it cannot overflow the slot. */
    if ((header->vector_address + header->image_size) >
        descriptor.payload_end_address)
    {
        return BOOT_IMAGE_ERR_ADDRESS_RANGE;
    }
    return BOOT_IMAGE_VALID;
}

BootImageValidationResultType BootImage_ValidatePayload(
    BootSlotIdType slot, const BootImageHeaderType *header,
    uint32_t *initial_msp, uint32_t *reset_handler)
{
    BootSlotDescriptorType descriptor;
    const volatile uint32_t *vectors;
    uint32_t msp;
    uint32_t reset;
    uint32_t reset_address;
    uint32_t payload_end;
    uint32_t calculated_crc;

    if ((header == (const BootImageHeaderType *)0) ||
        (initial_msp == (uint32_t *)0) ||
        (reset_handler == (uint32_t *)0))
    {
        return BOOT_IMAGE_ERR_INVALID_ARGUMENT;
    }
    if (!BootSlot_GetDescriptor(slot, &descriptor))
    {
        return BOOT_IMAGE_ERR_SLOT_ID;
    }
    if ((header->image_size == 0u) ||
        (header->image_size >
         (descriptor.payload_end_address - descriptor.vector_address)) ||
        (header->vector_address != descriptor.vector_address) ||
        !BootSlot_ContainsRange(slot, header->vector_address,
                                2u * sizeof(uint32_t)))
    {
        return BOOT_IMAGE_ERR_ADDRESS_RANGE;
    }

    payload_end = header->vector_address + header->image_size;
    if (payload_end > descriptor.payload_end_address)
    {
        return BOOT_IMAGE_ERR_ADDRESS_RANGE;
    }

    vectors = (const volatile uint32_t *)header->vector_address;
    msp = vectors[0];
    reset = vectors[1];
    reset_address = reset & ~1u;
    *initial_msp = msp;
    *reset_handler = reset;

    if ((msp == 0u) || (msp == 0xFFFFFFFFu) ||
        (msp < BOOT_SRAM_BASE_ADDRESS) ||
        (msp > BOOT_SRAM_APPLICATION_END_ADDRESS) ||
        ((msp & 0x7u) != 0u))
    {
        return BOOT_IMAGE_ERR_STACK_POINTER;
    }
    if ((reset == 0u) || (reset == 0xFFFFFFFFu) ||
        ((reset & 1u) == 0u) ||
        (reset_address < header->vector_address) ||
        (reset_address >= payload_end))
    {
        return BOOT_IMAGE_ERR_RESET_HANDLER;
    }
    if (header->entry_address != reset)
    {
        return BOOT_IMAGE_ERR_ENTRY_MISMATCH;
    }

    calculated_crc = BootCrc32_Calculate(
        (const uint8_t *)header->vector_address, header->image_size);
    if (calculated_crc != header->image_crc32)
    {
        return BOOT_IMAGE_ERR_IMAGE_CRC;
    }
    return BOOT_IMAGE_VALID;
}

BootImageValidationResultType BootImage_Validate(
    BootSlotIdType slot, BootImageInfoType *info)
{
    BootImageValidationResultType result;

    if (info == (BootImageInfoType *)0)
    {
        return BOOT_IMAGE_ERR_INVALID_ARGUMENT;
    }
    result = BootImage_ValidateHeader(slot, &info->header);
    if (result == BOOT_IMAGE_VALID)
    {
        result = BootImage_ValidatePayload(slot, &info->header,
                                           &info->initial_msp,
                                           &info->reset_handler);
    }
    info->validation_result = result;
    info->valid = (result == BOOT_IMAGE_VALID);
    return result;
}

BootImageValidationResultType BootImage_LoadInfo(
    BootSlotIdType slot, BootImageInfoType *info)
{
    BootSlotDescriptorType descriptor;
    BootImageValidationResultType result;

    if (info == (BootImageInfoType *)0)
    {
        return BOOT_IMAGE_ERR_INVALID_ARGUMENT;
    }
    BootImage_ClearInfo(info);
    if (!BootSlot_GetDescriptor(slot, &descriptor))
    {
        info->validation_result = BOOT_IMAGE_ERR_SLOT_ID;
        return info->validation_result;
    }

    info->slot_id = slot;
    info->slot_base = descriptor.base_address;
    info->slot_size = descriptor.size;
    info->header_address = descriptor.header_address;
    info->payload_address = descriptor.vector_address;
    info->vector_address = descriptor.vector_address;

    result = BootImage_ReadHeader(slot, &info->header);
    if (result != BOOT_IMAGE_VALID)
    {
        info->validation_result = result;
        return result;
    }

    /* Only a completely erased V2 header may use the proven V1 Slot A path. */
    if ((slot == BOOT_SLOT_ID_A) && BootImage_HeaderIsErased(&info->header))
    {
        return BootImage_LoadLegacySlotA(info);
    }
    return BootImage_Validate(slot, info);
}

uint32_t BootImage_GetVersion(const BootImageInfoType *info)
{
    return (info == (const BootImageInfoType *)0) ?
        0u : info->header.software_version;
}

bool BootImage_IsBootable(BootSlotIdType slot)
{
    BootImageInfoType info;
    const BootImageValidationResultType result =
        BootImage_LoadInfo(slot, &info);
    return (result == BOOT_IMAGE_VALID) ||
           (result == BOOT_IMAGE_VALID_LEGACY);
}
