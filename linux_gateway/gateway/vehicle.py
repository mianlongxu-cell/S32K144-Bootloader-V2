"""Vehicle signal decoding, state storage, and 0x200 command encoding."""#解码 ECU 周期发送的 0x100/0x101/0x102,生成发给 ECU 的 0x200 控制帧

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime
from typing import Dict, Optional

from .can_bus import CanFrame
from .config import (
    CAN_ID_BODY,
    CAN_ID_CONTROL,
    CAN_ID_FAULT,
    CAN_ID_POWERTRAIN,
    COMMANDS,
)


@dataclass
class PowertrainState:
    speed_kmh: int = 0
    rpm: int = 0
    coolant_c: int = 0
    gear: int = 0
    status: int = 0


@dataclass
class BodyState:
    light: int = 0
    door: int = 0
    wiper: int = 0
    turn_signal: int = 0
    lock: int = 0


@dataclass
class FaultState:
    active: int = 0
    dtc: int = 0
    level: int = 0
    occurrence: int = 0
    status: int = 0


@dataclass
class VehicleState:
    powertrain: PowertrainState = field(default_factory=PowertrainState)
    body: BodyState = field(default_factory=BodyState)
    fault: FaultState = field(default_factory=FaultState)
    last_update: Optional[datetime] = None

    def update(self, frame: CanFrame) -> bool:#根据 CAN ID 解码
        """Decode one configured vehicle frame; return True when recognized."""
        if frame.can_id == CAN_ID_POWERTRAIN:
            self._decode_powertrain(frame.data)
        elif frame.can_id == CAN_ID_BODY:
            self._decode_body(frame.data)
        elif frame.can_id == CAN_ID_FAULT:
            self._decode_fault(frame.data)
        else:
            return False

        self.last_update = datetime.now()
        return True

    def _decode_powertrain(self, data: bytes) -> None:
        _require_length(data, 7, CAN_ID_POWERTRAIN)
        self.powertrain.speed_kmh = int.from_bytes(data[0:2], "little")
        self.powertrain.rpm = int.from_bytes(data[2:4], "little")
        self.powertrain.coolant_c = data[4]
        self.powertrain.gear = data[5]
        self.powertrain.status = data[6]

    def _decode_body(self, data: bytes) -> None:
        _require_length(data, 5, CAN_ID_BODY)
        self.body.light = data[0]
        self.body.door = data[1]
        self.body.wiper = data[2]
        self.body.turn_signal = data[3]
        self.body.lock = data[4]

    def _decode_fault(self, data: bytes) -> None:
        _require_length(data, 6, CAN_ID_FAULT)
        self.fault.active = data[0]
        # The legacy 0x102 frame contains the low 16 bits of the 24-bit DTC.
        self.fault.dtc = int.from_bytes(data[1:3], "little")
        self.fault.level = data[3]
        self.fault.occurrence = data[4]
        self.fault.status = data[5]

    def csv_row(self, timestamp: Optional[datetime] = None) -> Dict[str, object]:#车辆状态转换成 Python 字典
        current_time = timestamp or self.last_update or datetime.now()
        return {
            "timestamp": current_time.isoformat(timespec="milliseconds"),
            "speed_kmh": self.powertrain.speed_kmh,
            "rpm": self.powertrain.rpm,
            "coolant_c": self.powertrain.coolant_c,
            "gear": self.powertrain.gear,
            "powertrain_status": self.powertrain.status,
            "light": self.body.light,
            "door": self.body.door,
            "wiper": self.body.wiper,
            "turn_signal": self.body.turn_signal,
            "lock": self.body.lock,
            "fault_active": self.fault.active,
            "dtc": f"{self.fault.dtc:06X}",
            "fault_level": self.fault.level,
            "occurrence": self.fault.occurrence,
            "dtc_status": f"{self.fault.status:02X}",
        }


def build_control_frame(command_name: str, argument: int = 0) -> CanFrame:#生成 0x200 控制帧
    if command_name not in COMMANDS:
        raise ValueError(f"Unknown control command: {command_name}")
    if not 0 <= argument <= 0xFF:
        raise ValueError("Control command argument must be in range 0..255")
    if command_name == "turn" and argument > 3:
        raise ValueError("Turn signal value must be 0 (off), 1, 2, or 3")
    if command_name != "turn" and argument != 0:
        raise ValueError(f"Command '{command_name}' does not take an argument")

    payload = bytes((COMMANDS[command_name], argument)) + bytes(6)
    return CanFrame(CAN_ID_CONTROL, payload)


def format_state(state: VehicleState) -> str:
    fault_text = (
        f"YES / {state.fault.dtc:06X} / status {state.fault.status:02X}"
        if state.fault.active
        else "NO"
    )
    return (
        "Vehicle ECU | "
        f"speed={state.powertrain.speed_kmh:3d} km/h  "
        f"rpm={state.powertrain.rpm:4d}  "
        f"coolant={state.powertrain.coolant_c:3d} C  "
        f"gear={state.powertrain.gear} | "
        f"light={state.body.light} door={state.body.door} "
        f"wiper={state.body.wiper} turn={state.body.turn_signal} "
        f"lock={state.body.lock} | fault={fault_text}"
    )


def _require_length(data: bytes, minimum: int, can_id: int) -> None:
    if len(data) < minimum:
        raise ValueError(
            f"CAN 0x{can_id:03X} requires at least {minimum} bytes, "
            f"received {len(data)}"
        )
