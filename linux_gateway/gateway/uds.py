"""UDS client and DEM/DTC decoding for the S32K144 Vehicle ECU."""

from __future__ import annotations

from dataclasses import dataclass
import time
from typing import Dict, List

from .isotp import IsoTpClient


SID_DIAGNOSTIC_SESSION_CONTROL = 0x10
SID_CLEAR_DIAGNOSTIC_INFORMATION = 0x14
SID_READ_DTC_INFORMATION = 0x19
SID_READ_DATA_BY_IDENTIFIER = 0x22
SID_TESTER_PRESENT = 0x3E
SID_ECU_RESET = 0x11
SID_SECURITY_ACCESS = 0x27
SID_ROUTINE_CONTROL = 0x31
SID_REQUEST_DOWNLOAD = 0x34
SID_TRANSFER_DATA = 0x36
SID_REQUEST_TRANSFER_EXIT = 0x37
POSITIVE_RESPONSE_OFFSET = 0x40

DID_VIN = 0xF190
DID_SOFTWARE_VERSION = 0xF100
DID_VEHICLE_SPEED = 0x0101
DID_ENGINE_RPM = 0x0102
DID_COOLANT_TEMPERATURE = 0x0103
DID_GEAR = 0x0104

DID_NAMES: Dict[str, int] = {
    "vin": DID_VIN,
    "sw": DID_SOFTWARE_VERSION,
    "speed": DID_VEHICLE_SPEED,
    "rpm": DID_ENGINE_RPM,
    "coolant": DID_COOLANT_TEMPERATURE,
    "gear": DID_GEAR,
}

NRC_NAMES = {
    0x11: "ServiceNotSupported",
    0x12: "SubFunctionNotSupported",
    0x13: "IncorrectMessageLengthOrInvalidFormat",
    0x31: "RequestOutOfRange",
    0x22: "ConditionsNotCorrect",
    0x24: "RequestSequenceError",
    0x33: "SecurityAccessDenied",
    0x35: "InvalidKey",
    0x70: "UploadDownloadNotAccepted",
    0x71: "TransferDataSuspended",
    0x72: "GeneralProgrammingFailure",
    0x73: "WrongBlockSequenceCounter",
    0x78: "ResponsePending",
}


class UdsError(RuntimeError):
    pass


class UdsNegativeResponseError(UdsError):
    def __init__(self, request_sid: int, nrc: int) -> None:
        self.request_sid = request_sid
        self.nrc = nrc
        name = NRC_NAMES.get(nrc, "UnknownNRC")
        super().__init__(
            f"UDS negative response: SID 0x{request_sid:02X}, "
            f"NRC 0x{nrc:02X} ({name})"
        )


@dataclass(frozen=True)
class SessionTiming:
    session: int
    p2_server_max_ms: int
    p2_star_server_max_ms: int
    raw_response: bytes


@dataclass(frozen=True)
class DtcRecord:
    code: int
    status: int

    @property
    def status_names(self) -> List[str]:#判断状态位：
        names: List[str] = []
        if self.status & 0x01:
            names.append("testFailed")
        if self.status & 0x04:
            names.append("pendingDTC")
        if self.status & 0x08:
            names.append("confirmedDTC")
        return names


@dataclass(frozen=True)
class DtcReport:
    status_availability_mask: int
    records: List[DtcRecord]
    raw_response: bytes


