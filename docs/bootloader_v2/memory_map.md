# Bootloader V2 Memory Map

## Device memories used by this design

| Memory | Range | Size | Stage 1 use |
|---|---|---:|---|
| PFlash | `0x00000000-0x0007FFFF` | 512 KiB | Current Bootloader/Application and planned A/B images |
| FlexNVM/D-Flash address space | `0x10000000-0x1000FFFF` | 64 KiB | PBL link draft only; no partition/program action |
| SRAM | `0x1FFF8000-0x20006FFF` | 60 KiB | code data, heap, stack; final 16 bytes reserved |
| FlexRAM | `0x14000000-0x14000FFF` | 4 KiB | not used in Stage 1 |

The project intentionally does not issue PGMPART, alter FlexNVM partitioning,
configure CSEc, or erase FlexNVM.

## Legacy reference (read-only compatibility in Stage 3)

This table describes the old V1 image only. Stage 3 never uses the broad V1
range for erase/program; all writes use the bounded inactive A/B slot below.

| Region | Start | End | Size |
|---|---:|---:|---:|
| Bootloader | `0x00000000` | `0x00007FFF` | 32 KiB |
| Application vector/payload | `0x00008000` | `0x0007EFFF` | 476 KiB |
| Legacy metadata | `0x0007F000` | `0x0007FFFF` | 4 KiB |
| SRAM used by linker | `0x1FFF8000` | `0x20006FEF` | 60 KiB - 16 B |
| retained boot request | `0x20006FF0` | `0x20006FFF` | 16 B: magic/inverse + slot/inverse |

Historical Stage 1 build observations (addresses/sizes may change on rebuild):

- Bootloader vector `0x00000000`, Reset Handler `0x00000410`;
- Bootloader `.text` starts `0x00000410`, size `0x1DD0`;
- Application vector `0x00008000`, Reset Handler `0x00008580`;
- Application `.text` starts `0x00008400`, size `0x6EFC`;
- Application `.data` VMA `0x1FFF8400`, `.bss` VMA `0x20000000`;
- Application stack `0x20006BF0-0x20006FEF`, MSP `0x20006FF0`.

## V2 A/B PFlash layout

Stages 2 and 3 retain this Stage 1 Flash layout. Stage 3 UDS writes only the
inactive slot as a full packed image; the legacy broad-range write is disabled.

| Region | Start | End | Size | Purpose |
|---|---:|---:|---:|---|
| compatibility/recovery | `0x00000000` | `0x00007FFF` | 32 KiB | keeps current Bootloader/FCF path |
| Slot A | `0x00008000` | `0x0003FFFF` | 224 KiB | primary image |
| Slot A payload | `0x00008000` | `0x0003EFFF` | 220 KiB | vector plus code/load image |
| Slot A header | `0x0003F000` | `0x0003FFFF` | 4 KiB | 64-byte header plus erased reserve |
| Slot B | `0x00040000` | `0x00077FFF` | 224 KiB | alternate image |
| Slot B payload | `0x00040000` | `0x00076FFF` | 220 KiB | vector plus code/load image |
| Slot B header | `0x00077000` | `0x00077FFF` | 4 KiB | 64-byte header plus erased reserve |
| Journal Copy 0 | `0x00078000` | `0x00078FFF` | 4 KiB | Stage4: first 128 bytes are record |
| Journal Copy 1 | `0x00079000` | `0x00079FFF` | 4 KiB | Stage4: first 128 bytes are record |
| Slot A Metadata Copy 0 | `0x0007A000` | `0x0007AFFF` | 4 KiB | first 80 bytes used |
| Slot A Metadata Copy 1 | `0x0007B000` | `0x0007BFFF` | 4 KiB | first 80 bytes used |
| Slot B Metadata Copy 0 | `0x0007C000` | `0x0007CFFF` | 4 KiB | first 80 bytes used |
| Slot B Metadata Copy 1 | `0x0007D000` | `0x0007DFFF` | 4 KiB | first 80 bytes used |
| remaining shared reserve | `0x0007E000` | `0x0007EFFF` | 4 KiB | untouched |
| legacy metadata | `0x0007F000` | `0x0007FFFF` | 4 KiB | read-only compatibility |

