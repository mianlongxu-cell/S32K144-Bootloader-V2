# Third-party components

This repository contains or depends on third-party material. File-level
copyright and license notices take precedence over the root `LICENSE`.

| Component | Location | License/status |
|---|---|---|
| FreeRTOS Kernel | `src/FreeRTOS-Kernel/`, `include/FreeRTOS/` | MIT; original notices and `src/FreeRTOS-Kernel/LICENSE.md` retained |
| NXP startup and device support | `include/`, `Project_Settings/Startup_Code/` | Mixed: some files are BSD-3-Clause; use the file-level notice |
| NXP device headers and generated linker/system files | local S32DS workspace | Several inspected files carry confidential/proprietary notices and are excluded from the public Git tree |
| Arm GCC runtime/toolchain | build prerequisite only | Not distributed by this repository |
| S32 Design Studio / PEmicro | development tools only | Not distributed by this repository |
| AI-generated Kagura fan-art banner | `images/kagura-ecu-banner.png` | Unofficial fan art; character rights belong to the respective rights holders; excluded from the repository MIT license |

## Files intentionally excluded from public source

The following local files are required by the current S32DS project but are
not redistributed because the inspected copies carry restrictive notices:

- `include/S32K144.h`
- `include/S32K144_features.h`
- `Project_Settings/Startup_Code/system_S32K144.c`
- `Project_Settings/Linker_Files/S32K144_64_flash.ld`
- `Project_Settings/Linker_Files/S32K144_64_ram.ld`

Obtain or regenerate these files from a properly licensed NXP S32 Design
Studio/device-support installation. Before publishing a fork, review every
vendor file against the license supplied with the exact installed version.
