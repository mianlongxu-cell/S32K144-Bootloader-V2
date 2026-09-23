#ifndef BOOT_CONFIG_H_
#define BOOT_CONFIG_H_

#include "boot_memory_contract.h"

/* S32K144 physical memories used by the V2 design. */
#define BOOT_PFLASH_BASE_ADDRESS              0x00000000u
#define BOOT_PFLASH_SIZE                      0x00080000u
#define BOOT_PFLASH_END_ADDRESS               (BOOT_PFLASH_BASE_ADDRESS + BOOT_PFLASH_SIZE)
#define BOOT_FLEXNVM_BASE_ADDRESS             0x10000000u
#define BOOT_FLEXNVM_SIZE                     0x00010000u
#define BOOT_FLEXNVM_END_ADDRESS              (BOOT_FLEXNVM_BASE_ADDRESS + BOOT_FLEXNVM_SIZE)
#define BOOT_SRAM_BASE_ADDRESS                0x1FFF8000u
#define BOOT_SRAM_APPLICATION_END_ADDRESS     0x20006FF0u
#define BOOT_FLASH_DRIVER_RAM_ADDRESS        0x20005800u
#define BOOT_FLASH_DRIVER_RAM_SIZE           0x00000800u
#define BOOT_FLASH_DRIVER_RAM_END            (BOOT_FLASH_DRIVER_RAM_ADDRESS + BOOT_FLASH_DRIVER_RAM_SIZE)

#define BOOT_FLASH_SECTOR_SIZE                0x00001000u
#define BOOT_FLASH_PHRASE_SIZE                8u
#define BOOT_VECTOR_ALIGNMENT                 0x00000400u
#define BOOT_IMAGE_HEADER_SIZE                0x00000040u
#define BOOT_IMAGE_HEADER_REGION_SIZE         BOOT_FLASH_SECTOR_SIZE

/*
 * Current, proven V1 layout. Stage 1 deliberately keeps this path active.
 * Addresses below remain the compatibility boundary for the existing UDS
 * flasher and metadata format.
 */
#define BOOT_CURRENT_PBL_BASE_ADDRESS         0x00000000u
#define BOOT_CURRENT_PBL_END_ADDRESS          0x00008000u
#define BOOT_CURRENT_APP_VECTOR_ADDRESS       0x00008000u
#define BOOT_CURRENT_APP_END_ADDRESS          0x0007F000u
#define BOOT_CURRENT_METADATA_ADDRESS         0x0007F000u
#define BOOT_CURRENT_METADATA_END_ADDRESS     0x00080000u

/*
 * V2 planned PFlash layout. Slot A deliberately retains the current vector
 * address. Each slot stores its immutable image header in its final sector.
 * The final 32 KiB remain reserved for future shared metadata/journal work.
 */
#define BOOT_V2_COMPAT_REGION_BASE            0x00000000u
#define BOOT_V2_COMPAT_REGION_SIZE            0x00008000u

#define BOOT_SLOT_A_BASE_ADDRESS              0x00008000u
#define BOOT_SLOT_A_SIZE                      0x00038000u
#define BOOT_SLOT_A_VECTOR_ADDRESS            BOOT_SLOT_A_BASE_ADDRESS
#define BOOT_SLOT_A_HEADER_ADDRESS            BOOT_CONTRACT_SLOT_A_HEADER
#define BOOT_SLOT_A_PAYLOAD_END_ADDRESS       BOOT_SLOT_A_HEADER_ADDRESS

#define BOOT_SLOT_B_BASE_ADDRESS              0x00040000u
#define BOOT_SLOT_B_SIZE                      0x00038000u
#define BOOT_SLOT_B_VECTOR_ADDRESS            BOOT_SLOT_B_BASE_ADDRESS
#define BOOT_SLOT_B_HEADER_ADDRESS            BOOT_CONTRACT_SLOT_B_HEADER
#define BOOT_SLOT_B_PAYLOAD_END_ADDRESS       BOOT_SLOT_B_HEADER_ADDRESS

