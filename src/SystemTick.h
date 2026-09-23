#ifndef SYSTEM_TICK_H_
#define SYSTEM_TICK_H_

#include <stdint.h>

extern volatile uint32_t SystemTimeMs;

void SystemTick_Init(void);
uint32_t SystemTick_GetMs(void);
uint8_t SystemTick_IsElapsed(uint32_t *last_ms, uint32_t period_ms);

#endif /* SYSTEM_TICK_H_ */
//使用的是 S32K144 的 LPIT0 定时器
