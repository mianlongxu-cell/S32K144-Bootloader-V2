#!/usr/bin/env python3
"""Verify Stage 1 ELF section placement without modifying any target memory."""

from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys


SECTION_RE = re.compile(
    r"^\s*\d+\s+(\S+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+"
    r"([0-9a-fA-F]+)\s+"
)


def read_sections(objdump: str, elf: pathlib.Path) -> dict[str, tuple[int, int, int]]:
    result = subprocess.run(
        [objdump, "-h", str(elf)],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    )
    sections: dict[str, tuple[int, int, int]] = {}
    for line in result.stdout.splitlines():
        match = SECTION_RE.match(line)
        if match:
            name, size, vma, lma = match.groups()
            sections[name] = (int(size, 16), int(vma, 16), int(lma, 16))
    return sections


def check_image(
    objdump: str,
    label: str,
    elf: pathlib.Path,
    vector: int,
    flash_start: int,
    flash_end: int,
) -> list[str]:
    errors: list[str] = []
    sections = read_sections(objdump, elf)
    interrupts = sections.get(".interrupts")
    if interrupts is None or interrupts[1] != vector:
        actual = "missing" if interrupts is None else f"0x{interrupts[1]:08X}"
        errors.append(f"{label}: vector is {actual}, expected 0x{vector:08X}")

    for name, (size, _vma, lma) in sections.items():
        if size == 0 or name.startswith(".debug"):
            continue
        if flash_start <= lma < flash_end and lma + size > flash_end:
            errors.append(
                f"{label}: {name} LMA 0x{lma:08X}+0x{size:X} exceeds "
                f"0x{flash_end:08X}"
            )

    for name, (size, vma, _lma) in sections.items():
        if size == 0:
            continue
        if 0x1FFF8000 <= vma < 0x20006FF0 and vma + size > 0x20006FF0:
            errors.append(f"{label}: {name} exceeds retained-request SRAM boundary")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--objdump", default="arm-none-eabi-objdump")
    parser.add_argument("--root", type=pathlib.Path,
                        default=pathlib.Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.root.resolve()

    images = [
        ("Bootloader", root / "Bootloader/build/Bootloader.elf",
         0x00000000, 0x00000000, 0x00008000),
        ("PBL_FlexNVM", root / "Bootloader/build/PBL_FlexNVM.elf",
         0x10000000, 0x10000000, 0x1000C000),
        ("APP_A", root / "Application_Build/APP_A/APP_A.elf",
         0x00008000, 0x00008000, 0x0003F000),
        ("APP_B", root / "Application_Build/APP_B/APP_B.elf",
         0x00040000, 0x00040000, 0x00077000),
    ]

    errors: list[str] = []
    for label, elf, vector, start, end in images:
        if not elf.exists():
            errors.append(f"{label}: missing {elf}")
            continue
        errors.extend(check_image(args.objdump, label, elf, vector, start, end))
        print(f"PASS {label}: vector=0x{vector:08X}, limit=0x{end:08X}")

    if errors:
        for error in errors:
            print(f"FAIL {error}", file=sys.stderr)
        return 1
    print("PASS V2 layout: no checked Flash/RAM overlap")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
