# Stage5 Slot Metadata

每个Slot有两个独立4KiB扇区，记录固定80字节、8字节对齐、PFlash小端。A0/A1位于
0x7A000/0x7B000，B0/B1位于0x7C000/0x7D000。0x7E000保留，0x7F000旧Metadata只读。

| 偏移 | 字段 |
|---:|---|
| 0..15 | magic=0x4D355642、format=1、slot_id、image_version |
| 16..35 | state、boot_attempts、successful_boots、update_counter、sequence |
| 36..47 | last_reset_reason、flags、last_result |
| 48..63 | failed_slot、failed_version、last_attempt_count、rollback_target |
| 64..71 | reserved[2]，必须0 |
| 72 | metadata_crc32，覆盖bytes[0..71] |
| 76 | commit_marker=0x534C4F54 |

Commit读取当前有效副本，构造sequence+1的新记录，计算CRC，擦除另一副本，逐phrase写入并
把CRC/marker所在最后phrase最后写，随后回读、语义校验和逐字节比较。只有全部通过才返回
成功。新副本半写时旧副本保持有效；两份有效时使用模2^32半范围规则选择最新sequence。
相同sequence但内容冲突、相差2^31、两份CRC都坏均视为不可恢复。

单副本损坏使用另一副本。某Slot双副本损坏时该Slot不可信；另一Slot若为有效CONFIRMED仍
可启动。两个Slot均不可信时进入Programming Mode，不根据Image版本猜测。
