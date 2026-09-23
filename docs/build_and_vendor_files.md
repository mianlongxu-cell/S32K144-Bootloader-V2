# Build and vendor-file setup

## Why some files are absent from public Git

The local S32DS project contains NXP-generated device headers, system startup,
and linker files. Several inspected copies explicitly state confidential or
proprietary terms. They remain on the developer machine but are excluded from
the public Git tree. See `THIRD_PARTY.md` for the exact paths.

This keeps the repository publishable without claiming redistribution rights
that the project does not grant. A public clone is therefore a source showcase
until the user supplies equivalent files from a legally licensed NXP package.

## Restore prerequisites

Install the S32K144 device support for the intended S32 Design Studio version,
then regenerate the project files or copy the required files from that licensed
installation into their documented paths. Do not copy files from an unrelated
commercial project or SDK installation whose terms prohibit redistribution/use.

After restoring local dependencies, the expected layout includes:

```text
include/S32K144.h
include/S32K144_features.h
Project_Settings/Startup_Code/system_S32K144.c
Project_Settings/Linker_Files/S32K144_64_flash.ld
Project_Settings/Linker_Files/S32K144_64_ram.ld
```

## S32DS Application build

Import the repository using the retained `.project` and `.cproject`. The tested
Flash build uses the Application vector origin at `0x00008000`.

The independent Makefile can also build the Application when `arm-none-eabi-*`
tools and vendor prerequisites are on `PATH`:

```bash
make -f Makefile.application clean
make -f Makefile.application all
```

Outputs are generated in `Application_Build/` and ignored by Git.

## Bootloader build

```bash
make -C Bootloader clean
make -C Bootloader all
```

Outputs are generated in `Bootloader/build/` and ignored by Git.

## Combined initial-programming image

`tools/package_firmware.py` packages a Bootloader ELF, Application ELF/bin, and
valid metadata into a combined ELF for initial debugger programming. It requires
`arm-none-eabi-objcopy`, `arm-none-eabi-ld`, and Python 3. Example:

```bash
python3 tools/package_firmware.py \
  --application-elf Application_Build/Application.elf \
  --application-bin Application_Build/Application.bin \
  --bootloader-elf Bootloader/build/Bootloader.elf \
  --output Application_Build/Combined_Firmware.elf \
  --version 0x00020000
```

Never commit generated ELF/map/object files. Firmware binaries should be release
assets only after third-party redistribution rights and checksums are verified.
