#include "S32K144.h"
#include "CanIf.h"
#include "FlexCAN.h"

#define FLEXCAN0                  IP_FLEXCAN0
#define FLEXCAN_MSG_BUF_SIZE      4u
#define FLEXCAN_TX_MB             0u
#define FLEXCAN_CONTROL_RX_MB     4u
#define FLEXCAN_DIAG_RX_MB        5u
#define FLEXCAN_CONTROL_RX_MASK   (1u << FLEXCAN_CONTROL_RX_MB)
#define FLEXCAN_DIAG_RX_MASK      (1u << FLEXCAN_DIAG_RX_MB)
#define FLEXCAN_RX_MB_MASKS       (FLEXCAN_CONTROL_RX_MASK | \
                                   FLEXCAN_DIAG_RX_MASK)
#define FLEXCAN_TX_MB_MASK        (1u << FLEXCAN_TX_MB)

#define FLEXCAN_CS_CODE_SHIFT     24u
#define FLEXCAN_CS_DLC_SHIFT      16u
#define FLEXCAN_CS_SRR_MASK       (1u << 22u)
#define FLEXCAN_CODE_RX_EMPTY     4u
#define FLEXCAN_CODE_TX_INACTIVE  8u
#define FLEXCAN_CODE_TX_ABORT     9u
#define FLEXCAN_CODE_TX_DATA      12u
#define FLEXCAN_STD_ID_SHIFT      18u
#define FLEXCAN_STD_ID_MASK       0x7FFu

#define NVIC_ISER_BASE            0xE000E100u
#define NVIC_ICPR_BASE            0xE000E280u
#define NVIC_IPR_BASE             0xE000E400u
#define NVIC_ISER                 ((volatile uint32_t *)NVIC_ISER_BASE)
#define NVIC_ICPR                 ((volatile uint32_t *)NVIC_ICPR_BASE)
#define NVIC_IPR                  ((volatile uint8_t *)NVIC_IPR_BASE)
#define NVIC_PRIORITY_SHIFT       4u
#define FLEXCAN_RX_IRQ_PRIORITY   5u

static uint32_t flexcan_mb_index(uint32_t mb)
{
    return mb * FLEXCAN_MSG_BUF_SIZE;
}

static uint32_t flexcan_mb_cs(uint32_t code, uint32_t dlc)
{
    return ((code & 0xFu) << FLEXCAN_CS_CODE_SHIFT) |
           ((dlc & 0xFu) << FLEXCAN_CS_DLC_SHIFT);
}

static uint32_t flexcan_std_id(uint32_t id)
{
    return (id & FLEXCAN_STD_ID_MASK) << FLEXCAN_STD_ID_SHIFT;
}

static uint32_t flexcan_pack_word(const uint8_t data[4])
{
    return ((uint32_t)data[0] << 24u) |
           ((uint32_t)data[1] << 16u) |
           ((uint32_t)data[2] << 8u) |
           (uint32_t)data[3];
}

static void FLEXCAN0_enter_freeze(void)
{
    FLEXCAN0->MCR |= FLEXCAN_MCR_FRZ_MASK | FLEXCAN_MCR_HALT_MASK;

    while ((FLEXCAN0->MCR & FLEXCAN_MCR_FRZACK_MASK) == 0u)
    {
    }
}

static void FLEXCAN0_exit_freeze(void)
{
    FLEXCAN0->MCR &= ~(FLEXCAN_MCR_HALT_MASK | FLEXCAN_MCR_FRZ_MASK);

    while ((FLEXCAN0->MCR & FLEXCAN_MCR_FRZACK_MASK) != 0u)
    {
    }

    while ((FLEXCAN0->MCR & FLEXCAN_MCR_NOTRDY_MASK) != 0u)
    {
    }
}

