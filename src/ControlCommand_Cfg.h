#ifndef CONTROL_COMMAND_CFG_H_
#define CONTROL_COMMAND_CFG_H_

/* 0x200 Control Command payload byte 0 definitions. */
#define CMD_START_TX          0x01u
#define CMD_STOP_TX           0x02u
#define CMD_RESET_SPEED       0x03u
#define CMD_INJECT_OVERTEMP   0x04u
#define CMD_CLEAR_DTC         0x05u
#define CMD_TOGGLE_LIGHT      0x06u
#define CMD_TOGGLE_DOOR       0x07u
#define CMD_TOGGLE_WIPER      0x08u
#define CMD_TOGGLE_LOCK       0x09u
#define CMD_SET_TURN_SIGNAL   0x0Au
#define CMD_CONTROL_HEARTBEAT 0x0Bu

#endif /* CONTROL_COMMAND_CFG_H_ */
