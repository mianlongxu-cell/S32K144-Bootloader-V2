#include "device_registers.h"
#include "boot_can.h"
#include "boot_protocol_cfg.h"
#include "Uja1169.h"

#define BOOT_CAN_TX_MB             0u
#define BOOT_CAN_RX_MB             4u
#define BOOT_CAN_MB_WORDS          4u
#define BOOT_CAN_RX_MASK           (1u << BOOT_CAN_RX_MB)
#define BOOT_CAN_TX_MASK           (1u << BOOT_CAN_TX_MB)
#define BOOT_CAN_CODE_SHIFT        24u
#define BOOT_CAN_DLC_SHIFT         16u
#define BOOT_CAN_STD_ID_SHIFT      18u
#define BOOT_CAN_CODE_RX_EMPTY     4u
#define BOOT_CAN_CODE_TX_INACTIVE  8u
#define BOOT_CAN_CODE_TX_DATA      12u

static uint32_t BootCan_MbIndex(uint32_t mb)
{
    return mb * BOOT_CAN_MB_WORDS;
}

static uint32_t BootCan_Pack(const uint8_t *data)//CAN 帧数据打包
{
    return ((uint32_t)data[0] << 24u) | ((uint32_t)data[1] << 16u) |
           ((uint32_t)data[2] << 8u) | (uint32_t)data[3];
}

static void BootCan_InitClock(void)//初始化 CAN 时钟
{
    IP_SCG->SOSCDIV = SCG_SOSCDIV_SOSCDIV1(1u) |
                      SCG_SOSCDIV_SOSCDIV2(1u);
    IP_SCG->SOSCCFG = SCG_SOSCCFG_RANGE(2u) | SCG_SOSCCFG_EREFS_MASK;
    IP_SCG->SOSCCSR = SCG_SOSCCSR_SOSCEN_MASK;
    while ((IP_SCG->SOSCCSR & SCG_SOSCCSR_SOSCVLD_MASK) == 0u)
    {
    }
}

void BootCan_Init(void)//配置 CAN 引脚
{
    uint32_t index;
    const uint32_t tx = BootCan_MbIndex(BOOT_CAN_TX_MB);
    const uint32_t rx = BootCan_MbIndex(BOOT_CAN_RX_MB);

    BootCan_InitClock();
    (void)Uja1169_InitCanNormal();
    IP_PCC->PCCn[PCC_PORTE_INDEX] |= PCC_PCCn_CGC_MASK;
    IP_PORTE->PCR[4] = PORT_PCR_MUX(5u);
    IP_PORTE->PCR[5] = PORT_PCR_MUX(5u);
    IP_PCC->PCCn[PCC_FlexCAN0_INDEX] |= PCC_PCCn_CGC_MASK;//打开 FlexCAN 时钟

    IP_FLEXCAN0->MCR |= FLEXCAN_MCR_MDIS_MASK;
    IP_FLEXCAN0->CTRL1 &= ~FLEXCAN_CTRL1_CLKSRC_MASK;
    IP_FLEXCAN0->MCR &= ~FLEXCAN_MCR_MDIS_MASK;//进入冻结模式
    while ((IP_FLEXCAN0->MCR & FLEXCAN_MCR_LPMACK_MASK) != 0u)
    {
    }

    IP_FLEXCAN0->MCR |= FLEXCAN_MCR_FRZ_MASK | FLEXCAN_MCR_HALT_MASK;
    while ((IP_FLEXCAN0->MCR & FLEXCAN_MCR_FRZACK_MASK) == 0u)
    {
    }
    IP_FLEXCAN0->CTRL1 = FLEXCAN_CTRL1_PRESDIV(0u) |
                         FLEXCAN_CTRL1_PSEG2(3u) |
                         FLEXCAN_CTRL1_PSEG1(3u) |
                         FLEXCAN_CTRL1_PROPSEG(6u) |
                         FLEXCAN_CTRL1_RJW(3u) |
                         FLEXCAN_CTRL1_SMP(1u);
    IP_FLEXCAN0->MCR = (IP_FLEXCAN0->MCR & ~FLEXCAN_MCR_MAXMB_MASK) |
                       FLEXCAN_MCR_MAXMB(31u);
    for (index = 0u; index < FLEXCAN_RAMn_COUNT; index++)
    {
        IP_FLEXCAN0->RAMn[index] = 0u;
    }
    for (index = 0u; index < FLEXCAN_RXIMR_COUNT; index++)
    {
        IP_FLEXCAN0->RXIMR[index] = 0xFFFFFFFFu;
    }
    IP_FLEXCAN0->RXMGMASK = 0x1FFFFFFFu;
    IP_FLEXCAN0->IMASK1 = 0u;
    IP_FLEXCAN0->IFLAG1 = 0xFFFFFFFFu;
    IP_FLEXCAN0->RAMn[tx] = BOOT_CAN_CODE_TX_INACTIVE << BOOT_CAN_CODE_SHIFT;
    IP_FLEXCAN0->RAMn[rx] = BOOT_CAN_CODE_RX_EMPTY << BOOT_CAN_CODE_SHIFT;
    IP_FLEXCAN0->RAMn[rx + 1u] =
        BOOT_CAN_REQUEST_ID << BOOT_CAN_STD_ID_SHIFT;

    IP_FLEXCAN0->MCR &= ~(FLEXCAN_MCR_HALT_MASK | FLEXCAN_MCR_FRZ_MASK);
    while ((IP_FLEXCAN0->MCR & (FLEXCAN_MCR_FRZACK_MASK |
                                FLEXCAN_MCR_NOTRDY_MASK)) != 0u)
    {
    }
}

