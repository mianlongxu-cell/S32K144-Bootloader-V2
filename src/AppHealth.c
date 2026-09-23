#include "S32K144.h"
#include "FreeRTOS.h"
#include "task.h"
#include "AppHealth.h"
#include "AppBootRequest.h"
#include "AppRTOS.h"
#include "CanIf.h"
#include "VehicleApp.h"
#include "boot_app_if.h"
static volatile uint8_t fatal_error;
uint8_t AppHealth_CheckBootReady(void)
{
    return fatal_error==0u&&CanIf_IsInitialized()!=0u&&VehicleApp_GetCycleCount()!=0u&&
           AppRTOSTxCount!=0u&&xTaskGetTickCount()>=pdMS_TO_TICKS(1000u);
}
void AppHealth_ReportFatal(void){fatal_error=1u;}
#if APP_TRIAL_MODE == APP_TRIAL_MODE_WATCHDOG_RESET
static void enable_short_watchdog(void)
{
    IP_WDOG->CNT=0xD928C520u;(void)IP_WDOG->CNT;IP_WDOG->TOVAL=512u;
    IP_WDOG->CS=WDOG_CS_CMD32EN_MASK|WDOG_CS_EN_MASK|WDOG_CS_UPDATE_MASK|WDOG_CS_CLK(1u);
}
#endif
void AppHealth_RunTrialAction(void)
{
    BootAppStatusType status;
    if(Boot_GetBootStatus(&status)!=BOOT_APP_IF_OK||status!=BOOT_APP_STATUS_TRIAL){return;}
#if APP_TRIAL_MODE == APP_TRIAL_MODE_NORMAL
    if(AppHealth_CheckBootReady()!=0u){(void)Boot_ConfirmApplication();}
#elif APP_TRIAL_MODE == APP_TRIAL_MODE_NO_CONFIRM
    (void)fatal_error;
#elif APP_TRIAL_MODE == APP_TRIAL_MODE_WATCHDOG_RESET
    enable_short_watchdog(); for(;;){}
#elif APP_TRIAL_MODE == APP_TRIAL_MODE_HARDFAULT_TEST
    __asm volatile("udf #0");
#elif APP_TRIAL_MODE == APP_TRIAL_MODE_RESET_LOOP
    AppBootRequest_ResetWithoutRequest();
#elif APP_TRIAL_MODE == APP_TRIAL_MODE_DELAY_CONFIRM
    if(xTaskGetTickCount()>=pdMS_TO_TICKS(10000u)&&AppHealth_CheckBootReady()!=0u){(void)Boot_ConfirmApplication();}
#else
#error "Unsupported APP_TRIAL_MODE"
#endif
}

#if APP_TRIAL_MODE == APP_TRIAL_MODE_HARDFAULT_TEST
void HardFault_Handler(void){AppBootRequest_RecordFaultAndReset();}
#endif
