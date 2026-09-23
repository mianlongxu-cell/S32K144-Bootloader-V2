# Bootloader V2 Stage 2 Test Matrix

## Automated host results

Run with:

```sh
node tools/tests/stage2_host_tests.js
```

All 25 tests pass. They cover the standard CRC32 vector, version ordering,
header format, both slot layouts, address boundary handling, every policy
branch, and malformed image rejection.

| Scenario | Expected | Host result | Target result |
|---|---|---|---|
| A valid 1.0.0 / B empty | Slot A | PASS | pending board test |
| A empty / B valid 1.0.0 | Slot B | PASS | pending board test |
| A valid 1.0.0 / B valid 1.1.0 | Slot B | PASS | pending board test |
| A valid 1.1.0 / B valid 1.0.0 | Slot A | PASS | pending board test |
| A valid 1.0.0 / B valid 1.0.0 | Slot A | PASS | pending board test |
| A payload CRC error / B valid | Slot B | PASS | pending board test |
| A valid / B payload CRC error | Slot A | PASS | pending board test |
| A bad magic / B valid | Slot B | PASS | pending board test |
| A valid / B bad vector | Slot A | PASS | pending board test |
| A invalid / B invalid | Programming Mode | PASS | pending board test |

## Malformed image generation

```sh
node tools/corrupt_image.js --slot A \
  --input Application_Build/APP_A/app_slot_a_image.bin \
  --output Application_Build/test_images/app_a_bad_crc.bin \
  --mode image-crc
```

Supported modes:

- `magic` -> `BOOT_IMAGE_ERR_MAGIC`
- `header-crc` -> `BOOT_IMAGE_ERR_HEADER_CRC`
- `image-crc` -> `BOOT_IMAGE_ERR_IMAGE_CRC`
- `vector` -> `BOOT_IMAGE_ERR_VECTOR`
- `reset-handler` -> `BOOT_IMAGE_ERR_RESET_HANDLER`

## Debugger observability

No UART was introduced. Add these symbols to the S32DS Expressions view:

```text
BootManager_SlotAInfo.validation_result
BootManager_SlotAInfo.header.software_version
BootManager_SlotBInfo.validation_result
BootManager_SlotBInfo.header.software_version
BootManager_SelectedTarget
Boot_CurrentState
```

Break at `BootManager_SelectBootTarget`, `BootPolicy_Select`, or
`BootJump_ToSlot` to inspect the complete decision without Release logging
overhead.

## Required board acceptance sequence

The target column cannot be claimed from static builds. On hardware, program
the Bootloader plus each complete slot image at its documented base, reset,
and check `BootManager_SelectedTarget` and CAN/UDS behavior for every row.
Never load a slot BIN at address zero.
