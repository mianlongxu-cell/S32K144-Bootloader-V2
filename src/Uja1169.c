#include "S32K144.h"
#include "Uja1169.h"

/* S32K144EVB-Q100: UJA1169 is connected to LPSPI1 PCS3 on PTB14..17. */
#define UJA_SPI_TIMEOUT_LOOPS       100000u
#define UJA_STARTUP_WAIT_LOOPS      200000u
#define UJA_RETRY_WAIT_LOOPS         20000u
#define UJA_CAN_START_WAIT_LOOPS     50000u
#define UJA_ID_READ_ATTEMPTS             5u
#define UJA_REG_MODE_CONTROL        0x01u
#define UJA_REG_MAIN_STATUS         0x03u
#define UJA_REG_WATCHDOG_STATUS     0x05u
#define UJA_REG_REGULATOR_CONTROL   0x10u
#define UJA_REG_SUPPLY_STATUS       0x1Bu
#define UJA_REG_CAN_CONTROL         0x20u
#define UJA_REG_TRANSCEIVER_STATUS  0x22u
#define UJA_REG_SBC_CONFIG          0x74u
#define UJA_REG_IDENTIFICATION      0x7Eu

#define UJA_MODE_NORMAL             0x07u
#define UJA_V2_ON_IN_NORMAL         0x04u
#define UJA_V2_CONTROL_MASK         0x0Cu
#define UJA_CAN_ACTIVE_UV_MONITORED 0x01u
#define UJA_WD_FORCED_NORMAL_MASK   0x08u
#define UJA_CAN_ACTIVE_STATUS_MASK  0x80u

volatile uint8_t Uja1169DeviceId;
volatile uint8_t Uja1169MainStatus;
volatile uint8_t Uja1169WatchdogStatus;
volatile uint8_t Uja1169ConfigStatus;
volatile uint8_t Uja1169SupplyStatus;
volatile uint8_t Uja1169CanStatus;
volatile uint8_t Uja1169ReadAttempts;
volatile Uja1169InitResultType Uja1169InitResult = UJA1169_INIT_NOT_RUN;

static void Uja1169_Delay(uint32_t loops)
{
    volatile uint32_t count;
    for (count = 0u; count < loops; count++)
    {
        __asm volatile ("nop");
    }
}

static bool Uja1169_IsSupportedDevice(uint8_t id)
{
    return (id == 0xCFu) || (id == 0xC9u) || (id == 0xEFu) ||
           (id == 0xE9u) || (id == 0xCEu) || (id == 0xEEu);
}

static void Uja1169_SpiInit(void)
{
    IP_PCC->PCCn[PCC_PORTB_INDEX] |= PCC_PCCn_CGC_MASK;
    IP_PORTB->PCR[14] = PORT_PCR_MUX(3u); /* LPSPI1_SCK */
    IP_PORTB->PCR[15] = PORT_PCR_MUX(3u); /* LPSPI1_SIN */
    IP_PORTB->PCR[16] = PORT_PCR_MUX(3u); /* LPSPI1_SOUT */
    IP_PORTB->PCR[17] = PORT_PCR_MUX(3u); /* LPSPI1_PCS3 */

    IP_PCC->PCCn[PCC_LPSPI1_INDEX] = 0u;
    /* SOSCDIV2 is 8 MHz in both Bootloader and Application. */
    IP_PCC->PCCn[PCC_LPSPI1_INDEX] = PCC_PCCn_PCS(1u) |
                                           PCC_PCCn_CGC_MASK;

    IP_LPSPI1->CR = 0u;
    IP_LPSPI1->IER = 0u;
    IP_LPSPI1->DER = 0u;
    IP_LPSPI1->CFGR0 = 0u;
    IP_LPSPI1->CFGR1 = LPSPI_CFGR1_MASTER_MASK;
    /* 8 MHz / (SCKDIV + 2) = 1 MHz. */
    IP_LPSPI1->CCR = LPSPI_CCR_SCKPCS(3u) |
                     LPSPI_CCR_PCSSCK(3u) |
                     LPSPI_CCR_DBT(6u) |
                     LPSPI_CCR_SCKDIV(6u);
    IP_LPSPI1->FCR = 0u;
    IP_LPSPI1->CR = LPSPI_CR_RRF_MASK | LPSPI_CR_RTF_MASK;
    IP_LPSPI1->CR = LPSPI_CR_MEN_MASK | LPSPI_CR_DBGEN_MASK;

    /* TCR is a command register and the S32K1xx LPSPI ignores the command
     * while MEN is clear.  Program it only after enabling the module. */
    IP_LPSPI1->TCR = LPSPI_TCR_CPHA_MASK |
                     LPSPI_TCR_PCS(3u) |
                     LPSPI_TCR_FRAMESZ(15u);
}

static bool Uja1169_Transfer(uint16_t tx, uint16_t *rx)
{
    uint32_t timeout = UJA_SPI_TIMEOUT_LOOPS;

    while (((IP_LPSPI1->SR & LPSPI_SR_TDF_MASK) == 0u) && (timeout > 0u))
    {
        timeout--;
    }
    if (timeout == 0u)
    {
        return false;
    }
    IP_LPSPI1->TDR = tx;

    timeout = UJA_SPI_TIMEOUT_LOOPS;
    while (((IP_LPSPI1->SR & LPSPI_SR_RDF_MASK) == 0u) && (timeout > 0u))
    {
        timeout--;
    }
    if (timeout == 0u)
    {
        return false;
    }
    *rx = (uint16_t)IP_LPSPI1->RDR;
    return true;
}

