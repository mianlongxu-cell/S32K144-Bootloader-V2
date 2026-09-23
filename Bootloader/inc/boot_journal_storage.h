#ifndef BOOT_JOURNAL_STORAGE_H
#define BOOT_JOURNAL_STORAGE_H
#include <stdint.h>
#include <stdbool.h>
bool BootJournalStorage_Read(uint32_t copy, uint8_t *data, uint32_t size);
bool BootJournalStorage_Erase(uint32_t copy);
bool BootJournalStorage_Program(uint32_t copy, uint32_t offset, const uint8_t *data, uint32_t size);
#endif
