#ifndef APP_HEALTH_H
#define APP_HEALTH_H
#include <stdint.h>
#define APP_TRIAL_MODE_NORMAL 0
#define APP_TRIAL_MODE_NO_CONFIRM 1
#define APP_TRIAL_MODE_WATCHDOG_RESET 2
#define APP_TRIAL_MODE_HARDFAULT_TEST 3
#define APP_TRIAL_MODE_RESET_LOOP 4
#define APP_TRIAL_MODE_DELAY_CONFIRM 5
#ifndef APP_TRIAL_MODE
#define APP_TRIAL_MODE APP_TRIAL_MODE_NORMAL
#endif
uint8_t AppHealth_CheckBootReady(void);
void AppHealth_ReportFatal(void);
void AppHealth_RunTrialAction(void);
#endif
