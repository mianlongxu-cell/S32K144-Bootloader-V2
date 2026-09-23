# 第四阶段：Transactional Update Journal

状态：代码、编译、主机测试和板上掉电验收均完成。10%/44%/90%传输断电以及
Journal擦写中断均由用户实物确认通过。
阶段4验收未修改原项目。Trial/Confirm/Rollback由后续Stage5独立Metadata实现；仍无续传或Secure Boot。

## 1. 现状分析与设计选择

阶段3已具备A/B校验、版本选择、Active保护、RAM Driver与在线UDS刷写；主要缺口是
事务只在RAM中，复位后无法分辨擦除/传输/验证阶段。Header也只缓存RAM，到FF01才写Flash。
因此Journal额外保存完整64字节Header，否则37之后断电无法恢复完整性验证。
Image Header的64字节ABI、CRC与用途不变，升级状态不写入Image Header。

原预留PFlash为0x78000..0x7FFFF，旧Metadata在0x7F000。取前两个独立扇区，
不移动Slot、不占用FlexNVM、不扩大APP下载范围。没有使用原占位BootSlotMetadata作事务记录。

## 2. 模块及文件

新增生产模块（均为Bootloader/inc同名.h与src同名.c，runtime除外）：

- boot_journal：记录校验、双副本选择、CRC、Commit/Clear。
- boot_journal_storage：Flash读取；写入口由boot_flash转到RAM Driver。
- boot_update：事务状态、进度、Abort、启动许可、恢复协调。
- boot_recovery_policy：纯恢复动作与槽许可规则。
- boot_image_install：恢复时重用BootImage校验并发布缓存Header。
- boot_fault：默认关闭的复位注入；boot_runtime.c提供无libc的memcpy/memset。

修改：boot_config.h、boot_flash.c、boot_manager.c、boot_jump.c、boot_programming.c、
boot_uds.c、boot_isotp.c、Bootloader/Makefile及两个linker、include/flash_driver_api.h、
FlashDriver/src/flash_driver.c。未修改APP源代码或A/B链接布局。

Linux新增gateway/update_status.py、uds_journal、tests/test_update_status.py；修改
gateway/flasher_v2.py、uds_flasher、tests/test_flasher_v2.py。
构建新增tools/build_stage4.ps1；测试新增stage4_journal_test.c、stage4_isotp_test.c；
更新stage3_programming_test.c、build_stage3.ps1、check_stage3_layout.py。
文档更新architecture/memory_map/boot_flow/flash_driver/README，新增本文、恢复流程、
验收矩阵及stage3_programming_flow。

## 3. Memory Map与完整Record定义

| 区域 | 地址（含结束） | 大小/用途 |
|---|---|---|
| Copy0 | 0x00078000..0x00078FFF | 4KiB独立扇区，前128B记录 |
| Copy1 | 0x00079000..0x00079FFF | 4KiB独立扇区，前128B记录 |
| 保留 | 0x0007A000..0x0007EFFF | 不写 |
| 旧Metadata | 0x0007F000..0x0007FFFF | 只读 |

擦除4KiB，编程8B phrase；RAM结构8B对齐、大小128B，有编译断言。
Flash固定小端u32，format_version=1。完整定义位于Bootloader/inc/boot_journal.h：

```c
typedef struct __attribute__((aligned(8))) {
    uint32_t magic, format_version, sequence, transaction_id;
    uint32_t active_slot, target_slot, state, target_version;
    uint32_t expected_size, committed_size, expected_crc, last_block_sequence;
    uint32_t last_result, flags;
    BootImageHeaderType cached_header;  /* 64 bytes, immutable image description */
    uint32_t journal_crc, commit_marker;
} BootUpdateJournal;
```

