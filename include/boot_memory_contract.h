#ifndef BOOT_MEMORY_CONTRACT_H_
#define BOOT_MEMORY_CONTRACT_H_

/*
 * Shared, versioned memory contract between the Application and Bootloader.
 * Keep this header free of Bootloader implementation details.
 */
#define BOOT_CONTRACT_VERSION                 2u

#define BOOT_CONTRACT_REQUEST_ADDRESS         0x20006FF0u
#define BOOT_CONTRACT_REQUEST_MAGIC           0x424F4F54u
#define BOOT_CONTRACT_REQUEST_MAGIC_INVERSE   (~BOOT_CONTRACT_REQUEST_MAGIC)
#define BOOT_CONTRACT_CONFIRM_MAGIC           0x434F4E46u
#define BOOT_CONTRACT_STATUS_MAGIC            0x53544154u
#define BOOT_CONTRACT_NORMALIZE_MAGIC         0x4E4F524Du
#define BOOT_CONTRACT_FAULT_MAGIC             0x4641554Cu

/* Retained four-word record: magic, ~magic, data, ~data. */
#define BOOT_CONTRACT_RECORD_WORDS            4u
#define BOOT_CONTRACT_STATUS_SLOT_MASK         0x000000FFu
#define BOOT_CONTRACT_STATUS_STATE_SHIFT       8u
#define BOOT_CONTRACT_STATUS_STATE_MASK        0x0000FF00u
#define BOOT_CONTRACT_STATUS_RESET_SHIFT       16u
#define BOOT_CONTRACT_STATUS_RESET_MASK        0x00FF0000u

#define BOOT_CONTRACT_SLOT_A_ID               0u
#define BOOT_CONTRACT_SLOT_B_ID               1u
#define BOOT_CONTRACT_SLOT_UNKNOWN_ID         0xFFFFFFFFu
#define BOOT_CONTRACT_SLOT_A_HEADER           0x0003F000u
#define BOOT_CONTRACT_SLOT_B_HEADER           0x00077000u
#define BOOT_CONTRACT_IMAGE_MAGIC             0x42493256u

#endif /* BOOT_MEMORY_CONTRACT_H_ */
