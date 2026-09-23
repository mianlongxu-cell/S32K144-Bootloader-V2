# Stage5 Boot Lifecycle

Update Journal只描述刷写事务；Slot Metadata只描述镜像启动生命周期。Journal达到
PENDING_ACTIVATION且目标镜像有效后，Bootloader先保证旧Active为CONFIRMED，再把目标
依次提交为VERIFIED、PENDING，最后把Journal提交为IDLE。掉电重启会幂等重做未完成步骤。

合法业务转换：

| From | To | 条件 |
|---|---|---|
| VERIFIED | PENDING | 完整编程和Image校验已成功 |
| PENDING | TRIAL | 第一次Jump之前，attempt=1 |
| TRIAL | TRIAL | 再次Jump之前，attempt加1且不超过3 |
| TRIAL | CONFIRMED | 当前Trial APP通过健康检查并请求确认 |
| TRIAL | INVALID | attempt已达3且上一次仍未确认 |
| INVALID/CONFIRMED | VERIFIED/PENDING | 仅新的完整编程事务 |

EMPTY→CONFIRMED仅用于一次性Stage4迁移：Journal明确给出旧Active、镜像也重新验证有效。
普通状态API禁止EMPTY→CONFIRMED、PENDING→CONFIRMED、INVALID→CONFIRMED和
CONFIRMED→TRIAL。INVALID镜像即使版本最高也不会启动。

策略优先级为TRIAL > PENDING > CONFIRMED；只在相同状态优先级时比较版本，版本相同
选择A。没有可信Metadata+有效Image时进入Programming Mode。

APP在FreeRTOS运行至少1秒、Vehicle任务至少运行一次、CAN初始化且已有一次发送、无Fatal
后调用Boot_ConfirmApplication。接口读取Bootloader交接状态；Trial时写保留RAM请求并软件
复位，由Bootloader执行Flash提交。已Confirmed时直接成功且不擦写Flash。

构建宏APP_TRIAL_MODE：0 NORMAL、1 NO_CONFIRM、2 WATCHDOG_RESET、3 HARDFAULT_TEST、
4 RESET_LOOP、5 DELAY_CONFIRM。Release默认0；主动HardFault代码只存在于模式3。
