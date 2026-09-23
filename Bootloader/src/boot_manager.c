#include <stdbool.h>
#include <stdint.h>
#include "device_registers.h"
#include "s32_core_cm4.h"
#include "boot.h"
#include "boot_can.h"
#include "boot_config.h"
#include "boot_flash.h"
#include "boot_image.h"
#include "boot_isotp.h"
#include "boot_jump.h"
#include "boot_lifecycle.h"
#include "boot_manager.h"
#include "boot_policy.h"
#include "boot_request.h"
#include "boot_types.h"
#include "boot_slot.h"
#include "boot_uds.h"
#include "boot_update.h"
#include "boot_reset_reason.h"

volatile Boot_StateType Boot_CurrentState = BOOT_STATE_INIT;
BootImageInfoType BootManager_SlotAInfo;
BootImageInfoType BootManager_SlotBInfo;
volatile BootTargetType BootManager_SelectedTarget = BOOT_TARGET_NONE;
static BootSlotIdType BootManager_ActiveSlot = BOOT_SLOT_ID_UNKNOWN;
static uint32_t BootManager_ActiveVersion;

BootSlotIdType BootManager_GetActiveSlot(void) { return BootManager_ActiveSlot; }
uint32_t BootManager_GetActiveVersion(void) { return BootManager_ActiveVersion; }

static void BootManager_CaptureActiveSlot(bool software_request)
{
    const volatile uint32_t *flag = (const volatile uint32_t *)BOOT_REQUEST_ADDRESS;
    const BootTargetType target = BootManager_SelectBootTarget();
    BootManager_ActiveSlot = target == BOOT_TARGET_SLOT_A ? BOOT_SLOT_ID_A :
        (target == BOOT_TARGET_SLOT_B ? BOOT_SLOT_ID_B : BOOT_SLOT_ID_UNKNOWN);
    /* If the protected image has subsequently failed CRC, retain its identity
     * for write protection; do not silently choose it as the erase target. */
    if (BootManager_ActiveSlot == BOOT_SLOT_ID_UNKNOWN && BootUpdate_JournalValid &&
        BootUpdate_Current.active_slot <= 1u) {
        BootManager_ActiveSlot = (BootSlotIdType)BootUpdate_Current.active_slot;
    }
    /* Explicit ID/complement from App. Old Apps use the validated Boot Policy. */
    if (software_request && flag[2] == ~flag[3] && flag[2] <= 1u) {
        if ((flag[2] == 0u && BootManager_SlotAInfo.valid) ||
            (flag[2] == 1u && BootManager_SlotBInfo.valid)) {
            BootManager_ActiveSlot = (BootSlotIdType)flag[2];
        }
    }
    BootManager_ActiveVersion = BootManager_ActiveSlot == BOOT_SLOT_ID_A && BootManager_SlotAInfo.valid ?
        BootManager_SlotAInfo.header.software_version :
        (BootManager_ActiveSlot == BOOT_SLOT_ID_B && BootManager_SlotBInfo.valid ?
         BootManager_SlotBInfo.header.software_version : 0u);
}
void BootManager_RefreshActiveSlot(void) { BootManager_CaptureActiveSlot(false); }

static void BootManager_DisableWatchdog(void)
{
    IP_WDOG->CNT = 0xD928C520u;
    (void)IP_WDOG->CNT;
    IP_WDOG->TOVAL = 0x0000FFFFu;
    /* Keep UPDATE enabled so Programming Mode can enable supervision. */
    IP_WDOG->CS = 0x00002120u;
}

static void BootManager_LedInit(void)
{
    IP_PCC->PCCn[PCC_PORTD_INDEX] |= PCC_PCCn_CGC_MASK;
    IP_PORTD->PCR[BOOT_LED_PIN] = PORT_PCR_MUX(1u);
    IP_PTD->PDDR |= (1u << BOOT_LED_PIN);
    IP_PTD->PCOR = (1u << BOOT_LED_PIN);
}

