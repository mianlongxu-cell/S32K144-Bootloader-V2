#include "boot_can.h"
#include "boot_isotp.h"

#define ISOTP_TYPE_MASK  0xF0u
#define ISOTP_SF         0x00u
#define ISOTP_FF         0x10u
#define ISOTP_CF         0x20u
#define ISOTP_FC         0x30u

typedef enum
{
    ISOTP_RX_IDLE = 0,
    ISOTP_RX_WAIT_CF
} BootIsoTp_RxStateType;

static BootIsoTp_RxStateType BootIsoTp_RxState;
static BootIsoTp_PduType BootIsoTp_RxPdu;//保存已经拼装的完整 UDS 报文
static uint16_t BootIsoTp_RxOffset;//表示已经写入了多少字节
static uint8_t BootIsoTp_NextSn;//表示下一个 CF 应该使用的序号
static uint32_t BootIsoTp_Timeout;

static void BootIsoTp_ClearFrame(BootCan_FrameType *frame, uint32_t id)
{
    uint8_t index;
    frame->id = id;
    frame->dlc = 8u;
    for (index = 0u; index < 8u; index++)
    {
        frame->data[index] = 0u;
    }
}

static void BootIsoTp_SendFlowControl(void)//发送 Flow Control
{
    BootCan_FrameType frame;
    BootIsoTp_ClearFrame(&frame, BOOT_CAN_RESPONSE_ID);
    frame.data[0] = ISOTP_FC;
    frame.data[1] = BOOT_ISOTP_RX_BLOCK_SIZE;//不需要再次发送 FC
    frame.data[2] = BOOT_ISOTP_RX_STMIN;
    (void)BootCan_Transmit(&frame);
}

void BootIsoTp_Init(void)
{
    BootIsoTp_Abort();
}

void BootIsoTp_Abort(void)
{
    BootIsoTp_RxState = ISOTP_RX_IDLE;
    BootIsoTp_RxPdu.length = 0u;
    BootIsoTp_RxOffset = 0u;
    BootIsoTp_NextSn = 1u;
    BootIsoTp_Timeout = 0u;
}

bool BootIsoTp_MainFunction(BootIsoTp_PduType *request)//每次调用，它尝试接收一帧 CAN。
{
    BootCan_FrameType frame;
    uint16_t total;
    uint16_t remaining;
    uint8_t pci;
    uint8_t count;
    uint16_t index; /* May copy a complete 256-byte PDU without wrapping. */

    if (BootIsoTp_RxState == ISOTP_RX_WAIT_CF)
    {
        BootIsoTp_Timeout++;
        if (BootIsoTp_Timeout >= BOOT_ISOTP_TIMEOUT_LOOPS)
        {
            BootIsoTp_Abort();
        }
    }
    if (!BootCan_Receive(&frame) || (frame.id != BOOT_CAN_REQUEST_ID) ||
        (frame.dlc == 0u))
    {
        return false;
    }
    pci = frame.data[0] & ISOTP_TYPE_MASK;
    if (pci == ISOTP_SF)//SF 的第一个字节低 4 位表示有效数据长度
    {
        count = frame.data[0] & 0x0Fu;
        if ((count == 0u) || (count > 7u) ||
            ((uint8_t)(count + 1u) > frame.dlc))
        {
            return false;
        }
        BootIsoTp_Abort();
        request->length = count;
        for (index = 0u; index < count; index++)
        {
            request->data[index] = frame.data[index + 1u];
        }
        return true;
    }
    if (pci == ISOTP_FF)//处理多帧首帧
    {
        /* FF carries the 12-bit PDU length and the first six payload bytes.
         * FC grants the sender permission to continue with CF sequence 1. */
        total = ((uint16_t)(frame.data[0] & 0x0Fu) << 8u) |
                frame.data[1];
        if ((frame.dlc != 8u) || (total <= 7u) ||
            (total > BOOT_ISOTP_MAX_PDU_LENGTH))
        {
            BootIsoTp_Abort();
            return false;
        }
        BootIsoTp_Abort();
        BootIsoTp_RxPdu.length = total;
        for (index = 0u; index < 6u; index++)
        {
            BootIsoTp_RxPdu.data[index] = frame.data[index + 2u];
        }
        BootIsoTp_RxOffset = 6u;
        BootIsoTp_RxState = ISOTP_RX_WAIT_CF;
        BootIsoTp_Timeout = 0u;
        BootIsoTp_SendFlowControl();
        return false;
    }
    if ((pci == ISOTP_CF) && (BootIsoTp_RxState == ISOTP_RX_WAIT_CF))
    {
        /* Reject an unexpected 4-bit sequence number instead of assembling a
         * corrupted UDS programming request. */
        if ((frame.data[0] & 0x0Fu) != BootIsoTp_NextSn)
        {
            BootIsoTp_Abort();
            return false;
        }
        remaining = BootIsoTp_RxPdu.length - BootIsoTp_RxOffset;
        count = (remaining > 7u) ? 7u : (uint8_t)remaining;
        if (frame.dlc < (uint8_t)(count + 1u))
        {
            BootIsoTp_Abort();
            return false;
        }
        for (index = 0u; index < count; index++)
        {
            BootIsoTp_RxPdu.data[BootIsoTp_RxOffset + index] =
                frame.data[index + 1u];//判断是否接收完成
        }
        BootIsoTp_RxOffset = (uint16_t)(BootIsoTp_RxOffset + count);
        BootIsoTp_NextSn = (BootIsoTp_NextSn + 1u) & 0x0Fu;
        BootIsoTp_Timeout = 0u;
        if (BootIsoTp_RxOffset == BootIsoTp_RxPdu.length)
        {
            request->length = BootIsoTp_RxPdu.length;
            for (index = 0u; index < request->length; index++)
            {
                request->data[index] = BootIsoTp_RxPdu.data[index];
            }
            BootIsoTp_Abort();
            return true;
        }
    }
    return false;
}

