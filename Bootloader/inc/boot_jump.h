#ifndef BOOT_JUMP_H_
#define BOOT_JUMP_H_

#include <stdbool.h>
#include <stdint.h>
#include "boot_types.h"

bool BootJump_ToVector(uint32_t vector_address);
bool BootJump_ToSlot(BootSlotIdType slot);
void Boot_JumpToApplication(void);

#endif /* BOOT_JUMP_H_ */
