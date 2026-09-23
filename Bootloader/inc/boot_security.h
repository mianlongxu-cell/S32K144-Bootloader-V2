#ifndef BOOT_SECURITY_H_
#define BOOT_SECURITY_H_

#include <stdbool.h>
#include <stdint.h>

void BootSecurity_Init(void);
uint32_t BootSecurity_GenerateSeed(void);
bool BootSecurity_ValidateKey(uint32_t key);
bool BootSecurity_IsUnlocked(void);

#endif /* BOOT_SECURITY_H_ */
