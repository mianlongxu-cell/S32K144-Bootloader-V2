"""Read actual ELF32 sections/symbols and verify RAM driver/link boundaries."""
import argparse
import re
import struct
import subprocess
from pathlib import Path

class Elf:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        assert self.data[:6] == b"\x7fELF\x01\x01", "Expected little-endian ELF32"
        head = struct.unpack_from("<16sHHIIIIIHHHHHH", self.data)
        shoff, shentsize, shnum, shstr = head[6], head[11], head[12], head[13]
        raw = [struct.unpack_from("<10I", self.data, shoff + i * shentsize) for i in range(shnum)]
        strings = self.data[raw[shstr][4]:raw[shstr][4] + raw[shstr][5]]
        self.sections = {}
        self.symbols = {}
        self.undefined = []
        for s in raw:
            name = strings[s[0]:].split(b"\0", 1)[0].decode()
            self.sections[name] = s
            if s[1] == 2:  # SYMTAB
                names = raw[s[6]]
                pool = self.data[names[4]:names[4] + names[5]]
                for pos in range(s[4], s[4] + s[5], s[9]):
                    n, value, size, info, other, index = struct.unpack_from("<IIIBBH", self.data, pos)
                    text = pool[n:].split(b"\0", 1)[0].decode()
                    if text:
                        self.symbols[text] = value
                        if index == 0 and info >> 4 == 1:
                            self.undefined.append(text)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--objdump", default="arm-none-eabi-objdump")
    parser.add_argument("--app-root", default="Application_Build")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    driver_path = root / "FlashDriver/build/flash_driver.elf"
    driver = Elf(driver_path)
    boot = Elf(root / "Bootloader/build/Bootloader.elf")
    assert boot.symbols["__JournalCopy0"] == 0x78000
    assert boot.symbols["__JournalCopy1"] == 0x79000
    assert boot.symbols["__JournalSectorSize"] == 0x1000
    assert [boot.symbols[name] for name in ("__MetadataA0", "__MetadataA1", "__MetadataB0", "__MetadataB1")] == [0x7A000,0x7B000,0x7C000,0x7D000]
    assert "BootFault_ArmedPoint" not in boot.symbols, "Final Bootloader must have injection disabled"
    fault_path = root / args.app_root / "FaultInjection/Bootloader.elf"
    if fault_path.exists():
        assert "BootFault_ArmedPoint" in Elf(fault_path).symbols
    assert not driver.undefined, driver.undefined
    for name, s in driver.sections.items():
        if s[2] & 2 and s[5]:  # SHF_ALLOC
            assert 0x20005800 <= s[3] < s[3] + s[5] <= 0x20006000, (name, s)
    api = driver.sections[".api"]
    assert api[3] == 0x20005800 and api[5] == 44
    values = struct.unpack_from("<11I", driver.data, api[4])
    assert values[:2] == (0x46445233, 3)
    code = driver.sections[".text"]
    for pointer in values[2:]:
        assert pointer & 1 and code[3] <= pointer & ~1 < code[3] + code[5]
    disasm = subprocess.check_output([args.objdump, "-d", str(driver_path)], text=True)
    for match in re.finditer(r"\bbl(?:\.w)?\s+([0-9a-fA-F]+)\s+<", disasm):
        assert 0x20005800 <= int(match[1], 16) < 0x20006000, match[0]
    assert not re.search(r"\bblx\s+r", disasm), "Unexpected indirect driver dependency"
    assert boot.symbols["__BSS_END"] <= 0x20005800
    assert boot.symbols["__FlashDriverStart"] == 0x20005800
    assert boot.symbols["__FlashDriverEnd"] == boot.symbols["__StackLimit"] == 0x20006000
    assert boot.symbols["__StackTop"] == 0x20006FF0
    for name in ("BootProgramming_Context", "BootProgramming_TransferBuffer", "BootUpdate_Current"):
        assert 0x1FFF8000 <= boot.symbols[name] < boot.symbols["__BSS_END"]
    assert boot.symbols["BootProgramming_TransferBuffer"] % 8 == 0
    for slot, base, limit in (("A", 0x8000, 0x3F000), ("B", 0x40000, 0x77000)):
        app = Elf(root / args.app_root / f"APP_{slot}/APP_{slot}.elf")
        assert app.sections[".interrupts"][3] == base
        assert base <= app.sections[".text"][3] < app.sections[".text"][3]+app.sections[".text"][5] <= limit
        assert app.symbols["__StackTop"] == 0x20006FF0
        assert app.symbols["__HeapLimit"] <= 0x20005800
        assert app.symbols["__StackLimit"] >= 0x20006000
        print(f"PASS APP_{slot}: vector=0x{base:08X}, header boundary=0x{limit:08X}")
    print("PASS driver: API/code/data in SRAM; no undefined symbols or calls to PFlash")
    print(f"PASS driver image={driver.symbols['__driver_image_end']-0x20005800} bytes, "
          f"RAM={driver.symbols['__driver_end']-0x20005800} bytes")
    print("PASS Bootloader data/context/transfer, driver and stack do not overlap")
    for name in ("BootProgramming_Context", "BootProgramming_TransferBuffer", "BootUpdate_Current", "__BSS_END",
                 "__JournalCopy0", "__JournalCopy1", "__MetadataA0", "__MetadataA1", "__MetadataB0", "__MetadataB1"):
        print(f"{name}: 0x{boot.symbols[name]:08X}")

if __name__ == "__main__":
    main()
