#ifndef BOOT_TYPES_H_
#define BOOT_TYPES_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    BOOT_SLOT_ID_A = 0,
    BOOT_SLOT_ID_B = 1,
    BOOT_SLOT_ID_UNKNOWN = 0x7FFFFFFF
} BootSlotIdType;

typedef enum
{
    BOOT_IMAGE_STATE_EMPTY = 0,
    BOOT_IMAGE_STATE_DOWNLOADING,
    BOOT_IMAGE_STATE_VERIFIED,
    BOOT_IMAGE_STATE_PENDING,
    BOOT_IMAGE_STATE_TRIAL,
    BOOT_IMAGE_STATE_CONFIRMED,
    BOOT_IMAGE_STATE_INVALID
} BootImageState;

/* Fixed-width storage form used in persistent metadata. */
typedef uint32_t BootImageStateType;

typedef enum
{
    BOOT_STATUS_OK = 0,
    BOOT_STATUS_INVALID_ARGUMENT,
    BOOT_STATUS_NOT_SUPPORTED,
    BOOT_STATUS_NOT_IMPLEMENTED,
    BOOT_STATUS_NOT_FOUND,
    BOOT_STATUS_INVALID_IMAGE
} BootStatusType;

typedef enum
{
    BOOT_IMAGE_VALID = 0,
    BOOT_IMAGE_VALID_LEGACY,
    BOOT_IMAGE_ERR_INVALID_ARGUMENT,
    BOOT_IMAGE_ERR_SLOT_ID,
    BOOT_IMAGE_ERR_HEADER_ADDRESS,
    BOOT_IMAGE_ERR_EMPTY,
    BOOT_IMAGE_ERR_MAGIC,
    BOOT_IMAGE_ERR_HEADER_VERSION,
    BOOT_IMAGE_ERR_HEADER_CRC,
    BOOT_IMAGE_ERR_SIZE,
    BOOT_IMAGE_ERR_ADDRESS_RANGE,
    BOOT_IMAGE_ERR_VECTOR,
    BOOT_IMAGE_ERR_STACK_POINTER,
    BOOT_IMAGE_ERR_RESET_HANDLER,
    BOOT_IMAGE_ERR_ENTRY_MISMATCH,
    BOOT_IMAGE_ERR_IMAGE_CRC
} BootImageValidationResultType;

typedef struct
{
    uint32_t magic;
    uint32_t header_version;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t software_version;
    uint32_t build_id;
    uint32_t vector_address;
    uint32_t entry_address;
    uint32_t flags;
    uint32_t reserved[6];
    uint32_t header_crc32;
} BootImageHeaderType;

typedef struct
{
    uint32_t magic, format_version, slot_id, image_version;
    BootImageStateType state;
    uint32_t boot_attempts, successful_boots, update_counter, sequence;
    uint32_t last_reset_reason, flags, last_result;
    uint32_t failed_slot, failed_version, last_attempt_count, rollback_target;
    uint32_t reserved[2];
    uint32_t metadata_crc32, commit_marker;
} BootSlotMetadataType;

typedef struct
{
    BootSlotIdType slot_id;
    uint32_t slot_base;
    uint32_t slot_size;
    uint32_t header_address;
    uint32_t payload_address;
    uint32_t vector_address;
    uint32_t initial_msp;
    uint32_t reset_handler;
    BootImageHeaderType header;
    BootImageValidationResultType validation_result;
    bool valid;
    bool legacy_format;
} BootImageInfoType;

_Static_assert(sizeof(BootImageHeaderType) == 64u,
               "BootImageHeaderType layout must remain 64 bytes");
_Static_assert(sizeof(BootSlotMetadataType) == 80u,
               "BootSlotMetadataType must occupy ten phrases");

#endif /* BOOT_TYPES_H_ */