/* Status DID adds multi-frame responses. Our gateway sends CTS, STmin=0;
 * nonzero STmin / WAIT are explicitly rejected rather than violating timing. */
static bool BootIsoTp_WaitCts(uint8_t *block_size)
{
    BootCan_FrameType frame;
    uint32_t timeout = BOOT_ISOTP_TIMEOUT_LOOPS;
    while (timeout-- != 0u) {
        if (!BootCan_Receive(&frame)) { continue; }
        if (frame.id != BOOT_CAN_REQUEST_ID) { continue; }
        if (frame.dlc < 3u || frame.data[0] != ISOTP_FC || frame.data[2] != 0u) { return false; }
        *block_size = frame.data[1];
        return true;
    }
    return false;
}
bool BootIsoTp_Transmit(const BootIsoTp_PduType *response)
{
    BootCan_FrameType frame;
    uint8_t index;
    uint8_t block_size, in_block = 0u, sequence = 1u, count;
    uint16_t offset;

    if ((response == (const BootIsoTp_PduType *)0) ||
        (response->length == 0u) || (response->length > BOOT_ISOTP_MAX_PDU_LENGTH))
    {
        return false;
    }
    BootIsoTp_ClearFrame(&frame, BOOT_CAN_RESPONSE_ID);
    if (response->length > 7u) {
        frame.data[0] = ISOTP_FF | (uint8_t)(response->length >> 8);
        frame.data[1] = (uint8_t)response->length;
        for (index = 0u; index < 6u; index++) { frame.data[index+2u] = response->data[index]; }
        if (!BootCan_Transmit(&frame) || !BootIsoTp_WaitCts(&block_size)) { return false; }
        offset = 6u;
        while (offset < response->length) {
            BootIsoTp_ClearFrame(&frame, BOOT_CAN_RESPONSE_ID);
            frame.data[0] = ISOTP_CF | sequence;
            count = (uint16_t)(response->length - offset) > 7u ? 7u : (uint8_t)(response->length-offset);
            for (index = 0u; index < count; index++) { frame.data[index+1u] = response->data[offset+index]; }
            if (!BootCan_Transmit(&frame)) { return false; }
            offset += count; sequence = (sequence+1u)&0x0Fu; in_block++;
            if (offset < response->length && block_size != 0u && in_block == block_size) {
                if (!BootIsoTp_WaitCts(&block_size)) { return false; }
                in_block = 0u;
            }
        }
        return true;
    }
    frame.data[0] = (uint8_t)response->length;
    for (index = 0u; index < response->length; index++)
    {
        frame.data[index + 1u] = response->data[index];
    }
    return BootCan_Transmit(&frame);
}
