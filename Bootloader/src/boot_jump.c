#include <stdint.h>
#include "device_registers.h"
#include "boot_config.h"
#include "boot_jump.h"
#include "boot_slot.h"
#include "boot_update.h"
#include "boot_image.h"
#include "boot_lifecycle.h"

#define BOOT_SYSTICK_CTRL       (*(volatile uint32_t *)0xE000E010u)
#define BOOT_SYSTICK_LOAD       (*(volatile uint32_t *)0xE000E014u)
#define BOOT_SYSTICK_VAL        (*(volatile uint32_t *)0xE000E018u)
#define BOOT_NVIC_ICER          ((volatile uint32_t *)0xE000E180u)
#define BOOT_NVIC_ICPR          ((volatile uint32_t *)0xE000E280u)
#define BOOT_NVIC_BANK_COUNT    8u

__attribute__((naked, noreturn))
static void BootJump_SetMspAndBranch(
    uint32_t app_stack __attribute__((unused)),
    uint32_t app_reset __attribute__((unused)))
{
    __asm volatile (
        "msr msp, r0\n"
        "dsb\n"
        "isb\n"
        "cpsie i\n"
        "bx r1\n");
}

bool BootJump_ToSlot(BootSlotIdType slot)
{
    BootSlotDescriptorType descriptor;
    const volatile uint32_t *vectors;
    uint32_t app_stack;
    uint32_t app_reset;
    uint32_t reset_address;
    uint32_t bank;

    if (!BootUpdate_IsSlotBootable(slot) || !BootLifecycle_IsSlotBootable(slot) ||
        !BootImage_IsBootable(slot) ||
        !BootSlot_GetDescriptor(slot, &descriptor) ||
        ((descriptor.vector_address & (BOOT_VECTOR_ALIGNMENT - 1u)) != 0u) ||
        !BootSlot_ContainsRange(slot, descriptor.vector_address,
                                2u * sizeof(uint32_t)) ||
        (descriptor.vector_address >= descriptor.payload_end_address))
    {
        return false;
    }

    vectors = (const volatile uint32_t *)descriptor.vector_address;
    app_stack = vectors[0];
    app_reset = vectors[1];
    reset_address = app_reset & ~1u;

    /* Minimal independent safety gate immediately before changing CPU state. */
    if ((app_stack == 0u) || (app_stack == 0xFFFFFFFFu) ||
        (app_stack < BOOT_SRAM_BASE_ADDRESS) ||
        (app_stack > BOOT_SRAM_APPLICATION_END_ADDRESS) ||
        ((app_stack & 0x7u) != 0u) ||
        (app_reset == 0u) || (app_reset == 0xFFFFFFFFu) ||
        ((app_reset & 1u) == 0u) ||
        (reset_address < descriptor.vector_address) ||
        (reset_address >= descriptor.payload_end_address))
    {
        return false;
    }

    DISABLE_INTERRUPTS();
    BOOT_SYSTICK_CTRL = 0u;
    BOOT_SYSTICK_LOAD = 0u;
    BOOT_SYSTICK_VAL = 0u;

    for (bank = 0u; bank < BOOT_NVIC_BANK_COUNT; bank++)
    {
        BOOT_NVIC_ICER[bank] = 0xFFFFFFFFu;
        BOOT_NVIC_ICPR[bank] = 0xFFFFFFFFu;
    }
    S32_SCB->ICSR = S32_SCB_ICSR_PENDSTCLR_MASK |
                    S32_SCB_ICSR_PENDSVCLR_MASK;
    S32_SCB->VTOR = descriptor.vector_address;

    __asm volatile ("movs r0, #0\nmsr control, r0" : : : "r0", "memory");
    __asm volatile ("dsb\nisb" : : : "memory");
    BootJump_SetMspAndBranch(app_stack, app_reset);
}

bool BootJump_ToVector(uint32_t vector_address)
{
    if (vector_address == BootSlot_GetVectorAddress(BOOT_SLOT_ID_A))
    {
        return BootJump_ToSlot(BOOT_SLOT_ID_A);
    }
    if (vector_address == BootSlot_GetVectorAddress(BOOT_SLOT_ID_B))
    {
        return BootJump_ToSlot(BOOT_SLOT_ID_B);
    }
    return false;
}

void Boot_JumpToApplication(void)
{
    (void)BootJump_ToSlot(BOOT_SLOT_ID_A);
}
