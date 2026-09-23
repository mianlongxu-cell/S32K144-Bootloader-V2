# 阶段3基线与阶段4接入点

第三阶段已由用户验收：A2→B3→A4→B5；A4保留、B5运行；坏CRC拒绝、
传输中止和约44%实物断电后旧APP恢复。此结论不是本次主机测试推测。

原流程：10 02进入Bootloader → 27解锁 → F101/F102/F103查询 → FF00擦除Inactive →
34全槽下载 → 36顺序传输 → 37结束 → FF01完整性检查 → 11复位。

保留的约束：CAN 500k、7E0/7E8、Classical CAN/ISO-TP、每块最多128数据字节、
8字节缓冲、BSC从1开始并回绕、重复/错序73不推进计数、精确0x38000全槽长度、
Header位于最后4KiB扇区、Header最后发布、版本必须高于Active、旧Metadata只读。
独立RAM Driver仍在0x20005800，应用/Bootloader/堆栈地址不动。

阶段4仅在这些操作之间加入事务提交：

| 现有操作 | 新增持久化约束 |
|---|---|
| FF00擦除前 | Begin→PREPARING→ERASING先提交，再碰目标Flash |
| 擦除完成 | DOWNLOAD_READY提交成功才返回成功 |
| 34 | PROGRAMMING提交后接收数据 |
| 36 | RAM精确进度；每32KiB持久化checkpoint，不逐包擦Journal |
| 37 | Header缓存+TRANSFER_COMPLETE持久化成功才应答77 |
| FF01 | VERIFYING→沿用BootImage校验→发布Header→回读→VERIFIED→PENDING_ACTIVATION |
| 11 | 仅在本会话完整校验且Pending提交成功后允许复位 |

Session重新进入/传输超时会持久化Abort，再丢弃RAM接收上下文。错误请求不会推进状态。
失败的Journal提交锁住当前更新；不能忽略错误继续擦写。新的完整升级必须重新擦除。

F103仍是Target，不重定义；F104是新状态。Linux仅在ECU明确回复F104不支持时
保留旧Stage3流程；超时、格式错误、Stage4无效Journal都不静默降级。

阶段4主机回归执行了生产boot_programming/boot_uds/boot_update/boot_journal等代码；
板级CAN、FTFC时序、掉电电气行为仍需第四阶段实测。
