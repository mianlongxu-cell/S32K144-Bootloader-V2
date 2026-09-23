#ifndef UJA1169_H
#define UJA1169_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    UJA1169_INIT_NOT_RUN = 0,
    UJA1169_INIT_OK_FORCED_NORMAL = 1,
    UJA1169_INIT_OK_NORMAL = 2,
    UJA1169_INIT_SPI_TIMEOUT = 3,
    UJA1169_INIT_BAD_DEVICE = 4,
    UJA1169_INIT_NOT_ACTIVE = 5
} Uja1169InitResultType;

extern volatile uint8_t Uja1169DeviceId;
extern volatile uint8_t Uja1169MainStatus;
extern volatile uint8_t Uja1169WatchdogStatus;
extern volatile uint8_t Uja1169ConfigStatus;
extern volatile uint8_t Uja1169SupplyStatus;
extern volatile uint8_t Uja1169CanStatus;
extern volatile uint8_t Uja1169ReadAttempts;
extern volatile Uja1169InitResultType Uja1169InitResult;

bool Uja1169_InitCanNormal(void);

#endif
