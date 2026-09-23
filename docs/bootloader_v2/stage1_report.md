# Bootloader V2 Stage 1 Delivery Report

## Result

Stage 1 is complete as a non-destructive architecture/build refactor. The
default runtime still uses the proven single Application, legacy metadata,
CAN/ISO-TP/UDS/CRC programming path, while V2 modules and A/B/PBL build
contracts are available for inspection and later phases.

## Added files

- `include/boot_memory_contract.h`
- `include/boot_app_if.h`
- `src/boot_app_if.c`
- `Bootloader/inc/boot_config.h`
- `Bootloader/inc/boot_types.h`
- `Bootloader/inc/boot_manager.h`
- `Bootloader/inc/boot_image.h`
- `Bootloader/inc/boot_slot.h`
- `Bootloader/src/boot_manager.c`
- `Bootloader/src/boot_image.c`
- `Bootloader/src/boot_slot.c`
- `Project_Settings/Linker_Files/S32K144_app_slot_a.ld`
- `Project_Settings/Linker_Files/S32K144_app_slot_b.ld`
- `Bootloader/linker/S32K144_pbl_flexnvm.ld`
- `Makefile.application_v2`
- `tools/check_v2_layout.py`
- `tools/check_v2_layout.ps1`
- `docs/bootloader_v2/architecture.md`
- `docs/bootloader_v2/memory_map.md`
- `docs/bootloader_v2/boot_flow.md`

## Modified files

- `Bootloader/inc/boot_cfg.h`: compatibility wrapper for centralized config.
- `Bootloader/inc/boot_jump.h`: generic vector jump API.
- `Bootloader/src/boot.c`: compatibility facade over Boot Manager.
- `Bootloader/src/boot_jump.c`: parameterized vector handover.
- Bootloader validation/flash/request/UDS sources: centralized config include.
- `Bootloader/Makefile`: link-only PBL FlexNVM target.
- `src/AppBootRequest.c`: shared boot request memory contract.
- `src/Dcm.c`: public Application-Bootloader interface.
- `src/CanTp.c`: compile-time handling for ISO-TP block size zero, removing a
  pre-existing `-Wtype-limits` warning without changing runtime behavior.

## Build summary

| Target | Result | Text | Data | BSS | Vector |
|---|---|---:|---:|---:|---:|
| current Bootloader | PASS | 7808 | 0 | 1336 | `0x00000000` |
| current Application | PASS | 29444 | 2408 | 23832 | `0x00008000` |
| APP_A ELF/BIN/SREC | PASS | 29444 | 2408 | 23832 | `0x00008000` |
| APP_B ELF/BIN/SREC | PASS | 29444 | 2408 | 23832 | `0x00040000` |
| PBL FlexNVM ELF/SREC | LINK PASS | 7808 | 0 | 1336 | `0x10000000` |

The V2-specific builds use `-Wall -Wextra -Werror`. Linker assertions and the
layout checker enforce vector, Flash boundary, and retained-SRAM boundaries.

## Still using the proven implementation

- current Slot A validity uses six-word legacy metadata at `0x0007F000`;
- CRC32 implementation and validation are unchanged;
- flash erase/program remains restricted to the current Application range;
- CAN, ISO-TP, UDS, security access, and Linux flasher protocol are unchanged;
- Boot Manager can select Slot A or Programming Mode only;
- current Bootloader remains linked in PFlash at address zero.

## Stage 1 TODO boundary

- PBL FlexNVM hardware bootability and partition strategy are not validated;
- no RAM SBL/flash-driver loading;
- no header generation/commit or header CRC validation;
- no persistent `BootSlotMetadata` or journal;
- no Slot B erase/program/select/jump workflow;
- no trial boot, confirmation, attempt counting, or rollback;
- no cryptographic authentication.

## Recommended Stage 2 entry

Start with an explicit RAM SBL/flash-driver interface and relocation test while
keeping the current PFlash Bootloader as recovery. Only after RAM execution and
flash-command safety are proven should UDS erase/program become slot-aware.
FlexNVM partitioning and boot-source changes should remain a separately
reviewed hardware procedure with a recovery plan.
