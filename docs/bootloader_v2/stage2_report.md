# Bootloader V2 Stage 2 Delivery Report

## Stage 1 baseline

Stage 1 memory addresses were retained without modification. The existing
PFlash Bootloader, Slot A compatibility vector, Slot A/B linker scripts,
FlexNVM PBL link draft, boot request contract, CAN/ISO-TP/UDS/CRC stack, and
Application source tree were reused.

## Stage 2 implementation

- full `BootImageValidationResultType` failure classification;
- runtime `BootImageInfoType` for each slot;
- overflow-safe slot address/range APIs;
- complete header, vector, MSP, reset entry, header CRC, and payload CRC checks;
- deterministic policy for zero, one, or two valid images;
- `0xMMmmppbb` version encoding and centralized comparison;
- `BootJump_ToSlot()` with a second independent safety gate;
- debugger-visible Slot A/Slot B results and selected target;
- dependency-free Node.js image packer, corruption tool, and host tests;
- restricted legacy Slot A fallback only when the V2 header is fully erased.

## Added files

- `Bootloader/inc/boot_version.h`
- `Bootloader/src/boot_version.c`
- `Bootloader/inc/boot_policy.h`
- `Bootloader/src/boot_policy.c`
- `tools/image_format.js`
- `tools/pack_image.js`
- `tools/corrupt_image.js`
- `tools/tests/stage2_host_tests.js`
- `docs/bootloader_v2/stage2_test_matrix.md`
- `docs/bootloader_v2/stage2_report.md`

## Modified files

- `Bootloader/inc/boot_types.h`
- `Bootloader/inc/boot_slot.h`
- `Bootloader/src/boot_slot.c`
- `Bootloader/inc/boot_image.h`
- `Bootloader/src/boot_image.c`
- `Bootloader/inc/boot_policy.h`
- `Bootloader/inc/boot_manager.h`
- `Bootloader/src/boot_manager.c`
- `Bootloader/inc/boot_jump.h`
- `Bootloader/src/boot_jump.c`
- `Makefile.application_v2`
- `docs/bootloader_v2/memory_map.md`
- `docs/bootloader_v2/boot_flow.md`

## Validation order

```text
valid Slot ID
 -> safe Header address
 -> empty check
 -> magic
 -> Header version
 -> image size/bounds
 -> Header CRC32
 -> exact vector address/alignment/range
 -> payload range
 -> MSP range/alignment
 -> Reset Handler Thumb/range
 -> Header entry equals vector[1]
 -> payload CRC32
```

All additions and ranges are bounded before dereferencing target addresses.

Validation results are:

```text
BOOT_IMAGE_VALID
BOOT_IMAGE_VALID_LEGACY
BOOT_IMAGE_ERR_INVALID_ARGUMENT
BOOT_IMAGE_ERR_SLOT_ID
BOOT_IMAGE_ERR_HEADER_ADDRESS
BOOT_IMAGE_ERR_EMPTY
BOOT_IMAGE_ERR_MAGIC
BOOT_IMAGE_ERR_HEADER_VERSION
BOOT_IMAGE_ERR_HEADER_CRC
BOOT_IMAGE_ERR_SIZE
BOOT_IMAGE_ERR_ADDRESS_RANGE
BOOT_IMAGE_ERR_VECTOR
BOOT_IMAGE_ERR_STACK_POINTER
BOOT_IMAGE_ERR_RESET_HANDLER
BOOT_IMAGE_ERR_ENTRY_MISMATCH
BOOT_IMAGE_ERR_IMAGE_CRC
```

`BootImageInfoType` records slot ID/base/size, header/payload/vector addresses,
the complete header, actual MSP and Reset Handler, validation result, validity,
and whether the restricted legacy format was used.

## CRC contract

MCU and host use reflected CRC-32/ISO-HDLC: polynomial `0xEDB88320`, initial
value `0xFFFFFFFF`, final XOR `0xFFFFFFFF`, byte reflection implicit in the
least-significant-bit-first loop. The known vector `123456789` produces
`0xCBF43926`.

- payload CRC covers exactly `image_size` bytes from the slot vector address;
- Header CRC covers bytes `0x00..0x3B`, excluding `header_crc32` itself.

## Policy

```text
A valid, B invalid -> A
A invalid, B valid -> B
A invalid, B invalid -> Programming
A and B valid -> higher software_version
equal software_version -> A
```

No PENDING/TRIAL/CONFIRMED/attempt/rollback state is consulted.

## Build and generated images

| Target | Result | Vector | Reset Handler | Text/Data/BSS |
|---|---|---:|---:|---|
| Bootloader | PASS | `0x00000000` | `0x00000410` | `9852/0/1536` |
| APP_A | PASS | `0x00008000` | `0x00008580` | `29444/2408/23832` |
| APP_B | PASS | `0x00040000` | `0x00040580` | `29444/2408/23832` |

Generated examples use version `2.0.0` (`0x02000000`):

- `Application_Build/APP_A/app_slot_a_image.bin`
- `Application_Build/APP_B/app_slot_b_image.bin`

Each pack operation writes a JSON manifest and performs host self-validation.
The current APP_A payload is 31852 bytes with CRC `0xF9AF6ECB`; APP_B is 31852
bytes with CRC `0x39F655EF`. Both full slot images are 229376 bytes.

Key Bootloader symbols after the final link:

```text
BootImage_LoadInfo             0x00001350
BootJump_ToSlot                0x000017B4
BootManager_SelectBootTarget   0x00001A9C
BootPolicy_Select              0x00001BE0
BootManager_SelectedTarget     0x1FFF8000
BootManager_SlotBInfo          0x1FFF819C
BootManager_SlotAInfo          0x1FFF8200
```

## Test status

- 25/25 automated host tests pass.
- Bootloader and both Applications compile with warnings treated as errors.
- Flash/RAM/map boundary checks pass.
- Five malformed image artifacts report the expected distinct failure class.
- Physical Slot A/Slot B boot and CAN/UDS regression remain board acceptance;
  no hardware programmer is attached to this build environment.

## Explicitly not implemented

RAM SBL/flash driver, online inactive-slot download, journal, persistent slot
metadata, power-loss recovery, trial boot, confirmation, rollback, attempt
recovery, secure boot, AES/CMAC, CAN-FD, SOME/IP, and OTA.

## Stage 3 recommendation

Introduce a RAM-resident flash-driver/SBL boundary, validate code relocation and
flash-command execution from SRAM, then make the existing UDS programming path
target an explicitly selected inactive slot. Keep journal/trial/rollback for a
later stage after the write path is proven.
