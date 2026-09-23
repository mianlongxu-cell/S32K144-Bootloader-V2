#ifndef BOOT_CAN_H_
#define BOOT_CAN_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
} BootCan_FrameType;

void BootCan_Init(void);
bool BootCan_Receive(BootCan_FrameType *frame);
bool BootCan_Transmit(const BootCan_FrameType *frame);

#endif /* BOOT_CAN_H_ */
