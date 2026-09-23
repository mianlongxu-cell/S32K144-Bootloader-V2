#!/usr/bin/env python3
"""Create metadata and a combined initial-programming firmware image."""

from __future__ import annotations

import argparse
import binascii
from pathlib import Path
import struct
import subprocess

METADATA_ADDRESS = 0x0007F000
METADATA_MAGIC = 0x53394D44
METADATA_VALID = 0x56414C49


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--application-elf", type=Path, required=True)
    parser.add_argument("--application-bin", type=Path, required=True)
    parser.add_argument("--bootloader-elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--objcopy", default="arm-none-eabi-objcopy")
    parser.add_argument("--ld", default="arm-none-eabi-ld")
    parser.add_argument("--version", type=lambda value: int(value, 0),
                        default=0x00010000)
    args = parser.parse_args()

    firmware = args.application_bin.read_bytes()
    crc = binascii.crc32(firmware) & 0xFFFFFFFF
    metadata = struct.pack("<6I", METADATA_MAGIC, len(firmware), crc,
                           args.version, METADATA_VALID, 0xFFFFFFFF)
    output_dir = args.output.parent
    output_dir.mkdir(parents=True, exist_ok=True)
    metadata_path = output_dir / "FirmwareMetadata.bin"
    metadata_path.write_bytes(metadata)

    boot_binary = output_dir / "BootloaderForCombine.bin"
    subprocess.run([args.objcopy, "-O", "binary", args.bootloader_elf,
                    boot_binary], check=True)
    inputs = (
        (boot_binary, output_dir / "BootloaderForCombine.o", ".boot"),
        (args.application_bin, output_dir / "ApplicationForCombine.o", ".app"),
        (metadata_path, output_dir / "MetadataForCombine.o", ".metadata"),
    )
    for source, target, section in inputs:
        subprocess.run([
            args.objcopy, "-I", "binary", "-O", "elf32-littlearm", "-B", "arm",
            "--rename-section",
            f".data={section},alloc,load,readonly,data,contents",
            source, target,
        ], check=True)
    linker_script = Path(__file__).with_name("combined_firmware.ld")
    subprocess.run([
        args.ld, "-T", linker_script, "-o", args.output,
        *(str(target) for _, target, _ in inputs),
    ], check=True)
    print(f"Firmware size: {len(firmware)}")
    print(f"Firmware CRC32: 0x{crc:08X}")
    print(f"Combined image: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
