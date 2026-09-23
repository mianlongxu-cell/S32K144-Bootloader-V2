#ifndef BOOT_H_
#define BOOT_H_

typedef enum
{
    BOOT_STATE_INIT = 0,
    BOOT_STATE_CHECK_REQUEST,
    BOOT_STATE_CHECK_APP,
    BOOT_STATE_STAY,
    BOOT_STATE_PROGRAMMING,
    BOOT_STATE_RESET,
    BOOT_STATE_JUMP_APP,
    BOOT_STATE_ERROR
} Boot_StateType;

extern volatile Boot_StateType Boot_CurrentState;

void Boot_Init(void);
void Boot_Run(void);

#endif /* BOOT_H_ */
