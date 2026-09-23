# RAM Flash Driver and programming contract

This is an implementation reference, not a hardware acceptance report.

## Memory and ownership

- API table: `0x20005800`; executable/data/BSS reservation ends at `0x20006000`.
- The 2 KiB reservation was chosen from the actual Application map: C heap ends
  at `0x20005438`, App stack starts at `0x20006BF0`.
- Bootloader data/BSS ends at `0x1FFF8284`; programming context is
  `0x1FFF8200` (112 bytes), phrase buffer `0x1FFF8270` (8 bytes).
- Boot stack is `0x20006000..0x20006FF0` exclusive end (4080 bytes).
- FreeRTOS's 16 KiB heap is inside Application BSS; Application is stopped/reset
  before programming. App startup may initialize driver RAM again; no calls to
  this driver are made from a running App.
- Boot/driver/App linker assertions protect driver vs heap/stack boundaries.
  Flash A/B/FlexNVM partitions and the 16-byte request reservation are unchanged.

## Binary and ABI

`make -C FlashDriver` emits ELF/BIN/MAP. The SRAM VMA and LMA are identical.
Bootloader embeds the BIN in PFlash, copies/zero-initializes it into the reserved
RAM, compares copied bytes, checks magic `0x46445233`, ABI version 2, and all seven
Thumb function pointers against the loaded code region before calling it.

Table order: magic, api_version, init(active,target), erase(address,size),
program(address,data,size), verify(address,data,size), get_version(),
journal_erase(copy), journal_program(copy,offset,data,size).
The 32-bit ABI occupies 36 bytes. It is not an uploaded/authenticated SBL.
Stage4 driver image is 752 bytes (760 bytes including BSS), within the original
2 KiB reservation. Historic Bootloader BSS addresses above are Stage3 snapshots.

The standalone ELF has no undefined symbols, no libc, no PFlash call targets,
and all constants/branches are SRAM resident. The caller buffers are SRAM.
`check_stage3_layout.py` verifies ELF sections, API pointers, dependencies,
driver bounds and Boot/App heap/stack bounds.

## Flash operations

Only the configured inactive slot is allowed by both the BootFlash facade and
the RAM driver. Bootloader, current active image, shared metadata and all
FlexNVM/RAM download targets are rejected. Legacy metadata writes are disabled.
Stage4 has separate Journal-only APIs for two sectors at 0x78000 and 0x79000;
program is additionally bounded to the first 128 bytes. They temporarily use a
private driver range and restore the inactive-slot range before returning.
They are synchronous, Bootloader-only, non-reentrant operations; no ISR calls
this driver. They do not expand the UDS download address window.

Erase operates one 4 KiB sector per call and blank-checks it. Program uses
8-byte phrases, checks alignment and source SRAM bounds, and reads data back.
All-FF phrases are verified without issuing an unnecessary program command.
FTFC busy polling, watchdog refresh, literal pools and callees remain in SRAM.
Interrupts are disabled during a command and restored afterward. A stuck busy
command never returns to PFlash; it stops refreshing the watchdog and stays in
SRAM. Cache/speculation are disabled in the RAM init routine for coherent reads.

These programming units follow NXP's [programming application note](https://www.nxp.com/docs/en/application-note/AN12130.pdf)
and [firmware update note](https://www.nxp.com/docs/en/application-note/AN12323.pdf).
Cache precautions follow [NXP support's coherency guidance](https://community.nxp.com/t5/S32K/S32k144/td-p/1772838).

## Download state and wire contract

BootManager captures active slot from a validated software-request slot ID plus
complement, or Stage2 Boot Policy for old Apps/hardware requests. It is frozen
for the complete programming session; the request cookie is not a journal.
A -> B; B -> A; no valid image -> recovery target A. Unknown IDs are rejected.

- 10 02: enter programming; cancel previous download, lock security.
- 27 01 / 27 02: existing demonstration seed/key scheme (not secure boot).
- 22 F1 01: active/running slot (00=A, 01=B, FF=none).
- 22 F1 02: active/packed version, four big-endian bytes.
- 22 F1 03: inactive target (Bootloader only).
- 31 01 FF 00 + address:u32be + size:u32be: exact full target slot erase.
- 34 00 44 + address:u32be + size:u32be: exact same full slot download.
- 36 + BSC + bytes: BSC starts at 1, wraps FF->00. Previous-block duplicate
  requests are rejected with 73 without advancing state. No automatic retry.
- 37: requires exact received/programmed length and empty phrase buffer.
- 31 01 FF 01: no legacy size/CRC/version parameters; verify the packed image.
- 11 01: accepted only after successful erase, complete transfer and validation.

NRCs: 13 malformed length, 22 wrong session, 24 wrong sequence/state,
31 range/target/non-increasing version, 33 security denied, 35 invalid key,
72 flash/image failure, 73 BSC mismatch, 78 response pending.

Full image size is 0x38000 bytes, phrase aligned. Individual TransferData
chunks may have any length from 1 to maxNumberOfBlockLength-2; an 8-byte buffer
assembles phrases across requests. Header.image_size remains the true payload
length. No synthetic final padding is required for full-slot images.

Header bytes are staged in RAM. The target header is first erased and marked
invalid, preventing normal aborted downloads from using a stale/legacy header.
After TransferExit, BootImage_Validate checks the staged header and written
payload using the existing Stage2 code. A higher version is required when an
active image exists. Only then is the header sector erased and the original
64 header bytes programmed. BootImage_LoadInfo checks the stored image again.

This is header-last publication, not a metadata transaction or power-loss
recovery implementation. Active slot is never modified, but reset/power failure
during the final header erase/program window is NOT claimed transaction-safe.

## Timing and supervision

Bootloader is polling-only; FreeRTOS is not running during programming.
ResponsePending is queued before each erase sector and each full validation.
The Linux client receives subsequent responses without resending the request,
uses advertised P2*, and enforces a 30-second / 120-pending upper bound.
Existing P2=50 ms and P2*=5000 ms remain. Actual P2 timing and CAN load must be
measured on the board; host tests cannot prove electrical/timing behavior.

Programming mode enables a non-windowed LPO watchdog with UPDATE/CMD32EN.
The server and RAM command loop refresh it. Normal startup compatibility
initially disables it but leaves UPDATE enabled. This is not permanent
watchdog disablement for the programming phase.

## Linux use

Use the updated `uds_flasher` AND `gateway/` directory together:
`python3 uds_flasher app_slot_b_image.bin` when current slot is A.
V2 is default. It reads/validates the packed header, queries active/target/version,
uses ECU-advertised block length, and verifies running slot AND packed version
after reset. `--legacy` is explicitly only for old V1 Bootloaders.
F100 remains the legacy display string; F102 is the packed boot-policy version.

Negative test options: `--skip-security`, `--wrong-sequence`, `--wrong-crc`,
`--stop-at 30`. After intentional interruption, use physical board reset;
UDS reset is deliberately gated until a successful verified update.

No Journal, persistent slot state, trial/confirm/rollback, secure boot or
FlexNVM partitioning is implemented.
