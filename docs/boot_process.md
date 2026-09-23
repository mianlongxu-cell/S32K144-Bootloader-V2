# Boot process

## State flow

```text
Reset
  |
Boot_Init
  |
software boot request? ---- yes ---> programming server
  |
 no
  |
physical boot request? ---- yes ---> programming server
  |
 no
  |
validate vectors + metadata + CRC
  |                         |
invalid                    valid
  |                         |
programming server       jump Application
```

## Boot request

The physical request uses the configured EVB input pin. The software request is
a magic value and inverse stored in a reserved SRAM area. The Bootloader accepts
the SRAM request only when the reset-cause register reports software reset, then
clears it before entering programming mode. The inverse word reduces the chance
of treating random retained RAM as a valid request.

## Application validity

`Boot_IsApplicationValid()` checks:

1. vector words are neither zero nor erased;
2. initial MSP is aligned and inside configured SRAM;
3. Reset_Handler has its Thumb bit set and points into Application Flash;
4. metadata magic and valid flag match;
5. firmware size is nonzero and bounded;
6. calculated CRC32 equals metadata CRC32.

Any failed condition keeps control in the programming server.

## Application jump

The Bootloader prepares a clean handover by stopping SysTick, disabling and
clearing NVIC banks, clearing pending system exceptions, and relocating
`SCB->VTOR` to `0x00008000`. It loads MSP from vector word 0 and branches to the
Thumb Reset_Handler from vector word 1. The Application startup performs normal
RAM/data initialization and eventually starts FreeRTOS.

The code also normalizes a cold/external boot into the software-reset handover
path before jumping. This keeps the handover consistent with the path exercised
after a successful UDS update.

## Debugging the handover

Useful observations when attaching a debugger after startup:

- a PC inside `0x00000000..0x00007FFF` is Bootloader code;
- a PC at the Application Reset_Handler indicates startup execution;
- `SCB->VTOR` should point to `0x00008000` after Application startup;
- `CONTROL=2` with a changing PSP indicates FreeRTOS Thread mode;
- a changing `xTickCount` confirms SysTick and scheduler operation.
