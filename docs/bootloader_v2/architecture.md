# Bootloader V2 Architecture

Stage 1 defines the fixed Flash memory map; Stage 2 validates and selects A/B
images. Stage 3 adds inactive-slot UDS programming through a standalone SRAM
Flash Driver. The original project outside this V2 directory is not changed.
Stage 4 adds a transactional dual-copy Journal, persistent update manager and
reset recovery; its 10/44/90% and torn-Journal board tests are accepted. Stage 5
adds a separate dual-copy Slot Metadata lifecycle: PENDING, TRIAL, CONFIRMED,
attempt counting and rollback.

## Modules

- BootManager: captures the active slot/version before clearing a programming
  request; owns Journal recovery -> allowed-slot scan -> BootPolicy -> BootJump.
  The active programming snapshot changes only on reset/explicit commissioning.
- BootJournal / JournalStorage: 128-byte record validation, wrap-safe sequence,
  alternate-sector commit/readback. No programming-state decisions.
- BootUpdate: legal state transitions, checkpoints, persistent abort, bootability.
- BootRecoveryPolicy: pure state-to-recovery-action and slot-permission rules.
- BootImageInstall: reuses BootImage validation and publishes the cached Header
  during reset recovery; does not implement a second image format/CRC policy.
- BootSlot: translates explicit slot IDs to base/end/header/vector addresses.
- BootImage: the single MCU implementation of header/payload/CRC/vector/MSP/
  reset-entry validation. Programming reuses it with a staged header and again
  with the final Flash header.
- BootMetadata / MetadataStorage: per-slot 80-byte record, CRC, wrap-safe
  sequence, alternate-sector commit/readback and legal transition enforcement.
- BootLifecycle: Journal-to-Metadata handoff, attempt/confirm/rollback ownership,
  reset diagnostics and fail-safe handling.
- BootPolicy: state priority TRIAL > PENDING > CONFIRMED > excluded, then
  version comparison; equal versions choose A. INVALID never wins by version.
- BootProgramming: session/security, active/target identity, exact download
  bounds, BSC, receive count, phrase buffer and header-last publication.
- BootFlash: loader/API validator and second write-range guard, not FTFC logic.
- FlashDriver: separately linked SRAM ELF/BIN/MAP, fixed checked API table,
  FTFC sector erase / phrase program / read-back, no PFlash dependencies.
- BootUds: protocol decoding, NRC translation and ResponsePending.
- BootCan/BootIsoTp: existing polling transport retained. A full 256-byte PDU
  copy now uses a non-wrapping 16-bit index.
- boot_app_if: same Application source with APP_BUILD_SLOT selected by the A/B
  build target; returns packed header version and records slot/complement in
  the final eight bytes of the existing retained request reservation.

## Image format remains unchanged

64 bytes, little-endian fields: magic, header_version, image_size, image_crc32,
software_version, build_id, vector_address, entry_address, flags, reserved[6],
header_crc32. Header CRC covers the first 60 bytes. Payload CRC covers exactly
image_size bytes from the slot vector. Version is 0xMMmmppbb.

The header remains in the final sector of each 224 KiB slot; it is not moved
in front of the vector. A full packed file is 0x38000 bytes. Header creation
belongs to the host packer, not the MCU.

## Safety and compatibility

Application programming never writes Active Slot, Bootloader, shared reserve,
FlexNVM or RAM. Separate Journal APIs access only 0x78000/0x79000; Metadata APIs
are bounded to 0x7A000..0x7DFFF. Both invalid
records enter Programming Mode, including first installation. Explicit FF02
commissioning trusts a manually selected, validated old slot; `none` is allowed
only when both images are invalid. A retained known active identity remains
write-protected even if its image later fails CRC. Dirty targets cannot be scanned
or jumped to through BootManager/BootJump until transaction validation succeeds.

CAN IDs, bitrate, ISO-TP, seed/key and service IDs remain compatible. The V2
download payload is now the full packed image, not a raw Application.bin.
FF01 parameters are removed: MCU validates the embedded header, with no legacy
metadata creation. Legacy metadata reads are retained for existing Slot A
images; legacy metadata remains read-only. Image Header format is unchanged.
The Linux CLI defaults to V2; --legacy is only for an actual old V1 Bootloader.

Programming requires an increasing version so the unchanged Stage2 policy
selects the new image. F100 remains the legacy App text version; F101 reports
slot, F102 reports packed version, F103 reports Bootloader target.
F104 reports a 32-byte Journal status and F105 reports the Stage5 lifecycle over
multi-frame ISO-TP. F103 remains the inactive programming target. Stage3 ECUs that
explicitly reject F104 keep the original full-update workflow; an invalid/error
Stage4 Journal is never treated as a Stage3 device.

## Explicit later work

No signature verification, cryptographic secure boot, anti-rollback security,
resume download or FlexNVM partitioning. CRC/version checks are integrity and
selection mechanisms, not authenticity. The FlexNVM PBL ELF remains link-only.

See [memory_map.md](memory_map.md), [boot_flow.md](boot_flow.md), and
[flash_driver.md](flash_driver.md) for memory and wire contracts.
See [stage4_update_journal.md](stage4_update_journal.md) and
[stage4_recovery_flow.md](stage4_recovery_flow.md) for Stage4 implementation/limits.
