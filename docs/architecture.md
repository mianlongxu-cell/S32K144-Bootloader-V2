# Architecture

## System boundary

The prototype has three independently testable parts:

1. the S32K144 Application;
2. the resident S32K144 Bootloader;
3. Linux SocketCAN tools connected through a Classical CAN adapter.

```text
Linux command / monitor / flasher
        |
SocketCAN frame API
        |
CAN 2.0A, 500 kbit/s
        |
S32K144 FlexCAN0
   +----+-------------------+
   |                        |
Bootloader              Application
```

## Application modules

```text
FreeRTOS tasks
  |
  +-- Vehicle / fault runnables -> RTE -> VehicleModel / DEM
  +-- CAN-Tx -> COM -> PduR -> CanIf -> FlexCAN
  +-- CAN-Rx <- PduR <- CanIf <- FlexCAN ISR
  +-- Diagnostic -> DCM <-> CanTp <-> PduR
```

- `AppRTOS` owns task creation and task periods.
- `VehicleModel` holds simulated powertrain/body state.
- `Com` serializes periodic PDUs and decodes control commands.
- `CanIf` queues frames and isolates FlexCAN access.
- `CanTp` handles ISO-TP SF/FF/CF/FC transport.
- `Dcm` dispatches Application UDS services.
- `Dem`/`Dtc` maintain DTC status and clearing behavior.
- `AppBootRequest` writes a retained SRAM request before software reset.

Periodic data flow:

```text
VehicleModel -> COM -> CanIf -> FlexCAN -> CAN IDs 0x100/0x101/0x102
```

Control data flow:

```text
Linux -> CAN ID 0x200 -> FlexCAN -> CanIf -> PduR/COM -> VehicleApp
```

## Bootloader modules

```text
boot.c state machine
  +-- boot_request   boot pin and retained software request
  +-- boot_check     vectors, metadata, and image CRC
  +-- boot_can       polling FlexCAN driver
  +-- boot_isotp     transport reassembly/segmentation
  +-- boot_uds       programming state machine
  +-- boot_security  seed/key transform
  +-- boot_flash     erase/program/read-back protection
  +-- boot_firmware  metadata lifecycle
  +-- boot_jump      VTOR/MSP/Reset_Handler handover
```

The Bootloader never accepts writes outside the configured Application region.
Metadata is invalidated before erase and marked valid only after transfer length
and CRC verification succeed.

## Linux modules

- `can_bus.py`: native Linux SocketCAN frame access.
- `vehicle.py`: decode vehicle PDUs and encode control commands.
- `csv_logger.py`: append decoded snapshots.
- `isotp.py`: SF/FF/CF/FC transport with sequence and timeout checks.
- `uds.py`: diagnostic and programming client primitives.
- `flasher.py`: ordered update workflow and fault-injection options.
- `main.py`: monitor, control, and diagnostic CLI.
- `uds_flasher`: firmware-update CLI.

Diagnostic data flow:

```text
CLI -> UdsClient -> IsoTpClient -> SocketCAN -> ECU DCM/Bootloader
CLI <- UdsClient <- IsoTpClient <- SocketCAN <- ECU response
```

## Separation decisions

- Bootloader and Application use separate linker regions and vector tables.
- The Bootloader is polling-based and does not depend on FreeRTOS.
- The Linux tools do not require vendor CAN libraries; they use SocketCAN.
- The existing S32DS Application layout is preserved because `.project` and
  `.cproject` reference the root `src/include/Project_Settings` paths.
