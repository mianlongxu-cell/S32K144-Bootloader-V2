#!/usr/bin/env python3
"""Command-line Linux gateway for the S32K144 Vehicle ECU."""

from __future__ import annotations

import argparse
from contextlib import nullcontext
import sys
import time

from gateway.can_bus import SocketCanBus
from gateway.config import (
    CAN_CHANNEL,
    CAN_ID_UDS_REQUEST,
    CAN_ID_UDS_RESPONSE,
    COMMANDS,
)
from gateway.csv_logger import CsvStateLogger
from gateway.isotp import IsoTpClient
from gateway.uds import DID_NAMES, UdsClient, decode_did_value
from gateway.vehicle import VehicleState, build_control_frame, format_state


def build_parser() -> argparse.ArgumentParser: #负责解析命令行参数
    parser = argparse.ArgumentParser(
        description="S32K144 Vehicle ECU SocketCAN gateway"
    )
    parser.add_argument(
        "--channel", default=CAN_CHANNEL, help="SocketCAN channel (default: can0)"
    )#指定使用哪个SocketCAN接口
    subparsers = parser.add_subparsers(dest="operation", required=True)

    monitor_parser = subparsers.add_parser(
        "monitor", help="decode 0x100/0x101/0x102 continuously"
    )
    monitor_parser.add_argument("--csv", help="append decoded snapshots to a CSV file")
    monitor_parser.add_argument(
        "--print-interval",
        type=float,
        default=1.0,
        help="seconds between terminal state lines (default: 1.0)",
    )

    control_parser = subparsers.add_parser(
        "control", help="keep a remote-control session alive until Ctrl+C"
    )
    control_parser.add_argument(
        "--heartbeat-interval",
        type=float,
        default=1.0,
        help="seconds between 0x200 heartbeat frames (default: 1.0)",
    )

    for command in COMMANDS:
        command_parser = subparsers.add_parser(
            command, help=f"send the 0x200 '{command}' command"
        )
        if command == "turn":
            command_parser.add_argument(
                "value", type=int, choices=range(4), help="0=off, values 1..3"
            )

    uds_parser = subparsers.add_parser("uds", help="run the Linux UDS tester")
    uds_parser.add_argument(
        "--timeout", type=float, default=1.0, help="ISO-TP timeout in seconds"
    )
    uds_subparsers = uds_parser.add_subparsers(dest="uds_operation", required=True)

    session_parser = uds_subparsers.add_parser(
        "session", help="change diagnostic session"
    )
    session_parser.add_argument("mode", choices=("default", "extended"))
    uds_subparsers.add_parser("tester-present", help="send UDS 0x3E 00")

    read_parser = uds_subparsers.add_parser("read", help="read a configured DID")
    read_parser.add_argument("name", choices=tuple(DID_NAMES))

    dtc_parser = uds_subparsers.add_parser("dtc", help="read or clear DEM DTCs")
    dtc_subparsers = dtc_parser.add_subparsers(dest="dtc_operation", required=True)
    dtc_read_parser = dtc_subparsers.add_parser("read", help="send UDS 0x19 02")
    dtc_read_parser.add_argument(
        "--mask",
        type=lambda value: int(value, 0),
        default=0xFF,
        help="DTC status mask, for example 0xFF",
    )
    dtc_subparsers.add_parser("clear", help="send UDS 0x14 FF FF FF")

    return parser


def monitor(channel: str, csv_path: str | None, print_interval: float) -> int:
    if print_interval <= 0:
        raise ValueError("--print-interval must be greater than zero")

    state = VehicleState()
    logger_context = CsvStateLogger(csv_path) if csv_path else nullcontext(None)
    next_print = time.monotonic()

    with SocketCanBus(channel) as bus, logger_context as logger:
        print(f"Monitoring {channel}; press Ctrl+C to stop")
        try:
            while True:
                frame = bus.receive(timeout=0.5)
                if frame is None or not state.update(frame):
                    continue
                if logger is not None:
                    logger.write(state)

                now = time.monotonic()
                if now >= next_print:
                    print(format_state(state), flush=True)
                    next_print = now + print_interval
        except KeyboardInterrupt:
            print("\nMonitor stopped")
    return 0


