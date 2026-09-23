#include "S32K144.h"
#include "FreeRTOS.h"
#include "task.h"
#include "App.h"
#include "AppRTOS.h"
#include "CanIf.h"
#include "clocks_and_modes.h"
#include "Uja1169.h"

#define GREEN_LED_PIN  16u

static void WDOG_disable(void)
{
    IP_WDOG->CNT = 0xD928C520u;
    (void)IP_WDOG->CNT;
    IP_WDOG->TOVAL = 0x0000FFFFu;
    /* UPDATE remains enabled so WATCHDOG_RESET test builds can reconfigure it. */
    IP_WDOG->CS = 0x00002120u;
}

static void PORT_init(void)
{
    IP_PCC->PCCn[PCC_PORTE_INDEX] |= PCC_PCCn_CGC_MASK;
    IP_PORTE->PCR[4] = PORT_PCR_MUX(5u);  /* PTE4: CAN0_RX */
    IP_PORTE->PCR[5] = PORT_PCR_MUX(5u);  /* PTE5: CAN0_TX */

    IP_PCC->PCCn[PCC_PORTD_INDEX] |= PCC_PCCn_CGC_MASK;
    IP_PORTD->PCR[GREEN_LED_PIN] = PORT_PCR_MUX(1u);
    IP_PTD->PDDR |= (1u << GREEN_LED_PIN);
    IP_PTD->PSOR = (1u << GREEN_LED_PIN);  /* Active-low LED off. */
}

int main(void)
{
    WDOG_disable();
    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();

    PORT_init();
    (void)Uja1169_InitCanNormal();
    App_Init();//初始化应用模块
    if (AppRTOS_Init() != 0u)//创建FreeRTOS对象和任务
    {
        CanIf_Init();
        vTaskStartScheduler();//启动 FreeRTOS 调度器
    }

    for (;;)
    {
    }
}
