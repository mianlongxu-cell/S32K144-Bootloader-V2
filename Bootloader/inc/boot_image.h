#ifndef BOOT_IMAGE_H_
#define BOOT_IMAGE_H_

#include <stdbool.h>
#include <stdint.h>
#include "boot_types.h"

#define BOOT_IMAGE_HEADER_MAGIC          0x42493256u
#define BOOT_IMAGE_HEADER_VERSION        1u
#define BOOT_IMAGE_HEADER_CRC_LENGTH     60u

BootImageValidationResultType BootImage_ReadHeader(
    BootSlotIdType slot, BootImageHeaderType *header);
BootImageValidationResultType BootImage_ValidateHeader(
    BootSlotIdType slot, const BootImageHeaderType *header);
BootImageValidationResultType BootImage_ValidatePayload(
    BootSlotIdType slot, const BootImageHeaderType *header,
    uint32_t *initial_msp, uint32_t *reset_handler);
BootImageValidationResultType BootImage_Validate(
    BootSlotIdType slot, BootImageInfoType *info);
BootImageValidationResultType BootImage_LoadInfo(
    BootSlotIdType slot, BootImageInfoType *info);
uint32_t BootImage_GetVersion(const BootImageInfoType *info);
bool BootImage_IsBootable(BootSlotIdType slot);

#endif /* BOOT_IMAGE_H_ */
