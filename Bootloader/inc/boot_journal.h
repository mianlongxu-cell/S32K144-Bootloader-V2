#ifndef BOOT_JOURNAL_H
#define BOOT_JOURNAL_H
#include <stddef.h>
#include "boot_types.h"
#define BOOT_JOURNAL_MAGIC 0x4A345642u
#define BOOT_JOURNAL_FORMAT 1u
#define BOOT_JOURNAL_COMMIT_MARKER 0x434F4D54u
#define BOOT_UPDATE_HEADER_KNOWN 1u
#define BOOT_UPDATE_CHECKPOINT 32768u
typedef enum {
    BOOT_UPDATE_IDLE = 0, BOOT_UPDATE_PREPARING, BOOT_UPDATE_ERASING,
    BOOT_UPDATE_DOWNLOAD_READY, BOOT_UPDATE_PROGRAMMING,
    BOOT_UPDATE_TRANSFER_COMPLETE, BOOT_UPDATE_VERIFYING,
    BOOT_UPDATE_VERIFIED, BOOT_UPDATE_PENDING_ACTIVATION, BOOT_UPDATE_ABORTED
} BootUpdateState;
typedef enum {
    BOOT_UPDATE_NONE = 0, BOOT_UPDATE_SUCCESS, BOOT_UPDATE_INTERRUPTED,
    BOOT_UPDATE_INTERRUPTED_PROGRAMMING, BOOT_UPDATE_ERASE_FAILED,
    BOOT_UPDATE_PROGRAM_FAILED, BOOT_UPDATE_TRANSFER_FAILED,
    BOOT_UPDATE_VERIFY_FAILED, BOOT_UPDATE_BAD_IMAGE, BOOT_UPDATE_JOURNAL_ERROR,
    BOOT_UPDATE_POWER_LOSS_RECOVERED, BOOT_UPDATE_COMMISSIONED
} BootUpdateResult;
/* Little-endian disk ABI. Last phrase is programmed last, CRC covers [0,120).
 * Cached immutable Header survives reset before its publication in the slot. */
typedef struct __attribute__((aligned(8))) {
    uint32_t magic, format_version, sequence, transaction_id;
    uint32_t active_slot, target_slot, state, target_version;
    uint32_t expected_size, committed_size, expected_crc, last_block_sequence;
    uint32_t last_result, flags;
    BootImageHeaderType cached_header;
    uint32_t journal_crc, commit_marker;
} BootUpdateJournal;
_Static_assert(sizeof(BootUpdateJournal) == 128u, "Journal ABI size");
_Static_assert(_Alignof(BootUpdateJournal) == 8u, "Journal RAM phrase alignment");
_Static_assert(offsetof(BootUpdateJournal, journal_crc) == 120u, "Journal CRC offset");
typedef enum { BOOT_JOURNAL_VALID, BOOT_JOURNAL_EMPTY, BOOT_JOURNAL_INVALID } BootJournalStatus;
uint32_t BootJournal_Crc(const void *data, uint32_t length);
bool BootJournal_SequenceNewer(uint32_t a, uint32_t b);
bool BootJournal_Validate(const BootUpdateJournal *record);
BootJournalStatus BootJournal_LoadLatest(BootUpdateJournal *record);
bool BootJournal_Commit(BootUpdateJournal *record);
bool BootJournal_Initialize(BootUpdateJournal *record);
bool BootJournal_Clear(void);
uint32_t BootJournal_GetState(void);
#endif
