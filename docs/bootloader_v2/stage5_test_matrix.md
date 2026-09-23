# Stage5 Test Matrix

| # | 场景 | 预期 | 当前状态 |
|---:|---|---|---|
| 1 | A Confirmed，完整刷B | B Pending | 软件通过；实物待验 |
| 2 | B Pending首次启动 | B Trial/attempt 1 | 软件通过；实物待验 |
| 3 | B Trial正常健康确认 | B Confirmed/attempt 0 | 软件通过；实物待验 |
| 4 | B NO_CONFIRM后复位 | attempt增加 | 软件通过；实物待验 |
| 5 | 连续3次失败 | B Invalid，回退A | 软件通过；实物待验 |
| 6 | WATCHDOG_RESET | reason=WATCHDOG，attempt增加 | 已编译；实物待验 |
| 7 | HARDFAULT_TEST | reason=FAULT，attempt增加 | 已编译；实物待验 |
| 8 | Trial确认前断电 | reason=POWER_ON，attempt增加 | 软件模拟通过；实物待验 |
| 9 | 第二次Trial成功 | Confirmed，不回退 | 软件通过；实物待验 |
| 10 | A/B均Confirmed | 高版本；同版本A | 主机通过 |
| 11 | B高版本但Invalid | 启动A | 主机通过 |
| 12 | Confirm commit半写 | 旧Trial或新Confirmed | 主机通过；实物待验 |
| 13 | Rollback commit半写 | 重试后Invalid并回退 | 主机通过；实物待验 |
| 14 | Copy0坏 | Copy1恢复 | 主机通过 |
| 15 | Copy1坏 | Copy0恢复 | 主机通过 |
| 16 | 两副本均坏 | Slot fail-safe；无可信槽进编程 | 主机通过 |
| 17 | Stage4 Journal恢复回归 | 原792断言和ISO-TP 11项通过 | 通过 |

“软件通过”使用生产状态机与内存Flash模型；它不等于S32K144真实擦写中物理断电测试。
Watchdog时钟、复位寄存器、电源斜坡、PFlash ECC和PCAN重枚举仍必须在实物上逐项验收。

F105请求22 F1 05，40字节payload（UDS总43字节）：ABI、valid mask、active、reset；
A/B各state、attempt、reserved、version BE、sequence BE；result、failed slot、rollback target、
storage fault、failed version BE、last attempt BE。F103保留为编程Target，没有改义。
