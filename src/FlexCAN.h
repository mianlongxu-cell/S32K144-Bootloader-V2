#ifndef FLEXCAN_H_
#define FLEXCAN_H_

#include <stdint.h>

#define FLEXCAN_CLASSIC_MAX_DLC  8u

void FLEXCAN0_init(uint32_t control_rx_id, uint32_t diag_rx_id);
uint8_t FLEXCAN0_transmit_msg(
    uint32_t can_id,
    uint8_t dlc,
    const uint8_t data[FLEXCAN_CLASSIC_MAX_DLC]);

void CAN0_ORed_0_15_MB_IRQHandler(void);

#endif /* FLEXCAN_H_ */
