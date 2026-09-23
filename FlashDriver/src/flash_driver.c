#include "device_registers.h"
#include "boot_config.h"
#include "flash_driver_api.h"

static uint32_t target_start;
static uint32_t target_end;

static bool range_ok(uint32_t address, uint32_t size)
{
    return size != 0u && address >= target_start && address < target_end &&
           size <= target_end - address;
}

bool FlashDriver_Init(uint32_t active, uint32_t target)
{
    target_start = 0u;
    target_end = 0u;
    if (target > 1u || target == active ||
        (active > 1u && active != 0x7FFFFFFFu)) { return false; }
    target_start = target == 0u ? BOOT_SLOT_A_BASE_ADDRESS : BOOT_SLOT_B_BASE_ADDRESS;
    target_end = target_start + (target == 0u ? BOOT_SLOT_A_SIZE : BOOT_SLOT_B_SIZE);
    IP_PCC->PCCn[PCC_FTFC_INDEX] |= PCC_PCCn_CGC_MASK;
    /* Executing in SRAM: disable instruction cache/speculation for coherent
     * read-back after erase/program. Reset restores Application cache setup. */
    IP_LMEM->PCCCR &= ~LMEM_PCCCR_ENCACHE_MASK;
    IP_MSCM->OCMDR[0] |= MSCM_OCMDR_OCM1(3u);
    IP_MSCM->OCMDR[1] |= MSCM_OCMDR_OCM1(3u);
    __asm volatile ("dsb\nisb" : : : "memory");
    return (IP_FTFC->FSTAT & FTFC_FSTAT_CCIF_MASK) != 0u;
}

static void refresh_watchdog(void)
{
    if ((IP_WDOG->CS & WDOG_CS_EN_MASK) != 0u) {
        IP_WDOG->CNT = 0xB480A602u; /* Programming mode uses CMD32EN, no window. */
    }
}

static bool command(uint8_t code, uint32_t address, const uint8_t *data)
{
    uint32_t timeout = 50000000u;
    uint32_t primask;
    uint32_t index;
    bool ok;
    __asm volatile ("mrs %0, primask\ncpsid i" : "=r"(primask) : : "memory");
    if ((IP_FTFC->FSTAT & FTFC_FSTAT_CCIF_MASK) == 0u) {
        /* Never return to PFlash while it is busy. Watchdog eventually resets. */
        for (;;) { }
    }
    IP_FTFC->FSTAT = FTFC_FSTAT_FPVIOL_MASK | FTFC_FSTAT_ACCERR_MASK |
                     FTFC_FSTAT_RDCOLERR_MASK;
    IP_FTFC->FCCOB[3] = code;
    IP_FTFC->FCCOB[2] = (uint8_t)(address >> 16);
    IP_FTFC->FCCOB[1] = (uint8_t)(address >> 8);
    IP_FTFC->FCCOB[0] = (uint8_t)address;
    if (data != (const uint8_t *)0) {
        for (index = 0; index < BOOT_FLASH_PHRASE_SIZE; index++) {
            IP_FTFC->FCCOB[index + 4u] = data[index];
        }
    }
    __asm volatile ("dsb\nisb" : : : "memory");
    IP_FTFC->FSTAT = FTFC_FSTAT_CCIF_MASK;
    while ((IP_FTFC->FSTAT & FTFC_FSTAT_CCIF_MASK) == 0u) {
        if (timeout == 0u) { for (;;) { } } /* Fail closed, in SRAM. */
        timeout--;
        refresh_watchdog();
    }
    ok = (IP_FTFC->FSTAT & (FTFC_FSTAT_MGSTAT0_MASK | FTFC_FSTAT_FPVIOL_MASK |
          FTFC_FSTAT_ACCERR_MASK | FTFC_FSTAT_RDCOLERR_MASK)) == 0u;
    __asm volatile ("dsb\nisb\nmsr primask, %0" : : "r"(primask) : "memory");
    return ok;
}

bool FlashDriver_Verify(uint32_t address, const uint8_t *data, uint32_t size)
{
    uint32_t i;
    const volatile uint8_t *flash = (const volatile uint8_t *)address;
    if (!range_ok(address, size) || data == (const uint8_t *)0) { return false; }
    for (i = 0; i < size; i++) { if (flash[i] != data[i]) { return false; } }
    return true;
}

bool FlashDriver_Erase(uint32_t address, uint32_t size)
{
    uint32_t i;
    const volatile uint8_t *flash = (const volatile uint8_t *)address;
    /* One sector per call keeps the UDS P2* scheduler in control. */
    if (!range_ok(address, size) || size != BOOT_FLASH_SECTOR_SIZE ||
        (address & (BOOT_FLASH_SECTOR_SIZE - 1u)) != 0u) { return false; }
    if (!command(0x09u, address, (const uint8_t *)0)) { return false; }
    for (i = 0; i < size; i++) { if (flash[i] != 0xFFu) { return false; } }
    return true;
}