static void BootManager_LedOff(void)
{
    IP_PTD->PSOR = (1u << BOOT_LED_PIN);
}

static void BootManager_WaitStartupIndication(void)
{
    volatile uint32_t count;
    for (count = 0u; count < BOOT_LED_DELAY_LOOPS; count++)
    {
        NOP();
    }
}

static void BootManager_PrepareApplicationHandover(void)
{
    BootManager_LedOff();
    IP_PORTC->PCR[BOOT_REQUEST_PIN] = 0u;
    IP_PTC->PDDR &= ~(1u << BOOT_REQUEST_PIN);
}

static void BootManager_SoftwareReset(void)
{
    S32_SCB->AIRCR = S32_SCB_AIRCR_VECTKEY(0x5FAu) |
                     S32_SCB_AIRCR_SYSRESETREQ_MASK;
    for (;;)
    {
    }
}

static void BootManager_RunProgrammingServer(void)
{
    BootIsoTp_PduType request;
    BootIsoTp_PduType response;

    BootCan_Init();
    BootIsoTp_Init();
    BootUds_Init();
    /* Programming watchdog: LPO, CMD32EN, non-windowed. RAM code refreshes it. */
    IP_WDOG->CNT = 0xD928C520u;
    (void)IP_WDOG->CNT;
    IP_WDOG->TOVAL = 0xFFFFu;
    IP_WDOG->CS = WDOG_CS_CMD32EN_MASK | WDOG_CS_EN_MASK |
                  WDOG_CS_UPDATE_MASK | WDOG_CS_CLK(1u);
    if ((IP_WDOG->CS & (WDOG_CS_EN_MASK | WDOG_CS_CMD32EN_MASK)) !=
        (WDOG_CS_EN_MASK | WDOG_CS_CMD32EN_MASK)) {
        Boot_CurrentState = BOOT_STATE_ERROR;
        return; /* Never enter the flash path without watchdog supervision. */
    }
    for (;;)
    {
        IP_WDOG->CNT = 0xB480A602u;
        BootUds_MainFunction();
        if (BootIsoTp_MainFunction(&request) &&
            BootUds_ProcessRequest(&request, &response))
        {
            (void)BootIsoTp_Transmit(&response);
            if (BootUds_IsResetPending())
            {
                volatile uint32_t delay;
                for (delay = 0u; delay < 1000000u; delay++)
                {
                    NOP();
                }
                BootManager_SoftwareReset();
            }
        }
    }
}

void BootManager_Init(void)
{
    BootResetReason reset_reason;
    BootManager_DisableWatchdog();
    reset_reason = BootResetReason_Capture();
    /* Recovery may commit a Journal or publish a cached Header: supervise these
     * RAM flash operations too, not only the later diagnostic server. */
    IP_WDOG->CNT = 0xD928C520u;
    (void)IP_WDOG->CNT;
    IP_WDOG->TOVAL = 0xFFFFu;
    IP_WDOG->CS = WDOG_CS_CMD32EN_MASK | WDOG_CS_EN_MASK |
                  WDOG_CS_UPDATE_MASK | WDOG_CS_CLK(1u);
    BootFlash_Init();
    if ((IP_WDOG->CS & (WDOG_CS_EN_MASK | WDOG_CS_CMD32EN_MASK)) ==
        (WDOG_CS_EN_MASK | WDOG_CS_CMD32EN_MASK)) {
        (void)BootUpdate_LoadRecoveryState();
        (void)BootLifecycle_Init(reset_reason);
    }
    BootManager_DisableWatchdog();
    BootManager_LedInit();
    Boot_RequestInit();
    BootManager_SelectedTarget = BOOT_TARGET_NONE;
}

