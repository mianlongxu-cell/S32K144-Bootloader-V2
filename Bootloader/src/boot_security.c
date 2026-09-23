#include "device_registers.h"
#include "boot_protocol_cfg.h"
#include "boot_security.h"

static uint32_t BootSecurity_Seed;
static bool BootSecurity_SeedIssued;
static bool BootSecurity_Unlocked;

static uint32_t BootSecurity_RotateLeft(uint32_t value, uint8_t count)
{
    return (value << count) | (value >> (32u - count));
}

void BootSecurity_Init(void)
{
    BootSecurity_Seed = 0u;
    BootSecurity_SeedIssued = false;
    BootSecurity_Unlocked = false;
}

uint32_t BootSecurity_GenerateSeed(void)
{
    static uint32_t nonce = 0x13579BDFu;
    BootSecurity_Unlocked = false;
    nonce = (nonce * 1664525u) + 1013904223u;
    BootSecurity_Seed = nonce ^ IP_FLEXCAN0->TIMER;
    if (BootSecurity_Seed == 0u)
    {
        BootSecurity_Seed = 0x6D2B79F5u;
    }
    BootSecurity_SeedIssued = true;
    return BootSecurity_Seed;
}

bool BootSecurity_ValidateKey(uint32_t key)
{
    const uint32_t expected =
        BootSecurity_RotateLeft(BootSecurity_Seed ^ BOOT_SECURITY_XOR,
                                BOOT_SECURITY_ROTATE);
    if (!BootSecurity_SeedIssued || (key != expected))
    {
        BootSecurity_Unlocked = false;
        BootSecurity_SeedIssued = false;
        return false;
    }
    BootSecurity_SeedIssued = false;
    BootSecurity_Unlocked = true;
    return true;
}

bool BootSecurity_IsUnlocked(void)
{
    return BootSecurity_Unlocked;
}
