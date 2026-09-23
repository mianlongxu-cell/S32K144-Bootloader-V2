#include "S32K144.h"
#include "SystemTick.h"

#define SYSTEM_TICK_LPIT_CLOCK_HZ  40000000u//LPIT 输入时钟
#define SYSTEM_TICK_PERIOD_HZ      1000u//目标中断频率
#define SYSTEM_TICK_LPIT_COUNTS    \
    (SYSTEM_TICK_LPIT_CLOCK_HZ / SYSTEM_TICK_PERIOD_HZ)//每计数约 40000 个时钟周期

#define NVIC_ISER_BASE             (0xE000E100u)
#define NVIC_ICPR_BASE             (0xE000E280u)
#define NVIC_IPR_BASE              (0xE000E400u)
#define NVIC_ISER                  ((volatile uint32_t *)NVIC_ISER_BASE)
#define NVIC_ICPR                  ((volatile uint32_t *)NVIC_ICPR_BASE)
#define NVIC_IPR                   ((volatile uint8_t *)NVIC_IPR_BASE)
#define NVIC_PRIORITY_SHIFT        4u
#define SYSTEM_TICK_IRQ_PRIORITY   10u

volatile uint32_t SystemTimeMs;

static void SystemTick_EnableIrq(void)
{
    const uint32_t irq = (uint32_t)LPIT0_Ch0_IRQn;
    const uint32_t mask = 1u << (irq & 0x1Fu);

    NVIC_ICPR[irq >> 5u] = mask;
    NVIC_IPR[irq] = (uint8_t)(SYSTEM_TICK_IRQ_PRIORITY <<
                              NVIC_PRIORITY_SHIFT);
    NVIC_ISER[irq >> 5u] = mask;
}

void SystemTick_Init(void)
{
    SystemTimeMs = 0u;

    /* PCS=6 selects SPLLDIV2_CLK, configured to 40 MHz. */
    IP_PCC->PCCn[PCC_LPIT_INDEX] = PCC_PCCn_PCS(6u);
    IP_PCC->PCCn[PCC_LPIT_INDEX] |= PCC_PCCn_CGC_MASK;
    (void)IP_PCC->PCCn[PCC_LPIT_INDEX];

    IP_LPIT0->MCR = LPIT_MCR_M_CEN_MASK;//开启LPIT模块
    IP_LPIT0->TMR[0].TCTRL = 0u;
    IP_LPIT0->MIER = 0u;
    IP_LPIT0->MSR = LPIT_MSR_TIF0_MASK;//清除旧的超时标志
    IP_LPIT0->TMR[0].TVAL =
        LPIT_TMR_TVAL_TMR_VAL(SYSTEM_TICK_LPIT_COUNTS);//清除旧的超时标志
    IP_LPIT0->MIER = LPIT_MIER_TIE0_MASK;//开启通道0中断

    SystemTick_EnableIrq();//配置NVIC
    IP_LPIT0->TMR[0].TCTRL = LPIT_TMR_TCTRL_T_EN_MASK;//启动通道0
}

uint32_t SystemTick_GetMs(void)
{
    return SystemTimeMs;
}

uint8_t SystemTick_IsElapsed(uint32_t *last_ms, uint32_t period_ms)//判断“某个周期任务是否到执行时间”，并自动更新该任务的时间基准，返回 1：周期已到
{
    const uint32_t now_ms = SystemTick_GetMs();

    if ((period_ms != 0u) &&
        ((uint32_t)(now_ms - *last_ms) >= period_ms))
    {
        *last_ms += period_ms;
        return 1u;
    }

    return 0u;
}

void LPIT0_Ch0_IRQHandler(void)//LPIT中断函数
{
    SystemTimeMs++;
    IP_LPIT0->MSR = LPIT_MSR_TIF0_MASK;//清除 LPIT 通道0中断标志
}
