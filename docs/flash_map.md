# Flash map

## Address map

The values are defined by `Bootloader/inc/boot_cfg.h` and the active linker
scripts.

| Region | Start | End exclusive | Size |
|---|---:|---:|---:|
| Bootloader | `0x00000000` | `0x00008000` | 32 KiB |
| Application | `0x00008000` | `0x0007F000` | 476 KiB |
| Firmware metadata sector | `0x0007F000` | `0x00080000` | 4 KiB |

```text
0x00000000  Bootloader vector table and code
0x00008000  Application vector table
0x00008400  Application text region
0x0007F000  Firmware metadata sector
0x00080000  End of S32K144 program Flash
```

The Application cannot start at address zero because the Cortex-M4 fetches the
initial MSP and reset vector from address zero after reset. Those entries belong
to the resident Bootloader. The Bootloader later validates and transfers control
to the Application vector table at `0x00008000`.

## Application vector handover

The first two words at `APP_START_ADDRESS` are:

1. initial Main Stack Pointer;
2. Thumb Reset_Handler address.

Before jumping, `Boot_IsApplicationValid()` rejects erased, unaligned, out-of-
SRAM stack pointers and reset handlers outside the Application region. The jump
code then:

1. disables and clears interrupts and SysTick;
2. writes `SCB->VTOR = 0x00008000`;
3. restores Thread mode control state;
4. loads the Application MSP;
5. branches to the Application Reset_Handler.

The Application startup subsequently initializes ECC RAM, `.data`, `.bss`, and
its runtime before calling `main()`.

## Firmware metadata

The metadata record occupies 24 bytes at the start of the final 4 KiB sector:

| Field | Size | Purpose |
|---|---:|---|
| magic | 4 | identifies firmware metadata (`0x53394D44`) |
| firmwareSize | 4 | number of valid Application bytes |
| firmwareCrc | 4 | reflected standard CRC-32 |
| firmwareVersion | 4 | numeric release value |
| validFlag | 4 | valid marker (`0x56414C49`) |
| reserved | 4 | reserved, currently erased value |

The CRC uses polynomial `0xEDB88320`, initial value `0xFFFFFFFF`, and final XOR
`0xFFFFFFFF`. The metadata sector is erased before an update and programmed only
after complete transfer and CRC verification.

## Flash programming constraints

- program Flash sector: 4096 bytes;
- program phrase: 8 bytes;
- final partial phrase is padded with `0xFF`;
- erase addresses must be sector aligned;
- all write ranges must remain inside `0x00008000..0x0007EFFF`;
- programmed phrases are read back and compared.