static void FLEXCAN0_enable_rx_irq(void)
{
    const uint32_t irq = (uint32_t)CAN0_ORed_0_15_MB_IRQn;
    const uint32_t mask = 1u << (irq & 0x1Fu);

    NVIC_ICPR[irq >> 5u] = mask;
    NVIC_IPR[irq] = (uint8_t)(FLEXCAN_RX_IRQ_PRIORITY <<
                              NVIC_PRIORITY_SHIFT);
    NVIC_ISER[irq >> 5u] = mask;
}

void FLEXCAN0_init(uint32_t control_rx_id, uint32_t diag_rx_id)
{
    uint32_t i;
    const uint32_t tx_mb = flexcan_mb_index(FLEXCAN_TX_MB);
    const uint32_t control_rx_mb = flexcan_mb_index(FLEXCAN_CONTROL_RX_MB);
    const uint32_t diag_rx_mb = flexcan_mb_index(FLEXCAN_DIAG_RX_MB);

    IP_PCC->PCCn[PCC_FlexCAN0_INDEX] |= PCC_PCCn_CGC_MASK;

    FLEXCAN0->MCR |= FLEXCAN_MCR_MDIS_MASK;
    FLEXCAN0->CTRL1 &= ~FLEXCAN_CTRL1_CLKSRC_MASK;
    FLEXCAN0->MCR &= ~FLEXCAN_MCR_MDIS_MASK;

    while ((FLEXCAN0->MCR & FLEXCAN_MCR_LPMACK_MASK) != 0u)
    {
    }

    FLEXCAN0_enter_freeze();

    /* SOSCDIV2_CLK=8 MHz, PRESDIV=0, 16 time quanta: 500 kbit/s. */
    FLEXCAN0->CTRL1 = FLEXCAN_CTRL1_PRESDIV(0u) |
                      FLEXCAN_CTRL1_PSEG2(3u) |
                      FLEXCAN_CTRL1_PSEG1(3u) |
                      FLEXCAN_CTRL1_PROPSEG(6u) |
                      FLEXCAN_CTRL1_RJW(3u) |
                      FLEXCAN_CTRL1_SMP(1u);

    FLEXCAN0->MCR = (FLEXCAN0->MCR & ~FLEXCAN_MCR_MAXMB_MASK) |
                    FLEXCAN_MCR_MAXMB(31u);

    for (i = 0u; i < FLEXCAN_RAMn_COUNT; i++)
    {
        FLEXCAN0->RAMn[i] = 0u;
    }

    for (i = 0u; i < FLEXCAN_RXIMR_COUNT; i++)
    {
        FLEXCAN0->RXIMR[i] = 0xFFFFFFFFu;
    }

    FLEXCAN0->RXMGMASK = 0x1FFFFFFFu;
    FLEXCAN0->IMASK1 = 0u;
    FLEXCAN0->IFLAG1 = 0xFFFFFFFFu;

    FLEXCAN0->RAMn[tx_mb + 0u] =
        flexcan_mb_cs(FLEXCAN_CODE_TX_INACTIVE, 0u);

    FLEXCAN0->RAMn[control_rx_mb + 0u] =
        flexcan_mb_cs(FLEXCAN_CODE_RX_EMPTY, 0u);
    FLEXCAN0->RAMn[control_rx_mb + 1u] = flexcan_std_id(control_rx_id);

    FLEXCAN0->RAMn[diag_rx_mb + 0u] =
        flexcan_mb_cs(FLEXCAN_CODE_RX_EMPTY, 0u);
    FLEXCAN0->RAMn[diag_rx_mb + 1u] = flexcan_std_id(diag_rx_id);

    FLEXCAN0_exit_freeze();

    FLEXCAN0->IFLAG1 = FLEXCAN_RX_MB_MASKS;
    FLEXCAN0->IMASK1 = FLEXCAN_RX_MB_MASKS;
    FLEXCAN0_enable_rx_irq();
}