static bool Uja1169_Read(uint8_t address, uint8_t *value)
{
    uint16_t rx;
    const uint16_t command =
        (uint16_t)((uint16_t)(((uint16_t)address << 1u) | 1u) << 8u);
    if (!Uja1169_Transfer(command, &rx))
    {
        return false;
    }
    *value = (uint8_t)rx;
    return true;
}

static bool Uja1169_Write(uint8_t address, uint8_t value)
{
    uint16_t rx;
    const uint16_t command =
        (uint16_t)(((uint16_t)address << 9u) | (uint16_t)value);
    return Uja1169_Transfer(command, &rx);
}

static bool Uja1169_ReadDeviceId(void)
{
    uint8_t attempt;
    uint8_t id = 0u;

    for (attempt = 1u; attempt <= UJA_ID_READ_ATTEMPTS; attempt++)
    {
        Uja1169ReadAttempts = attempt;
        if (!Uja1169_Read(UJA_REG_IDENTIFICATION, &id))
        {
            Uja1169InitResult = UJA1169_INIT_SPI_TIMEOUT;
            return false;
        }
        Uja1169DeviceId = id;
        if (Uja1169_IsSupportedDevice(id))
        {
            return true;
        }
        Uja1169_Delay(UJA_RETRY_WAIT_LOOPS);
    }
    Uja1169InitResult = UJA1169_INIT_BAD_DEVICE;
    return false;
}

bool Uja1169_InitCanNormal(void)
{
    uint8_t regulator_control;

    Uja1169InitResult = UJA1169_INIT_NOT_RUN;
    Uja1169ReadAttempts = 0u;
    Uja1169_SpiInit();

    /* UJA1169 ignores SPI for up to tto(SPI) after RSTN rises (20 us max).
     * This conservative delay is clock-independent enough for both the early
     * Bootloader clock and the 80 MHz Application clock. */
    Uja1169_Delay(UJA_STARTUP_WAIT_LOOPS);

    if (!Uja1169_ReadDeviceId())
    {
        return false;
    }
    if (!Uja1169_Read(UJA_REG_MAIN_STATUS, (uint8_t *)&Uja1169MainStatus) ||
        !Uja1169_Read(UJA_REG_WATCHDOG_STATUS,
                      (uint8_t *)&Uja1169WatchdogStatus) ||
        !Uja1169_Read(UJA_REG_SBC_CONFIG, (uint8_t *)&Uja1169ConfigStatus))
    {
        Uja1169InitResult = UJA1169_INIT_SPI_TIMEOUT;
        return false;
    }
    if ((Uja1169WatchdogStatus & UJA_WD_FORCED_NORMAL_MASK) != 0u)
    {
        (void)Uja1169_Read(UJA_REG_SUPPLY_STATUS,
                           (uint8_t *)&Uja1169SupplyStatus);
        (void)Uja1169_Read(UJA_REG_TRANSCEIVER_STATUS,
                           (uint8_t *)&Uja1169CanStatus);
        Uja1169InitResult = UJA1169_INIT_OK_FORCED_NORMAL;
        return true;
    }

    if (!Uja1169_Read(UJA_REG_REGULATOR_CONTROL, &regulator_control) ||
        !Uja1169_Write(UJA_REG_REGULATOR_CONTROL,
                       (uint8_t)((regulator_control & ~UJA_V2_CONTROL_MASK) |
                                 UJA_V2_ON_IN_NORMAL)) ||
        !Uja1169_Write(UJA_REG_CAN_CONTROL,
                       UJA_CAN_ACTIVE_UV_MONITORED) ||
        !Uja1169_Write(UJA_REG_MODE_CONTROL, UJA_MODE_NORMAL))
    {
        Uja1169InitResult = UJA1169_INIT_SPI_TIMEOUT;
        return false;
    }

    /* CTS may take up to 220 us after CAN Active is selected. */
    Uja1169_Delay(UJA_CAN_START_WAIT_LOOPS);
    if (!Uja1169_Read(UJA_REG_MAIN_STATUS,
                      (uint8_t *)&Uja1169MainStatus) ||
        !Uja1169_Read(UJA_REG_SUPPLY_STATUS,
                      (uint8_t *)&Uja1169SupplyStatus) ||
        !Uja1169_Read(UJA_REG_TRANSCEIVER_STATUS,
                      (uint8_t *)&Uja1169CanStatus))
    {
        Uja1169InitResult = UJA1169_INIT_SPI_TIMEOUT;
        return false;
    }

    if ((Uja1169CanStatus & UJA_CAN_ACTIVE_STATUS_MASK) == 0u)
    {
        Uja1169InitResult = UJA1169_INIT_NOT_ACTIVE;
        return false;
    }
    Uja1169InitResult = UJA1169_INIT_OK_NORMAL;
    return true;
}
