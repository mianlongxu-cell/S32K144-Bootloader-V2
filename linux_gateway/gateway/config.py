"""Static configuration shared by the Linux gateway modules."""

CAN_CHANNEL = "can0"
CAN_BITRATE = 500_000

CAN_ID_POWERTRAIN = 0x100
CAN_ID_BODY = 0x101
CAN_ID_FAULT = 0x102
CAN_ID_CONTROL = 0x200
CAN_ID_UDS_REQUEST = 0x7E0
CAN_ID_UDS_RESPONSE = 0x7E8

VEHICLE_RX_IDS = frozenset(
    (CAN_ID_POWERTRAIN, CAN_ID_BODY, CAN_ID_FAULT)
)

COMMANDS = {
    "start": 0x01,
    "stop": 0x02,
    "reset-speed": 0x03,
    "overtemp": 0x04,
    "clear-fault": 0x05,
    "light": 0x06,
    "door": 0x07,
    "wiper": 0x08,
    "lock": 0x09,
    "turn": 0x0A,
    "heartbeat": 0x0B,
}
