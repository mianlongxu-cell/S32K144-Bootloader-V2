"""Inactive-slot packed-image workflow; no automatic legacy/raw fallback."""
from enum import Enum, auto
import time
from .flasher import FlashOptions, calculate_key
from .packed_image import PackedImage, transfer_chunks, version_text
from .uds import UdsClient, UdsNegativeResponseError
from .update_status import UpdateStatus
from .boot_status import BootStatus

class FlashState(Enum):
    IDLE = auto()
    ENTER_PROGRAMMING_SESSION = auto()
    SECURITY_ACCESS = auto()
    SELECT_TARGET = auto()
    ERASE = auto()
    REQUEST_DOWNLOAD = auto()
    TRANSFER_DATA = auto()
    TRANSFER_EXIT = auto()
    VERIFY = auto()
    RESET = auto()
    DONE = auto()
    INTERRUPTED = auto()
    FAILED = auto()

class V2UdsFlasher:
    def __init__(self, client: UdsClient, options: FlashOptions, initialize_journal=None):
        self.client = client
        self.options = options
        self.state = FlashState.IDLE
        self.initialize_journal = initialize_journal

    def read_slot(self, did: int) -> int:
        data = self.client.read_did(did)
        if len(data) != 1 or data[0] not in (0, 1, 255):
            raise RuntimeError("Malformed slot DID")
        return data[0]

    def read_version(self) -> int:
        data = self.client.read_did(0xF102)
        if len(data) != 4:
            raise RuntimeError("Malformed image version DID")
        return int.from_bytes(data, "big")

    def flash(self, firmware: bytes) -> None:
        try:
            self._flash(firmware)
        except Exception:
            self.state = FlashState.FAILED
            raise

    def _flash(self, firmware: bytes) -> None:
        image = PackedImage.parse(firmware)  # Validate before contacting ECU.
        self.state = FlashState.ENTER_PROGRAMMING_SESSION
        self.client.diagnostic_session(2)
        time.sleep(0.4)
        deadline = time.monotonic() + 5.0
        while True:
            try:
                self.client.diagnostic_session(2)
                break
            except RuntimeError:
                if time.monotonic() >= deadline:
                    raise
                time.sleep(0.2)
        self.state = FlashState.SECURITY_ACCESS
        if not self.options.skip_security:
            self.client.security_key(calculate_key(self.client.security_seed()))
        self.state = FlashState.SELECT_TARGET
        status = None
        try:
            status = UpdateStatus.parse(self.client.read_did(0xF104))
        except UdsNegativeResponseError as error:
            if error.request_sid != 0x22 or error.nrc not in (0x11, 0x31):
                raise
            print("Stage3 ECU: Update Journal DID unavailable; using original full-update workflow.")
        if status is not None:
            status.describe()
            try:
                BootStatus.parse(self.client.read_did(0xF105)).describe()
            except UdsNegativeResponseError as error:
                if error.request_sid != 0x22 or error.nrc not in (0x11, 0x31):
                    raise
                print("Stage4 ECU: lifecycle DID F105 unavailable.")
        if self.initialize_journal is not None:
            if status is None:
                raise RuntimeError("Journal initialization requires Stage4 Bootloader")
            if status.valid:
                raise RuntimeError("Journal already valid; remove --initialize-journal")
            selected = {"A": 0, "B": 1, "none": 255}[self.initialize_journal]
            self.client.routine_start(0xFF02, bytes([selected]))
            status = UpdateStatus.parse(self.client.read_did(0xF104))
            status.describe()
        if status is not None:
            status.require_ready()
        active = self.read_slot(0xF101)
        target = self.read_slot(0xF103)
        current_version = self.read_version()
        expected = 0 if active == 255 else 1 - active
        if target != expected or target != image.slot:
            raise ValueError("Image slot does not match ECU inactive slot; build/pack the other slot")
        if active != 255 and image.version <= current_version:
            raise ValueError("Target version must be higher than active version")
        if self.options.address is not None and self.options.address != image.address:
            raise ValueError("--address conflicts with packed image")
        slot_name = lambda s: "NONE" if s == 255 else "AB"[s]
        print(f"Active Slot: {slot_name(active)}   Target Slot: {slot_name(target)}")
        print(f"Current Version: {version_text(current_version)}")
        print(f"Target Version: {version_text(image.version)}")
        print(f"Packed Size: {len(firmware)} bytes   Payload: {image.image_size} bytes")

        self.state = FlashState.ERASE
        params = image.address.to_bytes(4, "big") + len(firmware).to_bytes(4, "big")
        self.client.routine_start(0xFF00, params)
        print("Erase: PASS")
        self.state = FlashState.REQUEST_DOWNLOAD
        maximum = self.client.request_download(image.address, len(firmware))
        chunks = list(transfer_chunks(firmware, maximum))
        self.state = FlashState.TRANSFER_DATA
        sent = 0
        for index, chunk in enumerate(chunks):
            seq = (index + 1) & 255
            if self.options.wrong_sequence and index == 1:
                seq = (seq + 1) & 255
            if self.options.wrong_crc and index == 0:
                altered = bytearray(chunk)
                # Corrupt a payload byte, leaving packed header CRC untouched.
                altered[min(8, len(altered) - 1)] ^= 1
                chunk = bytes(altered)
            self.client.transfer_data(seq, chunk)
            sent += len(chunk)
            percent = sent * 100 // len(firmware)
            print(f"\rProgramming: Block {index + 1}/{len(chunks)}  {percent:3d}%", end="", flush=True)
            if self.options.stop_at_percent is not None and percent >= self.options.stop_at_percent:
                self.state = FlashState.INTERRUPTED
                print("\nIntentionally interrupted; use board reset to return to the old App.")
                return
        print()
        self.state = FlashState.TRANSFER_EXIT
        self.client.request_transfer_exit()
        print("TransferExit: PASS")
        self.state = FlashState.VERIFY
        self.client.routine_start(0xFF01)
        print("Image Verify: PASS")
        self.state = FlashState.RESET
        self.client.ecu_reset()
        deadline = time.monotonic() + 8.0
        while True:
            time.sleep(0.25)
            try:
                if self.read_slot(0xF101) == target and self.read_version() == image.version:
                    break
            except RuntimeError:
                pass
            if time.monotonic() >= deadline:
                raise RuntimeError("Reset completed but target Slot/Version could not be confirmed")
        self.state = FlashState.DONE
        print(f"ECU Reset: PASS; running Slot {'AB'[target]} V{version_text(image.version)}")
        print("Firmware update SUCCESS")
