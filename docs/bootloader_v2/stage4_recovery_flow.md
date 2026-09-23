# Stage4 Reset / Power-Loss Recovery

启动先加载RAM Driver并启用Flash操作看门狗，再LoadLatest/Recovery，最后才扫描允许的Slot。
BootPolicy仍选更高版本；候选必须同时满足ImageValid与Journal许可。BootJump再次检查两者。

| 最新记录 | 恢复动作 | Target | Active |
|---|---|---|---|
| 两份无效/空白/sequence歧义 | 留Programming Mode，需显式初始化 | 不允许 | 不猜测 |
| IDLE | 正常BootPolicy | 可参与校验 | 可参与校验 |
| PREPARING | Abort，INTERRUPTED | 禁止 | 完整有效则可启动 |
| ERASING | Abort，INTERRUPTED | 禁止 | 完整有效则可启动 |
| DOWNLOAD_READY | Abort，INTERRUPTED | 禁止 | 完整有效则可启动 |
| PROGRAMMING | Abort，INTERRUPTED_PROGRAMMING | 禁止；无续传 | 完整有效则可启动 |
| TRANSFER_COMPLETE | 提交VERIFYING，使用Journal缓存Header重新校验payload，必要时发布Header | 成功后Verified→Pending | 保留 |
| VERIFYING | 同上重新校验，不信上次RAM结果 | 成功后Verified→Pending | 保留 |
| VERIFIED | 再校验，必要时幂等发布Header，然后Pending提交 | 成功后允许参与版本策略 | 保留 |
| PENDING_ACTIVATION | 再校验cached Header/payload及已发布Header | 校验成功才允许 | 保留 |
| ABORTED | 保留txn/target/进度/last_result，不清成IDLE | 禁止 | 完整有效则可启动 |

重新校验失败：Abort VERIFY_FAILED，旧Active仍保留。Recovery提交失败：StorageFault置位，
拒绝目标启动/继续刷写；若旧有效Journal还能确定Active，则只允许该Active参与Image校验。
没有有效Journal时，保留RAM的请求魔术码不能代替持久化事实来源。

验证流程复用BootImage_Validate(slot,&info)，info.header从Journal.cached_header取；
这是必要的，因为阶段3下载时Flash Header仍为无效标记。校验通过才擦Header扇区并写64B，
再从Flash LoadInfo验证。已发布匹配的有效Header不重复擦除。恢复到Pending标记LastResult=
POWER_LOSS_RECOVERED；软件不能鉴别这次中断究竟是物理掉电还是手动Reset，该名称表示恢复分支。

事务内Active不变；下一次从新APP进入Bootloader后，BootManager捕获新Active，Begin建立新事务。
旧事务里的active_slot是历史身份，不应拿它和F101当前运行槽混为一谈。
若记录中的Active镜像后来也坏了，继续保留其写保护身份，进入编程而不擦它；另一槽可完整重刷。

## 不做的恢复

不恢复ISO-TP分帧状态/BSC，不从44%继续；低于37的中断必须重新FF00→34→全部36。
没有Trial/App Confirm/Boot Attempt/Automatic Rollback；旧槽启动属于未完成更新的安全保留，
不是对已启动新APP健康状况的回滚。

单副本损坏、截断、CRC错误在可正常读取Flash的前提下选择另一个有效副本；真实Flash ECC
读异常尚没有专用隔离处理，硬件故障可能停机进入Fault而不是此表的正常恢复分支。
两份都坏需要人工重建可信身份，不能仅因某个镜像CRC好或版本高就宣布它可信。