#define BOOT_V2_SHARED_METADATA_ADDRESS       0x00078000u
#define BOOT_V2_SHARED_METADATA_SIZE          0x00008000u
#define BOOT_JOURNAL_COPY0_ADDRESS            BOOT_V2_SHARED_METADATA_ADDRESS
#define BOOT_JOURNAL_COPY1_ADDRESS            (BOOT_JOURNAL_COPY0_ADDRESS + BOOT_FLASH_SECTOR_SIZE)
#define BOOT_JOURNAL_RECORD_SIZE              128u
#define BOOT_METADATA_SLOT_A_COPY0_ADDRESS     0x0007A000u
#define BOOT_METADATA_SLOT_A_COPY1_ADDRESS     0x0007B000u
#define BOOT_METADATA_SLOT_B_COPY0_ADDRESS     0x0007C000u
#define BOOT_METADATA_SLOT_B_COPY1_ADDRESS     0x0007D000u
#define BOOT_METADATA_RECORD_SIZE              80u
#define BOOT_MAX_TRIAL_ATTEMPTS                3u
#if ((BOOT_JOURNAL_COPY1_ADDRESS + BOOT_FLASH_SECTOR_SIZE) > BOOT_CURRENT_METADATA_ADDRESS)
#error "Journal overlaps legacy metadata"
#endif
#if ((BOOT_METADATA_SLOT_B_COPY1_ADDRESS + BOOT_FLASH_SECTOR_SIZE) > 0x0007E000u)
#error "Slot metadata overlaps reserved sector"
#endif

/*
 * Future FlexNVM layout. Stage 1 only links an image for review; it never
 * executes PGMPART, repartitions FlexNVM, or programs this range.
 */
#define BOOT_V2_PBL_FLEXNVM_BASE_ADDRESS      0x10000000u
#define BOOT_V2_PBL_FLEXNVM_SIZE              0x0000C000u
#define BOOT_V2_PERSISTENT_BASE_ADDRESS       0x1000C000u
#define BOOT_V2_PERSISTENT_SIZE               0x00004000u

/* Existing V1 metadata values retained for backward compatibility. */
#define BOOT_METADATA_MAGIC                   0x53394D44u
#define BOOT_METADATA_VALID                   0x56414C49u
#define BOOT_METADATA_INVALID                 0xFFFFFFFFu

#define BOOT_REQUEST_ADDRESS                  BOOT_CONTRACT_REQUEST_ADDRESS
#define BOOT_REQUEST_MAGIC                    BOOT_CONTRACT_REQUEST_MAGIC
#define BOOT_REQUEST_INVERSE                  BOOT_CONTRACT_REQUEST_MAGIC_INVERSE

#define BOOT_REQUEST_PIN                      12u
#define BOOT_LED_PIN                          16u
#define BOOT_LED_DELAY_LOOPS                  8000000u

/* Compatibility aliases used by the proven V1 implementation. */
#define BOOT_START_ADDRESS                    BOOT_CURRENT_PBL_BASE_ADDRESS
#define BOOT_END_ADDRESS                      BOOT_CURRENT_PBL_END_ADDRESS
#define APP_START_ADDRESS                     BOOT_CURRENT_APP_VECTOR_ADDRESS
#define APP_END_ADDRESS                       BOOT_CURRENT_APP_END_ADDRESS
#define METADATA_ADDRESS                      BOOT_CURRENT_METADATA_ADDRESS
#define FLASH_END_ADDRESS                     BOOT_PFLASH_END_ADDRESS
#define SRAM_START_ADDRESS                    BOOT_SRAM_BASE_ADDRESS
#define SRAM_END_ADDRESS                      BOOT_SRAM_APPLICATION_END_ADDRESS

#if ((BOOT_SLOT_A_BASE_ADDRESS + BOOT_SLOT_A_SIZE) != BOOT_SLOT_B_BASE_ADDRESS)
#error "Slot A must end where Slot B begins"
#endif

#if ((BOOT_SLOT_B_BASE_ADDRESS + BOOT_SLOT_B_SIZE) != BOOT_V2_SHARED_METADATA_ADDRESS)
#error "Slot B must end where shared metadata begins"
#endif

#if ((BOOT_V2_SHARED_METADATA_ADDRESS + BOOT_V2_SHARED_METADATA_SIZE) != BOOT_PFLASH_END_ADDRESS)
#error "V2 PFlash layout must consume exactly 512 KiB"
#endif

#if ((BOOT_V2_PBL_FLEXNVM_BASE_ADDRESS + BOOT_V2_PBL_FLEXNVM_SIZE) != BOOT_V2_PERSISTENT_BASE_ADDRESS)
#error "PBL and persistent FlexNVM regions must be contiguous"
#endif

#if ((BOOT_V2_PERSISTENT_BASE_ADDRESS + BOOT_V2_PERSISTENT_SIZE) != BOOT_FLEXNVM_END_ADDRESS)
#error "V2 FlexNVM layout must consume exactly 64 KiB"
#endif

#endif /* BOOT_CONFIG_H_ */
