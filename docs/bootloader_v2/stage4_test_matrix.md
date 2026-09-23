# 第四阶段验收入口与状态

第三阶段：用户已确认板上通过，包括A2→B3→A4→B5、44%物理断电后旧APP恢复。
第四阶段：编译与主机测试通过；下表实物项目尚未执行。不是“任意掉电绝对安全”认证。

## 先做首次安装（当前A4保留、B5运行）

1. 备份当前可恢复的A4/B5镜像、Bootloader及下载配置。确认PCAN连接到Ubuntu，can0仍为500000。
2. S32DS选 `Application_Build/Stage4/Release/Bootloader.elf`。这是仅Bootloader，不是Combined ELF。
   确认下载器采用局部扇区擦除，不擦A/B、0x78000/0x79000以外保留区。若无法确认，先停下检查配置。
   不勾Emergency full chip erase；不烧PBL_FlexNVM。此步骤本次没有代你执行。
3. Terminate调试并复位。第一次两份Journal空/无效留Bootloader是预期行为，暂时没有APP周期帧。
4. 把本次整个linux_gateway更新到Ubuntu同名目录，避免只有脚本没有新gateway模块。在Ubuntu执行：

```bash
cd ~/s32k/linux_gateway
python3 uds_journal
```

预期Journal INVALID，不是超时。若超时先检查PCAN/CAN，不要因为超时全片擦除。

5. 仅在确认旧B仍是完整且已验收5.0.0时，执行显式初始化：

```bash
python3 uds_journal --initialize B
```

预期VALID、ABORTED、LastResult=COMMISSIONED、Active B、Target A、txn=0。
这里ABORTED表示目标未被授权，不表示擦坏APP。仅写Journal，不擦A/B。
已有有效Journal会拒绝重复初始化；不要每次升级都带此参数。

6. 手动复位板子，验证旧B5恢复：有0x100/101/102周期帧，F101=B，F102=5.0.0。
   `main.py uds read`已有可读DID名字用`--help`确认；F100旧字符串不作为packed版本验收。
   若要只读F101/F102，可在另一个没有flasher运行的终端执行：

```bash
python3 -c "from gateway.can_bus import SocketCanBus; from gateway.isotp import IsoTpClient; from gateway.uds import UdsClient; b=SocketCanBus('can0'); c=UdsClient(IsoTpClient(b,0x7E0,0x7E8)); print('slot',c.read_did(0xF101).hex()); print('version',c.read_did(0xF102).hex()); b.close()"
```

预期slot `01`，version `05000000`。不要把从Linux回显的7E0当成ECU应答。

## 正常升级

将 `Application_Build/Stage4/APP_A/app_slot_a_image.bin` 复制到Ubuntu：

```bash
python3 uds_flasher app_slot_a_image.bin
```

预期先显示Journal状态，再刷A6；最终Slot A/version6.0.0，冷上电仍启动A6。
再用 `Application_Build/Stage4/APP_B/app_slot_b_image.bin` 刷B7，验证反方向及冷上电。
两份都是229376字节packed image，不能用裸APP_A.bin/APP_B.bin代替。

F104只在Bootloader提供。查看持久化结果时，可以通过现有App的10 02进入Bootloader，
等待约1秒再运行uds_journal；此操作会暂时停止APP业务：

```bash
cansend can0 7E0#0210020000000000
python3 uds_journal
```

两条命令之间人工等待板子进入Bootloader。读取后手动Reset回APP；读状态不会自行擦除。
正常结果PENDING_ACTIVATION/SUCCESS；事务Active为上一次升级前的旧槽，F101才是当前身份。

## 主机结果 / 待做板级矩阵

