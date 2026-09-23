# Bootloader V2 Boot and Programming Flow

## Boot

Reset -> SRAM ECC/data/BSS initialization -> BootManager_Init ->
load RAM driver under watchdog -> load/recover Journal -> hand verified target
to Slot Metadata -> resolve exhausted TRIAL -> scan lifecycle-allowed slots ->
capture active identity -> process programming/confirmation request.

A software request uses magic/inverse plus running slot/inverse in the existing
16-byte retained block. A valid old App request without slot identity falls
back to the validated Stage2 BootPolicy. PTC12 remains the hardware request.

Without a request, lifecycle state wins before version: TRIAL, then PENDING,
then CONFIRMED; INVALID/unknown Metadata is excluded. Equal-state images use the
higher version, with A winning ties. Neither trusted/valid
enters Programming Mode. Both invalid Journal copies never fall back to guessing
the higher version. See [stage4_recovery_flow.md](stage4_recovery_flow.md).
The cold-reset normalization occurs before attempt accounting. Immediately
before a PENDING jump, Metadata is durably changed to TRIAL/attempt=1. A later
TRIAL launch increments exactly once. At attempt=3 a subsequent unconfirmed
reset marks it INVALID and selects the other CONFIRMED slot. BootJump retains
its independent Journal, lifecycle, image, VTOR/MSP/entry safety gates.

## Programming

App 10 02 -> App records request -> software reset -> Programming Mode ->
enable programming watchdog -> 10 02 -> 27 seed/key -> read F104 Journal ->
read active/target/version -> PREPARING -> ERASING -> erase exact inactive slot ->
DOWNLOAD_READY -> 34 / PROGRAMMING -> 36 chunks + 32 KiB checkpoints ->
37 / TRANSFER_COMPLETE (persist cached Header) -> 31 FF01 / VERIFYING ->
Stage2 image validation -> publish Header -> validate stored image ->
VERIFIED -> PENDING_ACTIVATION -> 11 reset -> target Metadata VERIFIED/PENDING ->
Journal IDLE -> Metadata TRIAL/attempt=1 -> new Application health task ->
confirmation request/software reset -> Bootloader commits CONFIRMED -> Application.

Header is held in SRAM until integrity validation; Flash retains an invalid
marker while transfer is incomplete. Previous-block retries are rejected with
73 without advancing BSC. Failed programming requires erase before restart.
An interrupted download exits with physical reset rather than an unconditional
UDS reset; 11 is deliberately allowed only after verified completion.

Erase/verify emit 78 before blocking work and between sectors. Linux waits
for the final response, never retransmits the original request after 78, and
bounds P2* waiting. Board timing acceptance remains necessary.

## Build/test

On this Windows host:

```powershell
powershell -ExecutionPolicy Bypass -File tools/build_stage4.ps1
```

For the current lifecycle build use `tools/build_stage5.ps1`. The script accepts
compiler/runtime path overrides. It rebuilds without deleting
existing Stage3 APP_A/APP_B acceptance fixtures. Stage4 apps/artifacts go under
Application_Build/Stage4. It runs the production C state-machine tests,
Python gateway tests, Stage2 packer tests and ELF/RAM dependency checks.

Normal toolchain PATH workflow:

```sh
make -C Bootloader all
make -f Makefile.application_v2 images IMAGE_VERSION=3.0.0
python3 tools/check_stage3_layout.py
cd linux_gateway
python3 -m unittest discover -s tests -v
```

Stage4 outputs: Application_Build/Stage4/Release/Bootloader.elf, corresponding
FaultInjection build (not the default), APP_A (6.0.0), APP_B (7.0.0).
Bootloader/build/Bootloader.elf is left with injection OFF. Driver artifacts:
FlashDriver/build/flash_driver.elf,
.bin, .map; Application_Build/APP_A/APP_A.elf and app_slot_a_image.bin;
Application_Build/APP_B/APP_B.elf and app_slot_b_image.bin.

Use the new Bootloader and updated Linux gateway together. Debugger installation
may erase other Flash sectors depending on programmer settings: back up working
images and inspect the erase configuration. This work does not flash the board.
Do not use the link-only PBL_FlexNVM artifact for hardware installation.
The plain make examples above retain the original build paths/version options;
prefer the Stage4 script to keep old acceptance fixtures separate.
