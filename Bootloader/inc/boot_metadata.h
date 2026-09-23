#ifndef BOOT_METADATA_H
#define BOOT_METADATA_H
#include <stddef.h>
#include "boot_types.h"
#define BOOT_SLOT_METADATA_MAGIC 0x4D355642u
#define BOOT_SLOT_METADATA_FORMAT 1u
#define BOOT_SLOT_METADATA_COMMIT_MARKER 0x534C4F54u
typedef enum { BOOT_METADATA_STATUS_VALID, BOOT_METADATA_STATUS_EMPTY,
    BOOT_METADATA_STATUS_INVALID } BootMetadataStatus;
typedef enum { BOOT_LIFECYCLE_NONE=0, BOOT_LIFECYCLE_TRIAL_STARTED,
    BOOT_LIFECYCLE_TRIAL_FAILED, BOOT_LIFECYCLE_CONFIRMED,
    BOOT_LIFECYCLE_ROLLBACK_OCCURRED, BOOT_LIFECYCLE_NO_CONFIRMED_IMAGE,
    BOOT_LIFECYCLE_METADATA_ERROR } BootLifecycleResult;
typedef enum { BOOT_RESET_UNKNOWN=0, BOOT_RESET_POWER_ON, BOOT_RESET_WATCHDOG,
    BOOT_RESET_SOFTWARE, BOOT_RESET_EXTERNAL, BOOT_RESET_FAULT,
    BOOT_RESET_LOW_VOLTAGE, BOOT_RESET_CLOCK, BOOT_RESET_DEBUG } BootResetReason;
_Static_assert(offsetof(BootSlotMetadataType, metadata_crc32)==72u,
               "Metadata CRC offset");
bool BootMetadata_Validate(const BootSlotMetadataType *record, BootSlotIdType slot);
bool BootMetadata_SequenceNewer(uint32_t a, uint32_t b);
BootMetadataStatus BootMetadata_Load(BootSlotIdType slot, BootSlotMetadataType *record);
bool BootMetadata_Initialize(BootSlotIdType slot, BootSlotMetadataType *record);
bool BootMetadata_Commit(BootSlotIdType slot, BootSlotMetadataType *record);
bool BootMetadata_InstallVerified(BootSlotIdType slot, uint32_t image_version,
                                  uint32_t update_counter, uint32_t reset_reason);
bool BootMetadata_SetState(BootSlotIdType slot, BootImageState state,
                           uint32_t reset_reason, uint32_t result);
bool BootMetadata_IncrementAttempt(BootSlotIdType slot, uint32_t reset_reason);
BootImageState BootMetadata_GetState(BootSlotIdType slot);
#endif
