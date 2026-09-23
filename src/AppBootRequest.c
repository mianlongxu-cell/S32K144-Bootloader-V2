#include "S32K144.h"
#include "FreeRTOS.h"
#include "task.h"
#include "boot_memory_contract.h"
#include "AppBootRequest.h"
#include "boot_app_if.h"

#define APP_BOOT_RESET_DELAY_MS   100u
#define APP_AIRCR                 (*(volatile uint32_t *)0xE000ED0Cu)
#define APP_AIRCR_RESET_VALUE     0x05FA0004u

static volatile uint32_t AppBootRequest_PendingMagic;
static TickType_t AppBootRequest_ResetTick;

void AppBootRequest_RequestProgrammingReset(void)
{
    AppBootRequest_PendingMagic = BOOT_CONTRACT_REQUEST_MAGIC;
    AppBootRequest_ResetTick = xTaskGetTickCount();
}

void AppBootRequest_RequestConfirmReset(void)
{
    if (AppBootRequest_PendingMagic == 0u) {
        AppBootRequest_PendingMagic = BOOT_CONTRACT_CONFIRM_MAGIC;
        AppBootRequest_ResetTick = xTaskGetTickCount();
    }
}

static void write_and_reset(uint32_t magic, uint32_t data) __attribute__((noreturn));
static void write_and_reset(uint32_t magic, uint32_t data)
{
    volatile uint32_t *flag = (volatile uint32_t *)BOOT_CONTRACT_REQUEST_ADDRESS;
    flag[0] = magic; flag[1] = ~magic; flag[2] = data; flag[3] = ~data;
    __asm volatile ("dsb\nisb" : : : "memory");
    APP_AIRCR = APP_AIRCR_RESET_VALUE;
    for (;;) { }
}

void AppBootRequest_RecordFaultAndReset(void)
{ taskDISABLE_INTERRUPTS(); write_and_reset(BOOT_CONTRACT_FAULT_MAGIC, 1u); }

void AppBootRequest_ResetWithoutRequest(void)
{
    volatile uint32_t *flag = (volatile uint32_t *)BOOT_CONTRACT_REQUEST_ADDRESS;
    taskDISABLE_INTERRUPTS(); flag[0]=0u;flag[1]=0u;flag[2]=0u;flag[3]=0u;
    __asm volatile ("dsb\nisb" : : : "memory"); APP_AIRCR=APP_AIRCR_RESET_VALUE;
    for (;;) { }
}

void AppBootRequest_MainFunction(void)
{
    volatile uint32_t *flag;
    BootAppSlotType slot;

    if ((AppBootRequest_PendingMagic == 0u) ||
        ((TickType_t)(xTaskGetTickCount() - AppBootRequest_ResetTick) <
         pdMS_TO_TICKS(APP_BOOT_RESET_DELAY_MS)))
    {
        return;
    }
    taskDISABLE_INTERRUPTS();
    flag = (volatile uint32_t *)BOOT_CONTRACT_REQUEST_ADDRESS;
    flag[0] = AppBootRequest_PendingMagic;
    flag[1] = ~AppBootRequest_PendingMagic;
    if (Boot_GetRunningSlot(&slot) == BOOT_APP_IF_OK) {
        flag[2] = (uint32_t)slot;
        flag[3] = ~(uint32_t)slot;
    } else {
        flag[2] = 0u;
        flag[3] = 0u;
    }
    __asm volatile ("dsb\nisb" : : : "memory"); APP_AIRCR = APP_AIRCR_RESET_VALUE;
    for (;;) { }
}