def send_control(channel: str, command: str, argument: int = 0) -> int:#发送 0x200
    frame = build_control_frame(command, argument)
    with SocketCanBus(channel) as bus:#打开 CAN，发送一帧，退出后自动关闭
        bus.send(frame)
    print(
        f"Sent {channel} 0x{frame.can_id:03X}#"
        f"{frame.data.hex().upper()} ({command})"
    )
    return 0


def run_control_session(channel: str, heartbeat_interval: float) -> int:#控制会话
    if heartbeat_interval <= 0 or heartbeat_interval >= 5.0:
        raise ValueError("--heartbeat-interval must be greater than 0 and less than 5")

    start_frame = build_control_frame("start")
    heartbeat_frame = build_control_frame("heartbeat")
    stop_frame = build_control_frame("stop")

    with SocketCanBus(channel) as bus:
        bus.send(start_frame)
        print(
            f"Control session active on {channel}; heartbeat every "
            f"{heartbeat_interval:g} s; press Ctrl+C to stop",
            flush=True,
        )
        try:
            while True:
                time.sleep(heartbeat_interval)
                bus.send(heartbeat_frame)
        except KeyboardInterrupt:
            bus.send(stop_frame)
            print("\nControl session stopped; stop command sent")
    return 0


def run_uds(channel: str, args: argparse.Namespace) -> int:#运行 UDS 测试器
    with SocketCanBus(channel) as bus:
        transport = IsoTpClient(
            bus,
            CAN_ID_UDS_REQUEST,
            CAN_ID_UDS_RESPONSE,
            response_timeout=args.timeout,
            consecutive_frame_timeout=args.timeout,
        )#创建 ISO-TP 客户端
        client = UdsClient(transport)#创建 UDS 客户端

        if args.uds_operation == "session":
            session = 0x03 if args.mode == "extended" else 0x01
            result = client.diagnostic_session(session)
            print(f"UDS response: {_hex(result.raw_response)}")
            print(
                f"Session=0x{result.session:02X}, "
                f"P2={result.p2_server_max_ms} ms, "
                f"P2*={result.p2_star_server_max_ms} ms"
            )
            return 0

        if args.uds_operation == "tester-present":
            response = client.tester_present()
            print(f"UDS response: {_hex(response)}")
            return 0

        if args.uds_operation == "read":
            did = DID_NAMES[args.name]
            data = client.read_did(did)
            print(f"DID 0x{did:04X}: {decode_did_value(args.name, data)}")
            print(f"Data: {_hex(data)}")
            return 0

        if args.dtc_operation == "read":
            report = client.read_dtc_by_status_mask(args.mask)
            print(
                f"DTC status availability: "
                f"0x{report.status_availability_mask:02X}"
            )
            if not report.records:
                print("DTC list: empty")
            for record in report.records:
                names = ", ".join(record.status_names) or "none"
                print(
                    f"DTC 0x{record.code:06X}  status=0x{record.status:02X} "
                    f"({names})"
                )
            return 0

        response = client.clear_all_dtc()
        print(f"UDS response: {_hex(response)}")
        print("All DEM DTC records cleared")
        return 0


def _hex(data: bytes) -> str:
    return " ".join(f"{value:02X}" for value in data)


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.operation == "monitor":
            return monitor(args.channel, args.csv, args.print_interval)
        if args.operation == "control":
            return run_control_session(args.channel, args.heartbeat_interval)
        if args.operation == "uds":
            return run_uds(args.channel, args)
        argument = args.value if args.operation == "turn" else 0
        return send_control(args.channel, args.operation, argument)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