| 偏移 | 字段 |
|---|---|
| 0/4/8/12 | magic=0x4A345642 / format=1 / sequence / transaction_id |
| 16/20/24/28 | active_slot / target_slot / state / target_version |
| 32/36/40/44 | expected_size / committed_size / expected_crc / last_block_sequence |
| 48/52 | last_result / flags（bit0=HEADER_KNOWN，其余必须0） |
| 56..119 | cached_header原始64字节 |
| 120 | journal_crc：CRC32(bytes[0:120])，不含CRC/marker自身 |
| 124 | commit_marker=0x434F4D54，和CRC在最后一个phrase |

CRC32：反射多项式0xEDB88320、初始/最终异或0xFFFFFFFF；123456789→CBF43926。
slot A=0、B=1、UNKNOWN=0x7FFFFFFF。状态、槽关系、长度、flag、BSC以及完成态
缓存字段一致性也参与语义校验，不仅检查CRC。
0x34协议未传版本/期望CRC，因此到37保存Header时才填入target_version/expected_crc；
之前为0且HEADER_KNOWN=0，不把未知值描述成已校验。

## 4. 双副本与Commit

1. 读取并验证两份记录；只有一份有效用它；两份都有效用更新sequence。
2. `(a-b)!=0 && (a-b)<0x80000000`表示a新于b，支持FFFFFFFF→0。
3. 同sequence且字节相同可选Copy0；同sequence内容冲突或相差2^31视为歧义，fail-safe。
4. 构造新记录，sequence加1、更新CRC/marker；先校验将要写的语义。
5. 只擦除另一份扇区，绝不擦最新唯一有效副本。
6. 逐8字节写前120字节，最后写CRC+marker；回读128字节、校验且逐字节比较。
7. 全部通过才更新RAM状态并返回成功。

任一步失败返回false，UpdateManager锁住当前事务，禁止继续写Slot或假定状态已推进。
若新记录实际上完整写入但回读报错，当前会话仍停止；下次复位重新选择有效记录。
初始化在无有效Journal时通过显式FF02操作，首写Copy0。两份都无效（包括全FF）
进入Programming Mode，不凭高版本/保留RAM猜Active。若物理Flash读取异常导致CPU故障，
目前没有专门可恢复的ECC读取隔离层，见限制。

Clear只允许IDLE记录清诊断结果并产生新副本，不用于Abort，不擦除不完整事务证据。
Stage4本身正常完成保留PENDING_ACTIVATION。Stage5在目标Image再次验证且目标Metadata
成功提交PENDING之后，才将Journal提交为IDLE；此后启动生命周期只由Metadata负责。

## 5. 状态与转换表

| 值 | 状态 | 唯一正常下一状态 |
|---:|---|---|
| 0 | IDLE | PREPARING（新事务） |
| 1 | PREPARING | ERASING |
| 2 | ERASING | DOWNLOAD_READY |
| 3 | DOWNLOAD_READY | PROGRAMMING |
| 4 | PROGRAMMING | TRANSFER_COMPLETE |
| 5 | TRANSFER_COMPLETE | VERIFYING |
| 6 | VERIFYING | VERIFIED |
| 7 | VERIFIED | PENDING_ACTIVATION |
| 8 | PENDING_ACTIVATION | PREPARING（下次新事务） |
| 9 | ABORTED | PREPARING（重新完整刷写） |

状态1..9允许异常进入ABORTED并保留失败原因；IDLE→VERIFYING等跳步拒绝。
新事务ID加1，清旧进度和Header缓存；Abort保留txn、active/target、进度、版本/CRC。
PREPARING起保守屏蔽Target，所有实际擦除发生在ERASING持久化成功之后。
最终可启动条件是ImageValid AND UpdateStateAllowsBoot，BootManager和BootJump都检查。

LastResult枚举按值：0 NONE，1 SUCCESS，2 INTERRUPTED，3 INTERRUPTED_PROGRAMMING，
4 ERASE_FAILED，5 PROGRAM_FAILED，6 TRANSFER_FAILED，7 VERIFY_FAILED，8 BAD_IMAGE，
9 JOURNAL_ERROR，10 POWER_LOSS_RECOVERED，11 COMMISSIONED。Journal写失败本身
不保证还能写入Flash，所以F104会用RAM StorageFault覆盖显示JOURNAL_ERROR；旧有效副本保留。

