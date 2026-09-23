"""Bootloader-only F104, fixed 32-byte payload (network byte order)."""
from dataclasses import dataclass
import struct

STATES = ("IDLE", "PREPARING", "ERASING", "DOWNLOAD_READY", "PROGRAMMING",
          "TRANSFER_COMPLETE", "VERIFYING", "VERIFIED", "PENDING_ACTIVATION", "ABORTED")
RESULTS = ("NONE", "SUCCESS", "INTERRUPTED", "INTERRUPTED_DURING_PROGRAMMING",
           "ERASE_FAILED", "PROGRAM_FAILED", "TRANSFER_FAILED", "VERIFY_FAILED",
           "BAD_IMAGE", "JOURNAL_ERROR", "POWER_LOSS_RECOVERED", "COMMISSIONED")

def slot_text(slot: int) -> str:
    return "AB"[slot] if slot in (0, 1) else "UNKNOWN"

@dataclass(frozen=True)
class UpdateStatus:
    valid: bool
    fault: bool
    transaction_id: int
    state: int
    active: int
    target: int
    expected: int
    committed: int
    result: int

    @classmethod
    def parse(cls, data: bytes):
        if len(data) != 32:
            raise RuntimeError("Malformed F104 length")
        abi, valid, fault, reserved, *values = struct.unpack(">4B7I", data)
        if abi != 1 or valid > 1 or fault > 1 or reserved:
            raise RuntimeError("Unsupported/malformed F104 format")
        transaction, state, active, target, expected, committed, result = values
        if state >= len(STATES) or result >= len(RESULTS) or committed > expected or any(
                slot not in (0, 1, 0x7FFFFFFF) for slot in (active, target)):
            raise RuntimeError("Malformed F104 fields")
        return cls(bool(valid), bool(fault), *values)

    def describe(self) -> None:
        print(f"Journal: {'VALID' if self.valid else 'INVALID'}; Transaction {self.transaction_id}")
        print(f"Update State: {STATES[self.state]}; Last Result: {RESULTS[self.result]}")
        print(f"Previous Active: {slot_text(self.active)}; Target: {slot_text(self.target)}")
        print(f"Durable checkpoint: {self.committed}/{self.expected} (not a resume offset)")
        if 1 <= self.state <= 7 or self.state == 9 and self.result not in (0, 11):
            print("Previous update interrupted/failed. Restarting FULL update; no resume.")

    def require_ready(self) -> None:
        if self.fault:
            raise RuntimeError("Journal storage error: stop; inspect ECU/reset before another update")
        if not self.valid:
            raise RuntimeError("Journal invalid: fail-safe. Explicit --initialize-journal A/B/none required")