bool BootCan_Receive(BootCan_FrameType *frame)//接收 CAN 帧
{
    uint32_t dummy;
    uint32_t word0;
    uint32_t word1;
    uint32_t cs;
    const uint32_t rx = BootCan_MbIndex(BOOT_CAN_RX_MB);

    if ((frame == (BootCan_FrameType *)0) ||
        ((IP_FLEXCAN0->IFLAG1 & BOOT_CAN_RX_MASK) == 0u))
    {
        return false;
    }
    cs = IP_FLEXCAN0->RAMn[rx];//取 Mailbox 的控制/状态字
    frame->id = (IP_FLEXCAN0->RAMn[rx + 1u] >> BOOT_CAN_STD_ID_SHIFT) &
                0x7FFu;//取出 11 位标准 CAN ID
    frame->dlc = (uint8_t)((cs >> BOOT_CAN_DLC_SHIFT) & 0xFu);//取出 DLC，也就是数据长度
    if (frame->dlc > 8u)
    {
        frame->dlc = 8u;
    }
    word0 = IP_FLEXCAN0->RAMn[rx + 2u];
    word1 = IP_FLEXCAN0->RAMn[rx + 3u];//读取 8 字节数据
    frame->data[0] = (uint8_t)(word0 >> 24u);
    frame->data[1] = (uint8_t)(word0 >> 16u);
    frame->data[2] = (uint8_t)(word0 >> 8u);
    frame->data[3] = (uint8_t)word0;
    frame->data[4] = (uint8_t)(word1 >> 24u);
    frame->data[5] = (uint8_t)(word1 >> 16u);
    frame->data[6] = (uint8_t)(word1 >> 8u);
    frame->data[7] = (uint8_t)word1;
    dummy = IP_FLEXCAN0->TIMER;//解锁下一次接收
    (void)dummy;
    IP_FLEXCAN0->IFLAG1 = BOOT_CAN_RX_MASK;//清除接收中断标志
    return true;
}

bool BootCan_Transmit(const BootCan_FrameType *frame)
{
    uint32_t wait = 1000000u;
    uint32_t code;
    const uint32_t tx = BootCan_MbIndex(BOOT_CAN_TX_MB);

    if ((frame == (const BootCan_FrameType *)0) || (frame->dlc > 8u) ||
        (frame->id > 0x7FFu))
    {
        return false;
    }
    do
    {
        code = (IP_FLEXCAN0->RAMn[tx] >> BOOT_CAN_CODE_SHIFT) & 0xFu;
        wait--;
    } while ((code == BOOT_CAN_CODE_TX_DATA) && (wait != 0u));//等待发送 Mailbox 空闲
    if (wait == 0u)
    {
        return false;
    }
    IP_FLEXCAN0->IFLAG1 = BOOT_CAN_TX_MASK;
    IP_FLEXCAN0->RAMn[tx + 2u] = BootCan_Pack(&frame->data[0]);
    IP_FLEXCAN0->RAMn[tx + 3u] = BootCan_Pack(&frame->data[4]);
    IP_FLEXCAN0->RAMn[tx + 1u] = frame->id << BOOT_CAN_STD_ID_SHIFT;
    IP_FLEXCAN0->RAMn[tx] =
        (BOOT_CAN_CODE_TX_DATA << BOOT_CAN_CODE_SHIFT) |
        ((uint32_t)frame->dlc << BOOT_CAN_DLC_SHIFT) | (1u << 22u);
    return true;
}