| 项目 | 主机测试 | Stage4板级预期/状态 |
|---|---|---|
| A→B、B→A完整升级 | 生产C+Linux fake ECU通过 | 待测；新槽正确版本，周期帧恢复 |
| PREPARING后Reset | 通过 | 待测；Abort、旧槽启动 |
| ERASING期间Reset | 扇区间注入通过 | 待测；Target禁启，旧槽启动 |
| 擦除完成/34开始Reset | 通过 | 待测；Abort、完整重刷 |
| Programming 10%/44%/90% Reset | 生产C注入通过 | 待测；LastResult中断，旧槽运行 |
| 37之后Reset | 通过，恢复缓存Header | 待测；重新验证→Pending或失败Abort |
| Verify期间Reset | 校验入口注入通过 | 待测；重新验证 |
| Verify完成后Reset | VERIFIED后注入通过 | 待测；重新验证→Pending |
| Copy0坏 / Copy1坏 | 通过 | 待测；读取另一有效副本 |
| 最新记录写一半 | 128种字节截断、17个Commit边界通过 | 待测；在可读取前提下选旧有效记录 |
| 两份Journal都坏/空 | 通过 | 待测；Programming Mode，不猜高版本 |
| 错误Target镜像 | CRC/Header错误回归通过 | 待测；不发布/不启动，旧槽保留 |
| Journal擦写/回读失败 | 通过，停止后续Slot写入 | 待测；72、StorageFault，不继续假定成功 |
| Active全部Flash保持 | 主机全槽CRC比较通过 | 每个板级异常测试前后读取/比较旧镜像 |
| 非法状态转换 | 100种组合通过 | 不需要手工遍历所有，但关键UDS越序需板测 |
| Checkpoint/wear | 完整升级14提交通过 | 核对sequence增量；不得逐0x36递增 |
| ISO-TP F104多帧 | 生产发送11项+Python通过 | 待测；35字节UDS响应完整，CTS STmin=0 |
| Release/Fault开关 | ELF符号检查通过 | Release不得有可激活注入变量 |
| 最大payload、P2/WDOG/栈 | 布局静态检查通过 | 220KiB边界与实际时序/栈高水位待测 |

## 调试版复位注入

仅测试时换 `Stage4/FaultInjection/Bootloader.elf`，保持Journal/A/B不被下载器擦除。
S32DS设置 `BootFault_ArmedPoint=1..11`，`BootFault_SkipHits=0`，Resume再开始刷写。
点号对应 [Journal说明](stage4_update_journal.md)；11可用SkipHits逐个测试erase/phrase之后。
复位后开关自动清零。观察BootUpdate_Current、BootUpdate_JournalValid、BootUpdate_StorageFault；
Memory以32-bit Hex看0x78000/0x79000：state偏移0x18，committed偏移0x24，CRC偏移0x78。
恢复值以断点停在Recovery完成之后为准，不要将恢复前的临时RAM当最终结果。

Journal损坏工具当前是主机生产C测试夹具：其内存后端支持Copy0/Copy1/both、
CRC翻转、每phrase各前缀截断、sequence冲突、回读错误。运行build_stage4.ps1会全部执行。
没有提供可远程任意破坏板子Flash的UDS命令；板上损坏测试需专门可恢复夹具，先备份、明确范围，
不能在不知道当前有效副本的情况下直接修改Flash地址。

## 真正断电

软件Reset仅验证状态机。还需在上述阶段切断板子实际供电，确认不是仅拔PCAN；
注意USB调试口可能仍给板子供电。重新上电后检查恢复状态、F101/F102、旧槽CRC、can0统计。
Flash擦写脉冲中断、电压下降波形、不同温度/重复次数、ECC读Fault需要实际设备验证。
如果出现HardFault/总线故障，保留PC/CFSR/BFAR及两个Journal读取结果，停止验收并定位，
不要将“没收到CAN”直接判定为固件/Journal失败。

正式记录每次：旧槽版本/CRC、目标版本、注入点、是否物理掉电、两份sequence/state/CRC、
启动槽、F104 last_result、CAN返回。没有实际测量的行保持待测。
