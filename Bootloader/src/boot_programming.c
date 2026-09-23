#include <stddef.h>
#include "boot_programming.h"
#include "boot_config.h"
#include "boot_slot.h"
#include "boot_flash.h"
#include "boot_image.h"
#include "boot_version.h"
#include "boot_update.h"
#include "boot_fault.h"
#include "boot_lifecycle.h"

BootProgrammingContext BootProgramming_Context;
__attribute__((aligned(8))) uint8_t BootProgramming_TransferBuffer[BOOT_FLASH_PHRASE_SIZE];
#define C BootProgramming_Context

static uint8_t access_check(void)
{
    if (!C.session_active) { return 0x22u; }
    if (!C.security_unlocked) { return 0x33u; }
    if (!BootUpdate_CanContinue()) { return 0x72u; }
    return 0u;
}
static bool exact_target(uint32_t address, uint32_t size)
{
    return BootSlot_IsValidId(C.target_slot) && C.target_slot != C.active_slot &&
           address == BootSlot_GetBaseAddress(C.target_slot) &&
           size == BootSlot_GetSize(C.target_slot) &&
           BootSlot_ContainsRange(C.target_slot, address, size);
}
void BootProgramming_Abort(void)
{
    C.download_active = false;
    C.erase_complete = false;
    C.transfer_complete = false;
    C.image_verified = false;
    C.received_size = 0u;
    C.programmed_size = 0u;
    C.buffered_size = 0u;
    C.expected_block_sequence_counter = 1u;
}
void BootProgramming_Init(BootSlotIdType active, uint32_t version)
{
    uint32_t i;
    uint8_t *bytes = (uint8_t *)&C;
    for (i = 0; i < sizeof(C); i++) { bytes[i] = 0u; }
    C.active_slot = active;
    C.active_version = version;
    C.target_slot = BootSlot_GetOtherSlot(active);
    /* Recovery with no valid image has a deterministic target, Slot A. */
    if (active == BOOT_SLOT_ID_UNKNOWN) { C.target_slot = BOOT_SLOT_ID_A; }
    C.max_block_length = BOOT_DOWNLOAD_MAX_BLOCK_LENGTH;
    C.validation_result = BOOT_IMAGE_ERR_EMPTY;
    BootProgramming_Abort();
}
void BootProgramming_SetAccess(bool session, bool unlocked)
{
    C.session_active = session;
    C.security_unlocked = unlocked;
}
uint8_t BootProgramming_Erase(uint32_t address, uint32_t size, BootProgrammingPending pending)
{
    uint8_t result = access_check();
    uint32_t current, header;
    uint8_t invalid_marker[BOOT_FLASH_PHRASE_SIZE] = {0u};
    if (result != 0u) { return result; }
    if (C.download_active) { return 0x24u; }
    if (!exact_target(address, size)) { return 0x31u; }
    if (!BootLifecycle_CanStartUpdate(C.active_slot,C.target_slot)) { return 0x22u; }
    BootProgramming_Abort();
    if (!BootFlash_ConfigureSlots(C.active_slot, C.target_slot)) { return 0x72u; }
    header = BootSlot_GetHeaderAddress(C.target_slot);
    if (pending != NULL && !pending()) { return 0x72u; }
    if (!BootUpdate_Begin(C.active_slot, C.target_slot, size) ||
        !BootUpdate_SetState(BOOT_UPDATE_ERASING)) { return 0x72u; }
    if (!BootFlash_Erase(header, BOOT_FLASH_SECTOR_SIZE) ||
        !BootFlash_Program(header, invalid_marker, sizeof(invalid_marker))) {
        (void)BootUpdate_Abort(BOOT_UPDATE_ERASE_FAILED); return 0x72u;
    }
    /* Header stays invalid during an interrupted transfer, including legacy
     * Slot A fallback. Header is erased/replaced only after full validation. */
    for (current = address; current < address + size; current += BOOT_FLASH_SECTOR_SIZE) {
        if (current == header) { continue; }
        if (pending != NULL && !pending()) { return 0x72u; }
        if (!BootFlash_Erase(current, BOOT_FLASH_SECTOR_SIZE)) {
            (void)BootUpdate_Abort(BOOT_UPDATE_ERASE_FAILED); return 0x72u;
        }
        BootFault_Hit(BOOT_FAULT_DURING_ERASE);
    }
    if (!BootUpdate_SetState(BOOT_UPDATE_DOWNLOAD_READY)) { return 0x72u; }
    BootFault_Hit(BOOT_FAULT_AFTER_ERASE);
    C.target_address = address;
    C.target_size = size;
    C.erase_complete = true;
    return 0u;
}
uint8_t BootProgramming_Download(uint32_t address, uint32_t size)
{
    uint8_t result = access_check();
    uint32_t i;
    if (result != 0u) { return result; }
    if (!exact_target(address, size)) { return 0x31u; }
    if (!C.erase_complete || C.download_active || C.transfer_complete ||
        C.received_size != 0u || address != C.target_address || size != C.target_size) { return 0x24u; }
    if (!BootUpdate_SetState(BOOT_UPDATE_PROGRAMMING)) { return 0x72u; }
    C.download_active = true;
    BootFault_Hit(BOOT_FAULT_AFTER_DOWNLOAD_START);
    C.image_verified = false;
    C.validation_result = BOOT_IMAGE_ERR_EMPTY;
    for (i = 0; i < sizeof(C.staged_header); i++) {
        ((uint8_t *)&C.staged_header)[i] = 0xFFu;
    }
    return 0u;
}
static bool flush_phrase(void)
{
    uint32_t address = C.target_address + C.programmed_size;
    uint32_t header = BootSlot_GetHeaderAddress(C.target_slot);
    uint32_t i;
    if (!BootSlot_ContainsRange(C.target_slot, address, BOOT_FLASH_PHRASE_SIZE) ||
        C.target_slot == C.active_slot) { return false; }
    if (address >= header && address < header + sizeof(C.staged_header)) {
        for (i = 0; i < BOOT_FLASH_PHRASE_SIZE; i++) {
            ((uint8_t *)&C.staged_header)[address - header + i] = BootProgramming_TransferBuffer[i];
        }
    } else {
        /* Reserved tail must stay erased; it is not image payload. */
        if (address >= header + sizeof(C.staged_header)) {
            for (i = 0; i < BOOT_FLASH_PHRASE_SIZE; i++) {
                if (BootProgramming_TransferBuffer[i] != 0xFFu) { return false; }
            }
        }
        if (!BootFlash_Program(address, BootProgramming_TransferBuffer,
                               BOOT_FLASH_PHRASE_SIZE)) { return false; }
    }
    C.programmed_size += BOOT_FLASH_PHRASE_SIZE;
    C.buffered_size = 0u;
    return true;
}
uint8_t BootProgramming_Transfer(uint8_t sequence, const uint8_t *data, uint32_t size)
{
    uint32_t i;
    uint8_t result = access_check();
    if (result != 0u) { return result; }
    if (!C.download_active) { return 0x24u; }
    /* No duplicate replay is accepted. Wrong BSC leaves expected BSC unchanged. */
    if (sequence != C.expected_block_sequence_counter) { return 0x73u; }
    if (data == NULL || size == 0u || size > C.max_block_length - 2u) { return 0x13u; }
    if (C.received_size > C.target_size || size > C.target_size - C.received_size) { return 0x31u; }
    for (i = 0; i < size; i++) {
        BootProgramming_TransferBuffer[C.buffered_size++] = data[i];
        if (C.buffered_size == BOOT_FLASH_PHRASE_SIZE && !flush_phrase()) {
            (void)BootUpdate_Abort(BOOT_UPDATE_PROGRAM_FAILED);
            BootProgramming_Abort(); return 0x72u;
        }
    }
    C.received_size += size;
    if (!BootUpdate_SetProgress(C.programmed_size, sequence)) { BootProgramming_Abort(); return 0x72u; }
    if (C.received_size * 100u >= C.target_size * 10u &&
        (C.received_size - size) * 100u < C.target_size * 10u) { BootFault_Hit(BOOT_FAULT_PROGRAM_10_PERCENT); }
    if (C.received_size * 100u >= C.target_size * 44u &&
        (C.received_size - size) * 100u < C.target_size * 44u) { BootFault_Hit(BOOT_FAULT_PROGRAM_44_PERCENT); }
    if (C.received_size * 100u >= C.target_size * 90u &&
        (C.received_size - size) * 100u < C.target_size * 90u) { BootFault_Hit(BOOT_FAULT_PROGRAM_90_PERCENT); }
    C.expected_block_sequence_counter = (uint8_t)(C.expected_block_sequence_counter + 1u);
    return 0u;
}
uint8_t BootProgramming_Exit(void)
{
    uint8_t result = access_check();
    if (result != 0u) { return result; }
    if (!C.download_active || C.received_size != C.target_size ||
        C.buffered_size != 0u || C.programmed_size != C.target_size) { return 0x24u; }
    /* Full slot images are phrase-aligned; arbitrary chunks are buffered. */
    if (!BootUpdate_MarkTransferComplete(&C.staged_header,
            (uint8_t)(C.expected_block_sequence_counter - 1u))) { return 0x72u; }
    C.download_active = false;
    C.transfer_complete = true;
    BootFault_Hit(BOOT_FAULT_AFTER_TRANSFER_EXIT);
    return 0u;
}
uint8_t BootProgramming_Verify(BootProgrammingPending pending)
{
    BootImageInfoType candidate; /* Validate reads the assigned header only. */
    uint32_t header;
    uint8_t result = access_check();
    if (result != 0u) { return result; }
    if (!C.erase_complete || !C.transfer_complete || C.download_active ||
        C.image_verified) { return 0x24u; }
    if (pending != NULL && !pending()) { return 0x72u; }
    if (!BootUpdate_SetState(BOOT_UPDATE_VERIFYING)) { return 0x72u; }
    BootFault_Hit(BOOT_FAULT_DURING_VERIFY);
    candidate.header = C.staged_header;
    C.validation_result = BootImage_Validate(C.target_slot, &candidate);
    if (C.validation_result != BOOT_IMAGE_VALID) {
        (void)BootUpdate_Abort(BOOT_UPDATE_BAD_IMAGE); return 0x72u;
    }
    if (BootSlot_IsValidId(C.active_slot) &&
        BootVersion_Compare(candidate.header.software_version, C.active_version) <= 0) {
        (void)BootUpdate_Abort(BOOT_UPDATE_BAD_IMAGE); return 0x31u;
    }
    header = BootSlot_GetHeaderAddress(C.target_slot);
    if (pending != NULL && !pending()) { return 0x72u; }
    if (!BootFlash_Erase(header, BOOT_FLASH_SECTOR_SIZE) ||
        !BootFlash_Program(header, (const uint8_t *)&C.staged_header, sizeof(C.staged_header))) {
        (void)BootUpdate_Abort(BOOT_UPDATE_PROGRAM_FAILED);
        BootProgramming_Abort(); return 0x72u;
    }
    if (pending != NULL && !pending()) { return 0x72u; }
    C.validation_result = BootImage_LoadInfo(C.target_slot, &candidate);
    C.image_verified = C.validation_result == BOOT_IMAGE_VALID;
    if (!C.image_verified) { (void)BootUpdate_Abort(BOOT_UPDATE_VERIFY_FAILED); }
    if (C.image_verified && !BootUpdate_MarkVerified(false)) { C.image_verified = false; }
    return C.image_verified ? 0u : 0x72u;
}
bool BootProgramming_CanReset(void)
{
    return BootUpdate_CanContinue() && BootUpdate_Current.state == BOOT_UPDATE_PENDING_ACTIVATION &&
        C.erase_complete && C.transfer_complete && C.image_verified && !C.download_active;
}
