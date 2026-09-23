#include "S32K144.h"
#include "system_S32K144.h"
#include "clocks_and_modes.h"

void SOSC_init_8MHz(void) //初始化外部晶振
{
    IP_SCG->SOSCDIV = SCG_SOSCDIV_SOSCDIV1(1u) |
                      SCG_SOSCDIV_SOSCDIV2(1u);
    IP_SCG->SOSCCFG = SCG_SOSCCFG_RANGE(2u) |
                      SCG_SOSCCFG_EREFS_MASK;

    while ((IP_SCG->SOSCCSR & SCG_SOSCCSR_LK_MASK) != 0u)
    {
    }

    IP_SCG->SOSCCSR = SCG_SOSCCSR_SOSCEN_MASK;

    while ((IP_SCG->SOSCCSR & SCG_SOSCCSR_SOSCVLD_MASK) == 0u)
    {
    }
}

void SPLL_init_160MHz(void)//初始化160MHZ PLL
{
    while ((IP_SCG->SPLLCSR & SCG_SPLLCSR_LK_MASK) != 0u)
    {
    }

    IP_SCG->SPLLCSR &= ~SCG_SPLLCSR_SPLLEN_MASK;

    IP_SCG->SPLLDIV = SCG_SPLLDIV_SPLLDIV1(2u) |
                      SCG_SPLLDIV_SPLLDIV2(3u);
    IP_SCG->SPLLCFG = SCG_SPLLCFG_MULT(24u);
/*
 * VCO_CLK  = SPLL_SOURCE / (PREDIV + 1) * (MULT + 16)
    SPLL_CLK = VCO_CLK / 2
    */
    while ((IP_SCG->SPLLCSR & SCG_SPLLCSR_LK_MASK) != 0u)
    {
    }

    IP_SCG->SPLLCSR |= SCG_SPLLCSR_SPLLEN_MASK;

    while ((IP_SCG->SPLLCSR & SCG_SPLLCSR_SPLLVLD_MASK) == 0u)
    {
    }
}

void NormalRUNmode_80MHz(void)
{
    IP_SCG->SIRCDIV = SCG_SIRCDIV_SIRCDIV1(1u) |
                      SCG_SIRCDIV_SIRCDIV2(1u);

    IP_SCG->RCCR = SCG_RCCR_SCS(6u) |    //0110 -> SPLL，系统时钟源选择 SPLL_CLK
                   SCG_RCCR_DIVCORE(1u) |  //core = 160m /2 =80m
                   SCG_RCCR_DIVBUS(1u) |   //bus clock =80m/2 =40 m
                   SCG_RCCR_DIVSLOW(2u);    //slow clock = 80 /3 =26.67m

    while (((IP_SCG->CSR & SCG_CSR_SCS_MASK) >> SCG_CSR_SCS_SHIFT) != 6u)
    {
    }

    SystemCoreClockUpdate();
}
