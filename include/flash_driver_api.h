#ifndef FLASH_DRIVER_API_H
#define FLASH_DRIVER_API_H
#include <stdint.h>
#include <stdbool.h>
/* Fixed ABI, independently linked SRAM code. Layout is owned by boot_config. */
#define FLASH_DRIVER_MAGIC 0x46445233u
#define FLASH_DRIVER_API_VERSION 3u
typedef struct {
    uint32_t magic;
    uint32_t api_version;
    bool (*init)(uint32_t active_slot, uint32_t target_slot);
    bool (*erase)(uint32_t address, uint32_t size);
    bool (*program)(uint32_t address, const uint8_t *data, uint32_t size);
    bool (*verify)(uint32_t address, const uint8_t *data, uint32_t size);
    uint32_t (*get_version)(void);
    bool (*journal_erase)(uint32_t copy);
    bool (*journal_program)(uint32_t copy, uint32_t offset, const uint8_t *data, uint32_t size);
    bool (*metadata_erase)(uint32_t slot, uint32_t copy);
    bool (*metadata_program)(uint32_t slot, uint32_t copy, uint32_t offset,
                             const uint8_t *data, uint32_t size);
} FlashDriverApi;
#endif