## 6. Checkpoint与磨损

RAM received_size是接收的精确字节数；programmed_size是已处理的8B phrase数（含RAM缓存Header）。
Journal每32KiB（8扇区）提交checkpoint，记录已落盘的下界与最近块序号，不逐0x36写入。
完整224KiB槽最后一次普通checkpoint止于192KiB，避免把尚在RAM的Header误算成持久化。
37提交Header+全长后才记录完成。本阶段绝不使用checkpoint做断点续传或恢复BSC。

一次完整更新：8次状态提交+6次checkpoint=14次提交、合计14次扇区擦除，
双副本均摊约7次/扇区；初始化另1次，中断后Abort等恢复写入另计。
150KiB或200KiB payload均打包为224KiB，所以仍14次。
单槽payload最大220KiB；250KB/250KiB都超界，必须拒绝，不能为了示例压缩/移动Slot。
仅两个扇区轮换，不是复杂wear-leveling；这里统计擦除次数，不承诺硬件寿命次数。

## 7. F104诊断与Linux

F101 Active、F102 packed version、F103 Target保持原义。F104只在Bootloader提供。
请求 `22 F1 04`，响应UDS共35字节：`62 F1 04` + 下列32字节payload，需ISO-TP多帧。

| payload偏移 | 类型 | 含义 |
|---:|---|---|
| 0/1/2/3 | u8/u8/u8/u8 | ABI=1 / valid / storage_fault / reserved=0 |
| 4 | u32 BE | transaction_id |
| 8 | u32 BE | state |
| 12/16 | u32 BE各4B | active_slot / target_slot（事务创建时身份，不一定是当前运行槽） |
| 20/24 | u32 BE各4B | expected_size / committed_size |
| 28 | u32 BE | last_result |

Linux flasher在新擦除前读取、解析并打印F104；旧中断显示Previous update interrupted/failed，
从0完整下载而非checkpoint继续。无效Journal/存储错误停止，显式初始化需用户指定已知Active。
新增 `python3 uds_journal` 只读取（ECU需已在Bootloader），`--initialize B`明确初始化旧B。
例：`python3 uds_flasher app_slot_a_image.bin --initialize-journal B` 可合并初始化+完整升级，
首次验收推荐分两步操作，以先验证旧B可以独立启动。

新多帧发送支持SF/FF/CF、4位序号回绕、CTS block-size、有限等待。
当前接收方FC必须CTS/STmin=0（项目Linux客户端符合）；非零STmin/WAIT明确失败，
不假装支持所有ISO-TP流控时序。P2、P2*和Flash checkpoint耗时需板上确认。

## 8. Fault Hooks与测试

默认 `FAULT_INJECTION=0`；脚本分开产出Release与FaultInjection，结束时build/留Release。
仅调试版可在S32DS Expressions设置 `BootFault_ArmedPoint` 和 `BootFault_SkipHits`。
点号1..11：AFTER_PREPARING、DURING_ERASE、AFTER_ERASE、AFTER_DOWNLOAD_START、
PROGRAM_10_PERCENT、PROGRAM_44_PERCENT、PROGRAM_90_PERCENT、AFTER_TRANSFER_EXIT、
DURING_VERIFY、AFTER_VERIFY、DURING_JOURNAL_COMMIT。SkipHits=0命中即软件Reset。
DURING_ERASE是在扇区操作之间；DURING_VERIFY在校验开始处；Journal点在erase后及每phrase后。
这些不是Flash脉冲中真实掉电。复位后RAM注入开关清零，避免反复注入。