uint8_t FLEXCAN0_transmit_msg(
    uint32_t can_id,
    uint8_t dlc,
    const uint8_t data[FLEXCAN_CLASSIC_MAX_DLC])
{
    uint32_t code;
    const uint32_t tx_mb = flexcan_mb_index(FLEXCAN_TX_MB);

    if ((can_id > FLEXCAN_STD_ID_MASK) ||
        (dlc > FLEXCAN_CLASSIC_MAX_DLC))
    {
        return 0u;
    }

    code = (FLEXCAN0->RAMn[tx_mb + 0u] >> FLEXCAN_CS_CODE_SHIFT) & 0xFu;
    if ((code == FLEXCAN_CODE_TX_DATA) ||
        (code == FLEXCAN_CODE_TX_ABORT))
    {
        return 0u;
    }

    FLEXCAN0->IFLAG1 = FLEXCAN_TX_MB_MASK;
    FLEXCAN0->RAMn[tx_mb + 2u] = flexcan_pack_word(&data[0]);
    FLEXCAN0->RAMn[tx_mb + 3u] = flexcan_pack_word(&data[4]);
    FLEXCAN0->RAMn[tx_mb + 1u] = flexcan_std_id(can_id);
    FLEXCAN0->RAMn[tx_mb + 0u] =
        flexcan_mb_cs(FLEXCAN_CODE_TX_DATA, dlc) |
        FLEXCAN_CS_SRR_MASK;

    return 1u;
}

static void FLEXCAN0_receive_mb(uint32_t mb, uint32_t mb_mask)
{
    uint32_t dummy;
    uint32_t cs;
    uint32_t id_word;
    uint32_t data_word0;
    uint32_t data_word1;
    CanFrame_t frame;
    const uint32_t rx_mb = flexcan_mb_index(mb);

    cs = FLEXCAN0->RAMn[rx_mb + 0u];
    id_word = FLEXCAN0->RAMn[rx_mb + 1u];
    data_word0 = FLEXCAN0->RAMn[rx_mb + 2u];
    data_word1 = FLEXCAN0->RAMn[rx_mb + 3u];

    frame.id = (id_word >> FLEXCAN_STD_ID_SHIFT) & FLEXCAN_STD_ID_MASK;
    frame.dlc = (uint8_t)((cs >> FLEXCAN_CS_DLC_SHIFT) & 0xFu);
    if (frame.dlc > FLEXCAN_CLASSIC_MAX_DLC)
    {
        frame.dlc = FLEXCAN_CLASSIC_MAX_DLC;
    }

    frame.data[0] = (uint8_t)(data_word0 >> 24u);
    frame.data[1] = (uint8_t)(data_word0 >> 16u);
    frame.data[2] = (uint8_t)(data_word0 >> 8u);
    frame.data[3] = (uint8_t)data_word0;
    frame.data[4] = (uint8_t)(data_word1 >> 24u);
    frame.data[5] = (uint8_t)(data_word1 >> 16u);
    frame.data[6] = (uint8_t)(data_word1 >> 8u);
    frame.data[7] = (uint8_t)data_word1;

    /* Reading TIMER unlocks the receive MB before its W1C flag is cleared. */
    dummy = FLEXCAN0->TIMER;
    (void)dummy;

    CanIf_RxIndication(&frame);
    FLEXCAN0->IFLAG1 = mb_mask;
}

void CAN0_ORed_0_15_MB_IRQHandler(void)
{
    const uint32_t flags = FLEXCAN0->IFLAG1 & FLEXCAN_RX_MB_MASKS;

    if ((flags & FLEXCAN_CONTROL_RX_MASK) != 0u)
    {
        FLEXCAN0_receive_mb(FLEXCAN_CONTROL_RX_MB,
                            FLEXCAN_CONTROL_RX_MASK);
    }

    if ((flags & FLEXCAN_DIAG_RX_MASK) != 0u)
    {
        FLEXCAN0_receive_mb(FLEXCAN_DIAG_RX_MB,
                            FLEXCAN_DIAG_RX_MASK);
    }
}
