#include "boot.h"
#include "boot_manager.h"

void Boot_Init(void)
{
    BootManager_Init();
}

void Boot_Run(void)
{
    BootManager_Run();
}
