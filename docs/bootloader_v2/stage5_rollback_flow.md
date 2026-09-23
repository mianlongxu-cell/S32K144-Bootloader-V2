# Stage5 Rollback Flow

```text
A V12 CONFIRMED + B V13 PENDING
                 |
                 v  commit before jump
             B V13 TRIAL, attempt 1
                 |
       no confirm + reset/power cycle
                 v
             attempt 2 -> jump B
                 |
       no confirm + reset/watchdog/fault
                 v
             attempt 3 -> jump B
                 |
       no confirm + next reset
                 v
             B V13 INVALID
                 |
                 v
        ROLLBACK_OCCURRED -> A V12 CONFIRMED
```

Rollback不复制Flash，只禁止失败Slot并重新选择原CONFIRMED Slot。写INVALID时掉电会恢复旧
TRIAL或新INVALID；若恢复TRIAL，下一次策略再次提交INVALID，最终仍回退。Reset reason记录
Power-on、Watchdog、Software、External、Fault、Low-voltage、Clock或Debug；第一版判断只看
Trial是否确认，不把任何Reset reason误判成成功。
