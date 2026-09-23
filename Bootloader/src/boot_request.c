#include "device_registers.h"
#include "boot_config.h"
#include "boot_request.h"

static bool retained_request(uint32_t magic)
{
    const volatile uint32_t *flag = (const volatile uint32_t *)BOOT_REQUEST_ADDRESS;
    return Boot_WasSoftwareReset() && flag[0] == magic && flag[1] == ~magic &&
           flag[2] == ~flag[3];
}

void Boot_RequestInit(void)
{
    IP_PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;
    IP_PORTC->PCR[BOOT_REQUEST_PIN] = PORT_PCR_MUX(1u) |
                                             PORT_PCR_PE(1u) |
                                             PORT_PCR_PS(0u);//GPIO 下拉
    IP_PTC->PDDR &= ~(1u << BOOT_REQUEST_PIN);
}

bool Boot_IsBootRequested(void)//判断硬件是否请求升级
{
    return ((IP_PTC->PDIR & (1u << BOOT_REQUEST_PIN)) != 0u);//检测按键高电平
}

bool Boot_WasSoftwareReset(void)//判断是否发生过软件复位
{
    return (IP_RCM->SRS & RCM_SRS_SW_MASK) != 0u;
}

bool Boot_IsSoftwareBootRequested(void)//判断 Application 是否发出了软件启动请求
{
    return retained_request(BOOT_REQUEST_MAGIC);
}

bool Boot_IsApplicationConfirmRequested(void)
{ return retained_request(BOOT_CONTRACT_CONFIRM_MAGIC); }

bool Boot_GetRequestedSlot(BootSlotIdType *slot)
{
    const volatile uint32_t *flag=(const volatile uint32_t*)BOOT_REQUEST_ADDRESS;
    if(slot==0||flag[2]>1u||flag[2]!=~flag[3]){return false;}
    *slot=(BootSlotIdType)flag[2];return true;
}

void Boot_ClearSoftwareBootRequest(void)//清除软件请求标志
{
    volatile uint32_t *flag = (volatile uint32_t *)BOOT_REQUEST_ADDRESS;
    flag[0] = 0u;
    flag[1] = 0u;
    flag[2] = 0u;
    flag[3] = 0u;
}
