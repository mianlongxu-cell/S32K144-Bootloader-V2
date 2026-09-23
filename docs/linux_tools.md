# Linux SocketCAN tools

## Prerequisites

```bash
sudo apt update
sudo apt install -y python3 can-utils
cd linux_gateway
chmod +x uds_flasher scripts/*.sh
```

If Linux runs in a VM, attach the USB-CAN adapter to the guest. After reconnect
or VM restart, verify that `can0` still exists before diagnosing the ECU.

## Configure CAN

```bash
./scripts/setup_can.sh can0 500000
ip -details -statistics link show can0
```

Expected essentials are `state UP`, `bitrate 500000`, and zero bus-off count.

Manual equivalent:

```bash
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 500000 restart-ms 100
sudo ip link set can0 txqueuelen 1000
sudo ip link set can0 up
```

## Raw CAN observation

```bash
candump -tz can0
```

The validated periodic messages are `0x100`, `0x101`, and `0x102`, with nominal
periods of 100, 200, and 500 ms.

## Vehicle monitor and CSV

```bash
python3 main.py monitor
python3 main.py monitor --csv logs/vehicle.csv
python3 main.py monitor --print-interval 0.5
```

## Control commands

```bash
python3 main.py control
python3 main.py start
python3 main.py stop
python3 main.py reset-speed
python3 main.py overtemp
python3 main.py clear-fault
python3 main.py light
python3 main.py door
python3 main.py wiper
python3 main.py lock
python3 main.py turn 1
```

`control` sends start and a heartbeat every second; Ctrl+C sends stop. The ECU
raises control-timeout DTC `0x000301` only after monitoring has been enabled and
five consecutive seconds pass without heartbeat.

## UDS tester

```bash
python3 main.py uds session extended
python3 main.py uds tester-present
python3 main.py uds read vin
python3 main.py uds read sw
python3 main.py uds read speed
python3 main.py uds read rpm
python3 main.py uds read coolant
python3 main.py uds read gear
python3 main.py uds dtc read
python3 main.py uds dtc clear
```

Use another interface or a longer diagnostic timeout as follows:

```bash
python3 main.py --channel can1 uds read sw
python3 main.py uds --timeout 2.0 read vin
```

## Firmware flasher

```bash
./uds_flasher ../firmware/app_v2.bin
./scripts/flash_v2.sh ../firmware/app_v2.bin can0
```

The raw binary must be an Application image linked for `0x00008000`; do not pass
the Bootloader or combined initial-programming ELF.

## Unit tests

```bash
cd linux_gateway
python3 -m unittest discover -s tests -v
```

## Common errors

| Error | Check |
|---|---|
| `No such device` | attach USB-CAN to Linux/VM and run `ip -br link` |
| `Network is down` | rerun `scripts/setup_can.sh` |
| timeout waiting for `0x7E8` | ECU power, CAN_H/CAN_L/GND, bitrate, active Application/Bootloader |
| only transmitted `0x7E0` visible | ECU did not respond; check physical link and target state |
| no periodic frames after VM restart | reconnect the USB-CAN device to the guest |
| `NRC 0x33` | programming erase/download attempted before SecurityAccess |
| `NRC 0x73` | TransferData block sequence mismatch |
