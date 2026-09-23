#ifndef BOOT_IMAGE_INSTALL_H
#define BOOT_IMAGE_INSTALL_H
#include "boot_types.h"
/* Same immutable-header validation/publication path for live and recovery. */
bool BootImageInstall_ValidateAndPublish(BootSlotIdType active, BootSlotIdType target,
                                       const BootImageHeaderType *header);
#endif
