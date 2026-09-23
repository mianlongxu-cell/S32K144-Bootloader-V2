"""UDS firmware programming workflow."""

from __future__ import annotations

from dataclasses import dataclass
import binascii
import time

from .uds import DID_SOFTWARE_VERSION, UdsClient

APP_START_ADDRESS = 0x00008000
ROUTINE_ERASE_APP = 0xFF00
ROUTINE_VERIFY_CRC = 0xFF01
SECURITY_XOR = 0xA5C39E71
SECURITY_ROTATE = 7


def crc32(data: bytes) -> int:
    return binascii.crc32(data) & 0xFFFFFFFF


def calculate_key(seed: int) -> int:
    value = (seed ^ SECURITY_XOR) & 0xFFFFFFFF
    return ((value << SECURITY_ROTATE) |
            (value >> (32 - SECURITY_ROTATE))) & 0xFFFFFFFF


@dataclass
class FlashOptions:
    version: int = 0x00020000
    wrong_crc: bool = False
    wrong_sequence: bool = False
    skip_security: bool = False
    address: int | None = None
    stop_at_percent: int | None = None


class UdsFlasher:
    def __init__(self, client: UdsClient, options: FlashOptions) -> None:
        self.client = client
        self.options = options

    def flash(self, firmware: bytes) -> None:
        if self.options.address is None:
            self.options.address = APP_START_ADDRESS
        if not firmware:
            raise ValueError("Firmware file is empty")
        firmware_crc = crc32(firmware)
        print(f"Firmware size: {len(firmware)} bytes")
        print(f"CRC32: 0x{firmware_crc:08X}")

        print("Programming Session (Application)...", end=" ", flush=True)
        self.client.diagnostic_session(0x02)
        print("OK")
        time.sleep(0.4)
        print("Programming Session (Bootloader)...", end=" ", flush=True)
        self._retry_programming_session()
        print("OK")

        if not self.options.skip_security:
            print("Security Access...", end=" ", flush=True)
            seed = self.client.security_seed()
            self.client.security_key(calculate_key(seed))
            print("OK")

        parameters = (self.options.address.to_bytes(4, "big") +
                      len(firmware).to_bytes(4, "big"))
        print("Erase Application...", end=" ", flush=True)
        self.client.routine_start(ROUTINE_ERASE_APP, parameters)
        print("OK")

        print("Request Download...", end=" ", flush=True)
        max_block = self.client.request_download(self.options.address,
                                                 len(firmware))
        payload_size = min(128, max_block - 2)
        payload_size -= payload_size % 8
        if payload_size <= 0:
            raise RuntimeError("ECU advertised an unusable TransferData size")
        print(f"OK ({payload_size} data bytes/block)")

        sequence = 1
        sent = 0
        injected = False
        while sent < len(firmware):
            chunk = firmware[sent : sent + payload_size]
            tx_sequence = sequence
            if self.options.wrong_sequence and not injected and sent > 0:
                tx_sequence = (sequence + 1) & 0xFF
                injected = True
            self.client.transfer_data(tx_sequence, chunk)
            sent += len(chunk)
            sequence = (sequence + 1) & 0xFF
            percent = sent * 100 // len(firmware)
            width = 20
            filled = percent * width // 100
            print(f"\rTransfer: [{'#' * filled}{' ' * (width - filled)}] {percent:3d}%",
                  end="", flush=True)
            if (self.options.stop_at_percent is not None and
                    percent >= self.options.stop_at_percent):
                print("\nTransfer intentionally interrupted")
                return
        print()

        print("Transfer Exit...", end=" ", flush=True)
        self.client.request_transfer_exit()
        print("OK")

        verify_crc = firmware_crc ^ (1 if self.options.wrong_crc else 0)
        verify_parameters = (len(firmware).to_bytes(4, "big") +
                             verify_crc.to_bytes(4, "big") +
                             self.options.version.to_bytes(4, "big"))
        print("Verify CRC...", end=" ", flush=True)
        self.client.routine_start(ROUTINE_VERIFY_CRC, verify_parameters)
        print("OK")

        print("ECU Reset...", end=" ", flush=True)
        self.client.ecu_reset()
        print("OK")
        print("Waiting for Application...", end=" ", flush=True)
        version = self._wait_for_application()
        print(f"OK (DID F100: {version})")
        print("Firmware update SUCCESS")

    def _retry_programming_session(self) -> None:
        deadline = time.monotonic() + 5.0
        last_error: Exception | None = None
        while time.monotonic() < deadline:
            try:
                self.client.diagnostic_session(0x02)
                return
            except RuntimeError as error:
                last_error = error
                time.sleep(0.2)
        raise RuntimeError(f"Bootloader did not enter programming mode: {last_error}")

    def _wait_for_application(self) -> str:
        deadline = time.monotonic() + 8.0
        last_error: Exception | None = None
        time.sleep(0.5)
        while time.monotonic() < deadline:
            try:
                return self.client.read_did(DID_SOFTWARE_VERSION).decode("ascii")
            except (RuntimeError, UnicodeDecodeError) as error:
                last_error = error
                time.sleep(0.25)
        raise RuntimeError(f"Application did not restart: {last_error}")
