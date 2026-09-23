# S32K144 Linux Gateway

Stage5在Bootloader Programming Mode提供生命周期诊断：

```bash
chmod +x uds_boot_status
./uds_boot_status --channel can0
```

命令读取F105并显示A/B状态、版本、Attempt、Active、Reset reason和Rollback结果。
F103仍是Inactive Target，F104仍是Update Journal，没有改变旧DID语义。

本目录提供完整的Linux SocketCAN网关、车辆状态监控、UDS诊断和控制功能：

- 通过Linux SocketCAN接收和发送Classic CAN标准帧。
- 解析S32K144发出的0x100、0x101和0x102。
- 保存并显示最新车辆状态。
- 用Python命令替代手工 `cansend`，向0x200发送控制命令。
- 将解析后的车辆状态持续写入CSV。

- ISO-TP短请求发送和SF响应接收。
- ISO-TP FF/FC/CF多帧响应接收、SN校验和超时。
- Linux UDS Tester：0x10、0x3E、0x22、0x19、0x14。
- DEM 24位DTC和testFailed/pendingDTC/confirmedDTC状态解析。

控制链路包含心跳监督。为避免控制超时DTC误触发，S32K144固件只有
收到`start`后才监控心跳，
收到`stop`后停止监控，连续5秒无心跳才触发`0x000301`。

## 1. Ubuntu准备

```bash
sudo apt update
sudo apt install -y python3 python3-venv can-utils
cd linux_gateway
chmod +x scripts/setup_can.sh
./scripts/setup_can.sh can0 500000
```

检查结果中应包含：

```text
state UP
can state ERROR-ACTIVE
bitrate 500000
```

## 2. 监控车辆状态

```bash
python3 main.py monitor
```

默认每秒打印一次合并后的车辆状态。按 `Ctrl+C` 停止。

同时记录CSV：

```bash
python3 main.py monitor --csv logs/vehicle.csv
```

调整终端打印周期：

```bash
python3 main.py monitor --print-interval 0.5
```

## 3. 发送0x200控制命令

推荐先在独立终端启动持续控制会话：

```bash
python3 main.py control
```

程序先发送`start`，随后每1秒发送一次独立的`heartbeat`。按`Ctrl+C`
时程序发送`stop`并退出。另开终端执行下面的一次性控制命令：

```bash
python3 main.py start
python3 main.py stop
python3 main.py reset-speed
python3 main.py overtemp
python3 main.py clear-fault
python3 main.py light
python3 main.py door
python3 main.py wiper
python3 main.py lock
python3 main.py turn 0
python3 main.py turn 1
python3 main.py turn 2
python3 main.py turn 3
```

也可手动发送一帧心跳用于调试：

```bash
python3 main.py heartbeat
```

心跳命令为`0x200: 0B 00 00 00 00 00 00 00`。`overtemp`、`light`等
一次性命令不会刷新ECU的心跳计时器。

使用其他SocketCAN接口时，把全局选项放在子命令前：

```bash
python3 main.py --channel can1 monitor
python3 main.py --channel can1 overtemp
```

## 4. 运行单元测试

单元测试不访问CAN硬件，在Windows或Ubuntu均可运行：

```bash
python3 -m unittest discover -v
```

## 5. 实机验收

1. `monitor` 正确显示0x100中的车速、RPM、水温和挡位。
2. `light`、`door`、`wiper`、`lock`、`turn`能改变0x101状态。
3. `stop`停止0x100/0x101/0x102，`start`恢复发送。
4. `reset-speed`使车速归零。
5. `overtemp`使水温变为110，约300 ms后0x102出现0x000101低16位。
6. `clear-fault`恢复故障条件并清除DTC。
7. CSV持续新增记录，字段值与终端显示一致。

控制超时逻辑：ECU上电默认不监控；`start`启用；每个`heartbeat`刷新计时；
连续5秒无心跳触发`0x000301`；`stop`关闭监控。超时DTC确认后即使链路恢复，
历史记录仍保留，需使用`clear-fault`或UDS 0x14清除。

## 6. Linux UDS Tester

进入扩展会话：

```bash
python3 main.py uds session extended
```

TesterPresent：

```bash
python3 main.py uds tester-present
```

读取配置的DID：

```bash
python3 main.py uds read vin
python3 main.py uds read sw
python3 main.py uds read speed
python3 main.py uds read rpm
python3 main.py uds read coolant
python3 main.py uds read gear
```

VIN响应超过单帧容量，程序会自动完成：

```text
ECU First Frame
→ Linux Flow Control 30 00 00
→ ECU Consecutive Frame 21
→ ECU Consecutive Frame 22
→ 组装完整VIN
```

读取DEM中的DTC：

```bash
python3 main.py uds dtc read
```

指定状态掩码：

```bash
python3 main.py uds dtc read --mask 0xFF
```

清除全部DTC：

```bash
python3 main.py uds dtc clear
```

默认ISO-TP超时为1秒。需要临时增大时，全局UDS参数应放在UDS子命令后：

```bash
python3 main.py uds --timeout 2.0 read vin
```

## 7. DEM联调验收

在第一个终端保持控制会话运行：

```bash
python3 main.py control
```

制造高温并等待至少350 ms：

```bash
python3 main.py overtemp
sleep 0.4
python3 main.py uds dtc read
```

预期包含：

```text
DTC 0x000101  status=0x0D (testFailed, pendingDTC, confirmedDTC)
```

恢复故障条件并清除DTC：

```bash
python3 main.py clear-fault
python3 main.py uds dtc read
```

预期：

```text
DTC list: empty
```

也可以单独通过UDS 0x14清除：

```bash
python3 main.py uds dtc clear
```

如果水温仍保持110，清除后经过三次故障监测，0x000101会再次确认，这是DEM的预期行为。

## 8. UDS与ISO-TP验收

1. `uds session extended`返回0x50 03和P2/P2*。
2. `uds tester-present`返回0x7E 00。
3. 六个DID全部能够读取并正确解释。
4. VIN多帧自动完成FF/FC/CF，输出 `TESTS32K144000001`。
5. `uds dtc read`能够解析完整24位DTC及状态位。
6. `uds dtc clear`返回0x54。
7. ISO-TP SN错误或响应超时时，程序明确报错而不是无限等待。
8. 同时运行 `monitor` 时，UDS测试不影响0x100/0x101/0x102周期报文。

## 9. 系统集成验收

建议打开三个终端：

```bash
# 终端1：周期心跳，Ctrl+C时自动发送stop
python3 main.py control

# 终端2：车辆状态和CSV
python3 main.py monitor --csv logs/vehicle.csv

# 终端3：控制和诊断
python3 main.py overtemp
python3 main.py uds dtc read
```

验收以下项目：

1. ECU刚上电且从未收到`start`时，等待超过5秒也没有`0x000301`。
2. `control`运行超过5秒时不会出现`0x000301`。
3. 强制结束`control`而不发送`stop`（例如关闭终端），约5秒后出现`0x000301`。
4. 正常按`Ctrl+C`退出`control`后，等待超过5秒也不会出现新的控制超时。
5. 控制会话运行期间，车辆解析、CSV、VIN多帧、DTC读取及清除仍正常。
6. 执行`overtemp`、`light`等一次性命令不会刷新心跳计时。

完成上述项目后，Linux网关与诊断控制功能即可验收完成。