bool FlashDriver_Program(uint32_t address, const uint8_t *data, uint32_t size)
{
    uint32_t offset, i;
    uintptr_t source = (uintptr_t)data;
    if (!range_ok(address, size) ||
        (address & (BOOT_FLASH_PHRASE_SIZE - 1u)) != 0u ||
        (size & (BOOT_FLASH_PHRASE_SIZE - 1u)) != 0u ||
        source < BOOT_SRAM_BASE_ADDRESS || source >= BOOT_SRAM_APPLICATION_END_ADDRESS ||
        size > BOOT_SRAM_APPLICATION_END_ADDRESS - source) { return false; }
    for (offset = 0; offset < size; offset += BOOT_FLASH_PHRASE_SIZE) {
        bool blank = true;
        for (i = 0; i < BOOT_FLASH_PHRASE_SIZE; i++) {
            if (data[offset + i] != 0xFFu) { blank = false; }
        }
        /* Do not program erased padding/ECC phrases unnecessarily. */
        if (!blank && !command(0x07u, address + offset, data + offset)) { return false; }
        if (!FlashDriver_Verify(address + offset, data + offset,
                                 BOOT_FLASH_PHRASE_SIZE)) { return false; }
    }
    return true;
}

/* Separate, narrowly bounded entry points: the Application write API never
 * gains access to metadata. Restore its inactive-slot window before returning. */
bool FlashDriver_JournalErase(uint32_t copy)
{
    uint32_t saved_start = target_start, saved_end = target_end;
    bool result;
    if (copy > 1u) { return false; }
    target_start = BOOT_JOURNAL_COPY0_ADDRESS + copy * BOOT_FLASH_SECTOR_SIZE;
    target_end = target_start + BOOT_FLASH_SECTOR_SIZE;
    result = FlashDriver_Erase(target_start, BOOT_FLASH_SECTOR_SIZE);
    target_start = saved_start; target_end = saved_end;
    return result;
}
bool FlashDriver_JournalProgram(uint32_t copy, uint32_t offset, const uint8_t *data, uint32_t size)
{
    uint32_t saved_start = target_start, saved_end = target_end;
    bool result;
    if (copy > 1u || offset >= BOOT_JOURNAL_RECORD_SIZE ||
        size > BOOT_JOURNAL_RECORD_SIZE - offset) { return false; }
    target_start = BOOT_JOURNAL_COPY0_ADDRESS + copy * BOOT_FLASH_SECTOR_SIZE;
    target_end = target_start + BOOT_JOURNAL_RECORD_SIZE;
    result = FlashDriver_Program(target_start + offset, data, size);
    target_start = saved_start; target_end = saved_end;
    return result;
}
static uint32_t metadata_address(uint32_t slot, uint32_t copy)
{ return BOOT_METADATA_SLOT_A_COPY0_ADDRESS + (slot * 2u + copy) * BOOT_FLASH_SECTOR_SIZE; }
bool FlashDriver_MetadataErase(uint32_t slot, uint32_t copy)
{
    uint32_t saved_start = target_start, saved_end = target_end;
    bool result;
    if (slot > 1u || copy > 1u) { return false; }
    target_start = metadata_address(slot, copy);
    target_end = target_start + BOOT_FLASH_SECTOR_SIZE;
    result = FlashDriver_Erase(target_start, BOOT_FLASH_SECTOR_SIZE);
    target_start = saved_start; target_end = saved_end;
    return result;
}
bool FlashDriver_MetadataProgram(uint32_t slot, uint32_t copy, uint32_t offset,
                                const uint8_t *data, uint32_t size)
{
    uint32_t saved_start = target_start, saved_end = target_end;
    bool result;
    if (slot > 1u || copy > 1u || offset >= BOOT_METADATA_RECORD_SIZE ||
        size > BOOT_METADATA_RECORD_SIZE - offset) { return false; }
    target_start = metadata_address(slot, copy);
    target_end = target_start + BOOT_METADATA_RECORD_SIZE;
    result = FlashDriver_Program(target_start + offset, data, size);
    target_start = saved_start; target_end = saved_end;
    return result;
}
uint32_t FlashDriver_GetVersion(void) { return 0x00030000u; }

__attribute__((section(".driver_api"), used))
const FlashDriverApi FlashDriver_Table = {
    FLASH_DRIVER_MAGIC, FLASH_DRIVER_API_VERSION, FlashDriver_Init,
    FlashDriver_Erase, FlashDriver_Program, FlashDriver_Verify, FlashDriver_GetVersion,
    FlashDriver_JournalErase, FlashDriver_JournalProgram,
    FlashDriver_MetadataErase, FlashDriver_MetadataProgram
};
_Static_assert(sizeof(FlashDriverApi) == 44u, "RAM API ABI changed");
