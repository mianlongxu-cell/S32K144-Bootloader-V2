#ifndef APP_BOOT_REQUEST_H_
#define APP_BOOT_REQUEST_H_

#include <stdint.h>

void AppBootRequest_RequestProgrammingReset(void);
void AppBootRequest_RequestConfirmReset(void);
void AppBootRequest_RecordFaultAndReset(void) __attribute__((noreturn));
void AppBootRequest_ResetWithoutRequest(void) __attribute__((noreturn));
void AppBootRequest_MainFunction(void);

#endif /* APP_BOOT_REQUEST_H_ */
