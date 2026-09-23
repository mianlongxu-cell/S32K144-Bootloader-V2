#include <stdint.h>
#include "boot_config.h"
#include "boot_check.h"
#include "boot_crc32.h"
#include "boot_firmware.h"

bool Boot_IsApplicationValid(void)
{
    Boot_FirmwareMetadataType metadata;
    const volatile uint32_t *const vectors =
        (const volatile uint32_t *)APP_START_ADDRESS;
    const uint32_t initial_msp = vectors[0];//Application 初始 MSP，SRAM现场
    const uint32_t reset_handler = vectors[1];//Application Reset_Handle
    const uint32_t reset_address = reset_handler & ~1u;//清低位后是复位的falsh地址

    /* Cortex-M4 consumes these vector words before executing Reset_Handler.
     * Validate both before trusting any code from the download region. */
    if ((initial_msp == 0u) || (initial_msp == 0xFFFFFFFFu) ||
        (reset_handler == 0u) || (reset_handler == 0xFFFFFFFFu))//排除空 Flash 或未烧录状态
    {
        return false;
    }

    if ((initial_msp < SRAM_START_ADDRESS) ||
        (initial_msp > SRAM_END_ADDRESS) ||
        ((initial_msp & 0x7u) != 0u))//接着检查 MSP 是否落在合法 SRAM，要求 8 字节对齐
    {
        return false;
    }

    if (((reset_handler & 1u) == 0u) ||//检查复位函数地址
        (reset_address < APP_START_ADDRESS) ||
        (reset_address >= APP_END_ADDRESS))
    {
        return false;
    }
    /* The valid marker is committed only after a complete transfer and CRC
     * check, so an interrupted update must fail this gate. */
    if (!BootFirmware_ReadMetadata(&metadata) ||
        (metadata.magic != BOOT_METADATA_MAGIC) ||
        (metadata.validFlag != BOOT_METADATA_VALID) ||
        (metadata.firmwareSize == 0u) ||
        (metadata.firmwareSize > (APP_END_ADDRESS - APP_START_ADDRESS)))//只有完整下载并校验成功后，才会写入有效标志。
    {
        return false;
    }
    return BootCrc32_Calculate((const uint8_t *)APP_START_ADDRESS,
                               metadata.firmwareSize) == metadata.firmwareCrc;//CRC 校验
}
