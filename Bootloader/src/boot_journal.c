#include "boot_journal.h"
#include "boot_journal_storage.h"
#include "boot_config.h"
#include "boot_fault.h"
static int32_t latest_copy = -1;
uint32_t BootJournal_Crc(const void *data, uint32_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu, i, bit;
    for (i = 0u; i < length; i++) {
        crc ^= bytes[i];
        for (bit = 0u; bit < 8u; bit++) { crc = (crc >> 1) ^ ((0u - (crc & 1u)) & 0xEDB88320u); }
    }
    return ~crc;
}
bool BootJournal_SequenceNewer(uint32_t a, uint32_t b)
{ uint32_t difference = a - b; return difference != 0u && difference < 0x80000000u; }
static bool equal(const BootUpdateJournal *a, const BootUpdateJournal *b)
{
    uint32_t i;
    for (i = 0; i < sizeof(*a); i++) {
        if (((const uint8_t *)a)[i] != ((const uint8_t *)b)[i]) { return false; }
    }
    return true;
}
bool BootJournal_Validate(const BootUpdateJournal *r)
{
    if (r == NULL || r->magic != BOOT_JOURNAL_MAGIC || r->format_version != BOOT_JOURNAL_FORMAT ||
        r->commit_marker != BOOT_JOURNAL_COMMIT_MARKER ||
        r->journal_crc != BootJournal_Crc(r, offsetof(BootUpdateJournal, journal_crc)) ||
        r->state > BOOT_UPDATE_ABORTED || r->last_result > BOOT_UPDATE_COMMISSIONED ||
        (r->active_slot > 1u && r->active_slot != (uint32_t)BOOT_SLOT_ID_UNKNOWN) ||
        r->target_slot > 1u || r->target_slot == r->active_slot ||
        r->expected_size > BOOT_SLOT_A_SIZE || r->committed_size > r->expected_size ||
        r->last_block_sequence > 255u || (r->flags & ~BOOT_UPDATE_HEADER_KNOWN) != 0u) { return false; }
    if (r->state >= BOOT_UPDATE_PREPARING && r->state <= BOOT_UPDATE_PENDING_ACTIVATION &&
        r->expected_size != BOOT_SLOT_A_SIZE) { return false; }
    if (r->state >= BOOT_UPDATE_TRANSFER_COMPLETE && r->state <= BOOT_UPDATE_PENDING_ACTIVATION &&
        (r->flags != BOOT_UPDATE_HEADER_KNOWN || r->committed_size != r->expected_size ||
         r->target_version != r->cached_header.software_version ||
         r->expected_crc != r->cached_header.image_crc32)) { return false; }
    return true;
}
BootJournalStatus BootJournal_LoadLatest(BootUpdateJournal *record)
{
    BootUpdateJournal copies[2];
    bool valid[2], erased = true;
    uint32_t i, j;
    latest_copy = -1;
    for (i = 0; i < 2u; i++) {
        if (!BootJournalStorage_Read(i, (uint8_t *)&copies[i], sizeof(copies[i]))) { return BOOT_JOURNAL_INVALID; }
        valid[i] = BootJournal_Validate(&copies[i]);
        for (j = 0; j < sizeof(copies[i]); j++) {
            if (((uint8_t *)&copies[i])[j] != 0xFFu) { erased = false; }
        }
    }
    if (valid[0] && valid[1]) {
        if (BootJournal_SequenceNewer(copies[0].sequence, copies[1].sequence)) { latest_copy = 0; }
        else if (BootJournal_SequenceNewer(copies[1].sequence, copies[0].sequence)) { latest_copy = 1; }
        else if (equal(&copies[0], &copies[1])) { latest_copy = 0; }
        else { return BOOT_JOURNAL_INVALID; } /* Ambiguous half-range or conflicting equal sequence. */
    } else if (valid[0]) { latest_copy = 0; }
    else if (valid[1]) { latest_copy = 1; }
    else { return erased ? BOOT_JOURNAL_EMPTY : BOOT_JOURNAL_INVALID; }
    *record = copies[latest_copy];
    return BOOT_JOURNAL_VALID;
}
static bool commit(BootUpdateJournal *record, bool initialize)
{
    BootUpdateJournal previous, next = *record, readback;
    BootJournalStatus status = BootJournal_LoadLatest(&previous);
    uint32_t copy, offset;
    if (status != BOOT_JOURNAL_VALID && !initialize) { return false; }
    if (status == BOOT_JOURNAL_VALID && initialize) { return false; }
    copy = status == BOOT_JOURNAL_VALID ? (uint32_t)(1 - latest_copy) : 0u;
    next.sequence = status == BOOT_JOURNAL_VALID ? previous.sequence + 1u : 1u;
    next.magic = BOOT_JOURNAL_MAGIC; next.format_version = BOOT_JOURNAL_FORMAT;
    next.commit_marker = BOOT_JOURNAL_COMMIT_MARKER;
    next.journal_crc = BootJournal_Crc(&next, offsetof(BootUpdateJournal, journal_crc));
    if (!BootJournal_Validate(&next) || !BootJournalStorage_Erase(copy)) { return false; }
    BootFault_Hit(BOOT_FAULT_DURING_JOURNAL_COMMIT);
    for (offset = 0; offset < sizeof(next); offset += BOOT_FLASH_PHRASE_SIZE) {
        if (!BootJournalStorage_Program(copy, offset, ((const uint8_t *)&next) + offset,
                                       BOOT_FLASH_PHRASE_SIZE)) { return false; }
        BootFault_Hit(BOOT_FAULT_DURING_JOURNAL_COMMIT);
    }
    if (!BootJournalStorage_Read(copy, (uint8_t *)&readback, sizeof(readback)) ||
        !BootJournal_Validate(&readback) || !equal(&next, &readback)) { return false; }
    *record = readback;
    return true;
}
bool BootJournal_Commit(BootUpdateJournal *record) { return commit(record, false); }
bool BootJournal_Initialize(BootUpdateJournal *record) { return commit(record, true); }
bool BootJournal_Clear(void)
{
    BootUpdateJournal r;
    /* Never erase interruption evidence or unvalidated pending activation. */
    if (BootJournal_LoadLatest(&r) != BOOT_JOURNAL_VALID || r.state != BOOT_UPDATE_IDLE) { return false; }
    r.last_result = BOOT_UPDATE_NONE;
    return BootJournal_Commit(&r);
}
uint32_t BootJournal_GetState(void)
{
    BootUpdateJournal r;
    return BootJournal_LoadLatest(&r) == BOOT_JOURNAL_VALID ? r.state : 0xFFFFFFFFu;
}