生产C主机测试通过792个显式断言（另有底层assert）：CRC、双副本坏/截断、回绕/歧义、
100种状态转换组合、Abort、checkpoint、两方向所有恢复状态、旧槽全区域CRC不变、
10个刷写复位点、17个Commit点、128种半phrase截断、读回损坏及首次初始化限制。
另生产ISO-TP发送11项用例、Stage3生产C回归、25项Stage2 JS、47项Python网关测试通过。
Flash/Image读取以主机假设备替代；实际FTFC/ECC/电压行为不在这些结果范围内。

## 9. 构建结果及.map地址（2026-09-04）

`powershell -ExecutionPolicy Bypass -File tools/build_stage4.ps1`

| 产物 | text / data / bss（字节） |
|---|---|
| Release Bootloader | 18560 / 24 / 6884 |
| FaultInjection Bootloader | 18576 / 24 / 6892 |
| APP_A与APP_B各自 | 26716 / 1076 / 23608 |
| RAM Driver | 752 / 0 / 8 |

Bootloader bss统计包含预留Driver/Stack，不能简单当作普通全局变量占用。
Bootloader Flash load结束0x4C58 < 0x8000；Journal仅absolute symbols，无Journal的LOAD segment。
两个Bootloader版本、APP_A/B、link-only FlexNVM草案编译成功，0警告/0错误。

| 符号/区域 | 实际地址 |
|---|---|
| Boot vector / Reset / 初始MSP | 0x0 / 0x410（Thumb 0x411） / 0x20006FF0 |
| BootUpdate_Current / BSS_END | 0x1FFF8288 / 0x1FFF830C |
| BootProgramming_Context / phrase buffer | 0x1FFF8200 / 0x1FFF8270 |
| Driver API / RAM结束 | 0x20005800 / 0x20005AF8（760B） |
| Boot stack | 0x20006000..0x20006FF0（end exclusive） |
| A vector / .text / Reset / main | 0x8000 / 0x8400 / 0x8504 / 0xC04C |
| B vector / .text / Reset / main | 0x40000 / 0x40400 / 0x40504 / 0x4404C |
| A/B Header | 0x3F000 / 0x77000 |
| App HeapLimit / StackLimit / StackTop | 0x20005438 / 0x20006BF0 / 0x20006FF0 |
| JournalCopy0 / JournalCopy1 | 0x78000 / 0x79000 |

APP_A packed=6.0.0、APP_B packed=7.0.0，均229376B，payload均27792B；
分别用于当前B5→A6以及A6→B7。原Application_Build/APP_A、APP_B未被脚本覆盖。
APP裸ELF/BIN不包含打包后的尾部Header，线上发送必须用`app_slot_*_image.bin`。

## 10. 限制与下一阶段入口

- 双Journal皆坏保守留在Bootloader；手动初始化是信任操作，选错旧槽不能由CRC替用户鉴别。
- CRC不是认证，seed/key仍是演示算法；不会把版本递增包装成安全Anti-Rollback。
- 本阶段没有Confirmed身份；Pending不等于APP已健康运行。阶段5入口是Pending→Trial/
  App Confirm协议，另设计尝试次数和回滚，当前没有提前实现。
- MCU在真实掉电中断Flash操作可能出现不同于字节损坏的ECC/读异常。目前直接Flash读取
  没有可恢复的ECC异常屏蔽/重试层；主机模拟不能证明这种情况下单副本仍能读出。
  若硬件出现BusFault/HardFault/无法读Journal，需要进一步诊断，不能报告任意掉电绝对安全。
- 断电扇区擦写、电压斜坡、重复掉电、CAN/PCAN重枚举、最大payload CRC/看门狗/P2时序、
  栈高水位尚需实物验证。Debug暂停时看门狗也可能影响观察。
- 安装新Bootloader前必须确认下载器仅擦目标扇区；ELF地址限定不等于下载器保证不全擦。
  不使用旧Combined ELF覆盖正在验收的A4/B5，不烧link-only PBL_FlexNVM。

逐项实物步骤和未完成项目见 [stage4_test_matrix.md](stage4_test_matrix.md)。
