#ifndef APP_RTOS_H_
#define APP_RTOS_H_

#include <stdint.h>

extern volatile uint32_t AppRTOSTxCount;

uint8_t AppRTOS_Init(void);

#endif /* APP_RTOS_H_ */