class UdsClient:
    def __init__(self, transport: IsoTpClient) -> None:
        self._transport = transport
        self._p2_star_seconds = 5.0

    def diagnostic_session(self, session: int) -> SessionTiming:
        response = self._exchange(
            bytes((SID_DIAGNOSTIC_SESSION_CONTROL, session)),
            SID_DIAGNOSTIC_SESSION_CONTROL,
        )
        if len(response) != 6 or response[1] != session:
            raise UdsError("Malformed DiagnosticSessionControl response")
        p2_ms = int.from_bytes(response[2:4], "big")
        p2_star_ms = int.from_bytes(response[4:6], "big") * 10
        if p2_ms <= 0 or p2_star_ms <= 0:
            raise UdsError("Invalid zero P2/P2* timing")
        self._p2_star_seconds = min(p2_star_ms / 1000.0 + 0.1, 10.0)
        return SessionTiming(session, p2_ms, p2_star_ms, response)

    def tester_present(self) -> bytes:
        response = self._exchange(
            bytes((SID_TESTER_PRESENT, 0x00)), SID_TESTER_PRESENT
        )
        if len(response) != 2 or response[1] != 0x00:
            raise UdsError("Malformed TesterPresent response")
        return response

    def security_seed(self) -> int:
        response = self._exchange(bytes((SID_SECURITY_ACCESS, 0x01)),
                                  SID_SECURITY_ACCESS)
        if len(response) != 6 or response[1] != 0x01:
            raise UdsError("Malformed SecurityAccess seed response")
        return int.from_bytes(response[2:6], "big")

    def security_key(self, key: int) -> bytes:
        response = self._exchange(
            bytes((SID_SECURITY_ACCESS, 0x02)) + key.to_bytes(4, "big"),
            SID_SECURITY_ACCESS,
        )
        if response != bytes((0x67, 0x02)):
            raise UdsError("Malformed SecurityAccess key response")
        return response

    def routine_start(self, routine_id: int, parameters: bytes = b"") -> bytes:
        request = bytes((SID_ROUTINE_CONTROL, 0x01)) + routine_id.to_bytes(2, "big") + parameters
        response = self._exchange(request, SID_ROUTINE_CONTROL)
        if len(response) != 4 or response[1] != 0x01 or int.from_bytes(response[2:4], "big") != routine_id:
            raise UdsError(f"Malformed RoutineControl response for 0x{routine_id:04X}")
        return response

    def request_download(self, address: int, size: int) -> int:
        request = bytes((SID_REQUEST_DOWNLOAD, 0x00, 0x44)) + address.to_bytes(4, "big") + size.to_bytes(4, "big")
        response = self._exchange(request, SID_REQUEST_DOWNLOAD)
        if len(response) < 3:
            raise UdsError("Malformed RequestDownload response")
        length_bytes = (response[1] >> 4) & 0x0F
        if length_bytes == 0 or len(response) != 2 + length_bytes:
            raise UdsError("Invalid maxNumberOfBlockLength format")
        return int.from_bytes(response[2:], "big")

    def transfer_data(self, sequence: int, data: bytes) -> bytes:
        response = self._exchange(bytes((SID_TRANSFER_DATA, sequence)) + data,
                                  SID_TRANSFER_DATA)
        if len(response) != 2 or response[1] != sequence:
            raise UdsError("TransferData block counter echo mismatch")
        return response

    def request_transfer_exit(self) -> bytes:
        response = self._exchange(bytes((SID_REQUEST_TRANSFER_EXIT,)),
                                  SID_REQUEST_TRANSFER_EXIT)
        if len(response) != 1:
            raise UdsError("Malformed RequestTransferExit response")
        return response

    def ecu_reset(self) -> bytes:
        response = self._exchange(bytes((SID_ECU_RESET, 0x01)), SID_ECU_RESET)
        if response != bytes((0x51, 0x01)):
            raise UdsError("Malformed ECUReset response")
        return response

    def read_did(self, did: int) -> bytes:#读取 VIN
        request = bytes((
            SID_READ_DATA_BY_IDENTIFIER,
            (did >> 8) & 0xFF,
            did & 0xFF,
        ))
        response = self._exchange(request, SID_READ_DATA_BY_IDENTIFIER)
        if len(response) < 3 or int.from_bytes(response[1:3], "big") != did:
            raise UdsError(f"DID echo mismatch for 0x{did:04X}")
        return response[3:]

    def read_dtc_by_status_mask(self, status_mask: int = 0xFF) -> DtcReport:#读取 DTC
        if not 0 <= status_mask <= 0xFF:
            raise ValueError("DTC status mask must be in range 0..255")
        response = self._exchange(
            bytes((SID_READ_DTC_INFORMATION, 0x02, status_mask)),
            SID_READ_DTC_INFORMATION,
        )
        if len(response) < 3 or response[1] != 0x02:
            raise UdsError("Malformed ReadDTCInformation response")
        if (len(response) - 3) % 4 != 0:
            raise UdsError("ReadDTCInformation record length is not divisible by four")

        records = []
        for offset in range(3, len(response), 4):
            records.append(
                DtcRecord(
                    int.from_bytes(response[offset : offset + 3], "big"),
                    response[offset + 3],
                )
            )
        return DtcReport(response[2], records, response)

    def clear_all_dtc(self) -> bytes:#清除全部 DTC
        response = self._exchange(
            bytes((SID_CLEAR_DIAGNOSTIC_INFORMATION, 0xFF, 0xFF, 0xFF)),
            SID_CLEAR_DIAGNOSTIC_INFORMATION,
        )
        if len(response) != 1:
            raise UdsError("Malformed ClearDiagnosticInformation response")
        return response


    def _exchange(self, request: bytes, request_sid: int) -> bytes:#核心公共函数
        response = self._transport.request(request)
        deadline = time.monotonic() + 30.0
        pending_count = 0
        while response == bytes((0x7F, request_sid, 0x78)):
            pending_count += 1
            remaining = deadline - time.monotonic()
            if pending_count > 120 or remaining <= 0:
                raise UdsError("ResponsePending exceeded bounded operation deadline")
            response = self._transport.receive_response(min(self._p2_star_seconds, remaining))
        if len(response) == 0:
            raise UdsError("Received an empty UDS response")
        if response[0] == 0x7F:
            if len(response) != 3:
                raise UdsError("Malformed UDS negative response")
            if response[1] != request_sid:
                raise UdsError(
                    f"Negative response SID echo mismatch: 0x{response[1]:02X}"
                )
            raise UdsNegativeResponseError(response[1], response[2])

        expected_sid = (request_sid + POSITIVE_RESPONSE_OFFSET) & 0xFF
        if response[0] != expected_sid:
            raise UdsError(
                f"Expected positive SID 0x{expected_sid:02X}, "
                f"received 0x{response[0]:02X}"
            )
        return response


def decode_did_value(name: str, data: bytes) -> str:#这个函数把原始字节转换成可读文本
    if name in ("vin", "sw"):
        try:
            return data.decode("ascii")
        except UnicodeDecodeError as error:
            raise UdsError(f"DID '{name}' is not valid ASCII") from error
    if name in ("speed", "rpm"):
        if len(data) != 2:
            raise UdsError(f"DID '{name}' expected two data bytes")
        value = int.from_bytes(data, "little")
        return f"{value} {'km/h' if name == 'speed' else 'rpm'}"
    if name in ("coolant", "gear"):
        if len(data) != 1:
            raise UdsError(f"DID '{name}' expected one data byte")
        return f"{data[0]} {'C' if name == 'coolant' else ''}".rstrip()
    raise ValueError(f"Unknown DID name: {name}")