BootTargetType BootManager_SelectBootTarget(void)
{
    /* Do not even scan a dirty/untrusted slot during normal boot selection. */
    BootManager_SlotAInfo = (BootImageInfoType){0};
    BootManager_SlotBInfo = (BootImageInfoType){0};
    if (BootUpdate_IsSlotBootable(BOOT_SLOT_ID_A) && BootLifecycle_IsSlotBootable(BOOT_SLOT_ID_A)) {
        (void)BootImage_LoadInfo(BOOT_SLOT_ID_A, &BootManager_SlotAInfo);
    }
    if (BootUpdate_IsSlotBootable(BOOT_SLOT_ID_B) && BootLifecycle_IsSlotBootable(BOOT_SLOT_ID_B)) {
        (void)BootImage_LoadInfo(BOOT_SLOT_ID_B, &BootManager_SlotBInfo);
    }
    return BootLifecycle_Select(&BootManager_SlotAInfo,&BootManager_SlotBInfo);
}

void BootManager_Run(void)
{
    bool software_request;
    bool hardware_request;
    bool confirm_request;
    BootSlotIdType selected_slot;

    Boot_CurrentState = BOOT_STATE_INIT;
    for (;;)
    {
        switch (Boot_CurrentState)
        {
            case BOOT_STATE_INIT:
                BootManager_Init();
                Boot_CurrentState = BOOT_STATE_CHECK_REQUEST;
                break;

            case BOOT_STATE_CHECK_REQUEST:
                software_request = Boot_IsSoftwareBootRequested();
                confirm_request = Boot_IsApplicationConfirmRequested();
                hardware_request = Boot_IsBootRequested();
                if (confirm_request && Boot_GetRequestedSlot(&selected_slot)) {
                    (void)BootLifecycle_Confirm(selected_slot);
                    Boot_ClearSoftwareBootRequest();
                }
                BootManager_CaptureActiveSlot(software_request);
                if (software_request)
                {
                    Boot_ClearSoftwareBootRequest();
                }
                Boot_CurrentState = (software_request || hardware_request) ?
                    BOOT_STATE_PROGRAMMING : BOOT_STATE_CHECK_APP;
                break;

            case BOOT_STATE_CHECK_APP:
                BootManager_SelectedTarget = BootManager_SelectBootTarget();
                Boot_CurrentState =
                    ((BootManager_SelectedTarget == BOOT_TARGET_SLOT_A) ||
                     (BootManager_SelectedTarget == BOOT_TARGET_SLOT_B)) ?
                    BOOT_STATE_JUMP_APP : BOOT_STATE_PROGRAMMING;
                break;

            case BOOT_STATE_JUMP_APP:
                selected_slot =
                    (BootManager_SelectedTarget == BOOT_TARGET_SLOT_B) ?
                    BOOT_SLOT_ID_B : BOOT_SLOT_ID_A;
                if (!Boot_WasSoftwareReset())
                {
                    BootResetReason_PreserveForNormalization(BootLifecycle_GetResetReason());
                    BootManager_SoftwareReset();
                }
                if (!BootLifecycle_PrepareBoot(selected_slot))
                {
                    Boot_CurrentState = BOOT_STATE_ERROR;
                    break;
                }
                BootLifecycle_WriteAppHandover(selected_slot);
                BootManager_WaitStartupIndication();
                BootManager_PrepareApplicationHandover();
                if (!BootJump_ToSlot(selected_slot))
                {
                    Boot_CurrentState = BOOT_STATE_ERROR;
                }
                break;

            case BOOT_STATE_STAY:
                NOP();
                break;

            case BOOT_STATE_PROGRAMMING:
                BootManager_SelectedTarget = BOOT_TARGET_PROGRAMMING;
                BootManager_RunProgrammingServer();
                Boot_CurrentState = BOOT_STATE_ERROR;
                break;

            case BOOT_STATE_ERROR:
            default:
                Boot_CurrentState = BOOT_STATE_ERROR;
                NOP();
                break;
        }
    }
}
