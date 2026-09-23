#ifndef CAN_IF_CFG_H_
#define CAN_IF_CFG_H_

#include <stdint.h>

#define CANIF_CFG_CONTROL_RX_CAN_ID  0x200u
#define CANIF_CFG_DIAG_RX_CAN_ID     0x7E0u

extern const uint32_t CanIfCfg_ControlRxCanId;
extern const uint32_t CanIfCfg_DiagRxCanId;

#endif /* CAN_IF_CFG_H_ */