All erase boundaries use 4 KiB alignment; program phrases use 8-byte
alignment; vector regions reserve `0x400` bytes. Slot A deliberately retains
the current vector address to minimize migration risk.

## V2 planned FlexNVM layout

| Region | Start | End | Size | Status |
|---|---:|---:|---:|---|
| PBL | `0x10000000` | `0x1000BFFF` | 48 KiB | link-only draft |
| persistent boot data | `0x1000C000` | `0x1000FFFF` | 16 KiB | reserved, no writes |

The PBL draft links with vector `0x10000000`, Reset Handler `0x10000410`, and
`.text` at `0x10000410`. Successful linking does not prove that the device is
partitioned or configured to boot from FlexNVM. Hardware boot-source,
partition, protection, and recovery procedures must be validated before any
programming is attempted.

## Slot linker results

| Output | Vector | `.text` | Reset Handler | `main` | Header boundary |
|---|---:|---:|---:|---:|---:|
| APP_A | `0x00008000` | `0x00008400` | `0x00008504` | `0x0000C04C` | `< 0x0003F000` |
| APP_B | `0x00040000` | `0x00040400` | `0x00040504` | `0x0004404C` | `< 0x00077000` |

Both use the same sources and identical RAM layout. Linker assertions reject
payload/header overlap and RAM heap/stack overflow.

## Stage 2 complete image format

The host packer creates one full `0x38000`-byte slot image:

```text
slot offset 0x00000 : vector table + linked payload
                      image_size bytes, image_crc32 covers exactly this range
remaining gap       : 0xFF
slot offset 0x37000 : 64-byte BootImageHeader
                      header_crc32 covers header bytes 0x00..0x3B
remaining sector    : 0xFF
```

For Slot A the header offset resolves to `0x0003F000`; for Slot B it resolves
to `0x00077000`. The complete BIN must be programmed at the matching slot base,
not at address zero.

## RAM suballocation (Stage3 historical addresses)

Flash partitions above are unchanged. The previously unassigned SRAM gap is
now reserved for an independent RAM driver:

| Object | Start | Exclusive end / size |
|---|---:|---:|
| Bootloader data/BSS (including contexts) | `0x1FFF8000` | `0x1FFF8284` |
| Programming Context | `0x1FFF8200` | 112 bytes |
| Transfer phrase buffer | `0x1FFF8270` | 8 bytes, aligned |
| Driver reservation / API | `0x20005800` | `0x20006000` (2 KiB) |
| Bootloader stack | `0x20006000` | `0x20006FF0` (4080 bytes) |
| Retained request | `0x20006FF0` | `0x20007000` |

Application FreeRTOS heap is 16 KiB in BSS; its C heap ends at `0x20005438`,
and its stack starts at `0x20006BF0`. Linker assertions prevent either heap
or stack from growing into the driver reservation. The Bootloader and App
execute in separate reset lifetimes. See [flash_driver.md](flash_driver.md).

## Stage4 Journal allocation

The existing 32 KiB shared reservation supplies two independent 4 KiB sectors.
No A/B base, payload limit, Header, Bootloader, FlexNVM, driver or stack address
is moved. Each commit erases the other sector (4 KiB), programs 16 aligned
8-byte phrases (128 bytes), CRC/marker phrase last, and validates readback.
Record RAM alignment is 8 bytes, format version 1, CRC offset 120, marker 124.
The unused 3968 bytes per Journal sector remain erased. Independent Journal
API entry points cannot write Slot Metadata or legacy Metadata at 0x7F000.
Stage5 adds separate narrowly bounded Metadata entry points for exactly the four
0x7A000..0x7DFFF sectors. No linker or Slot address moved.

Linker absolute symbols `__JournalCopy0`, `__JournalCopy1`, `__JournalSectorSize`
make these addresses visible in .map/ELF without creating loadable Flash segments.
Current Stage4 RAM/symbol observations and image sizes are recorded in
[stage4_update_journal.md](stage4_update_journal.md); rerun the ELF checker after
each rebuild instead of assuming historical BSS addresses remain unchanged.
