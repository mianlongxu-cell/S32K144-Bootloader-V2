#ifndef BOOT_PROGRAMMING_H
#define BOOT_PROGRAMMING_H
#include "boot_types.h"
#include "boot_protocol_cfg.h"
typedef struct {
    bool session_active, security_unlocked, download_active;
    bool erase_complete, transfer_complete, image_verified;
    BootSlotIdType active_slot, target_slot;
    uint32_t active_version, target_address, target_size, received_size;
    uint32_t programmed_size;
    uint8_t expected_block_sequence_counter;
    uint16_t max_block_length;
    uint8_t buffered_size;
    BootImageValidationResultType validation_result;
    BootImageHeaderType staged_header;
} BootProgrammingContext;
extern BootProgrammingContext BootProgramming_Context;
extern uint8_t BootProgramming_TransferBuffer[8];
typedef bool (*BootProgrammingPending)(void);
void BootProgramming_Init(BootSlotIdType active, uint32_t version);
void BootProgramming_SetAccess(bool session, bool unlocked);
void BootProgramming_Abort(void);
uint8_t BootProgramming_Erase(uint32_t address, uint32_t size, BootProgrammingPending pending);
uint8_t BootProgramming_Download(uint32_t address, uint32_t size);
uint8_t BootProgramming_Transfer(uint8_t sequence, const uint8_t *data, uint32_t size);
uint8_t BootProgramming_Exit(void);
uint8_t BootProgramming_Verify(BootProgrammingPending pending);
bool BootProgramming_CanReset(void);
#endif
