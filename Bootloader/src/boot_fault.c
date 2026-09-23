#include "boot_fault.h"
#if BOOT_ENABLE_FAULT_INJECTION
#include "device_registers.h"
#include "s32_core_cm4.h"
volatile uint32_t BootFault_ArmedPoint, BootFault_SkipHits;
void BootFault_Hit(BootFaultPoint point)
{
    if (BootFault_ArmedPoint != (uint32_t)point) { return; }
    if (BootFault_SkipHits != 0u) { BootFault_SkipHits--; return; }
    BootFault_ArmedPoint = 0u;
    S32_SCB->AIRCR = S32_SCB_AIRCR_VECTKEY(0x5FAu) | S32_SCB_AIRCR_SYSRESETREQ_MASK;
    for (;;) { }
}
#endif
