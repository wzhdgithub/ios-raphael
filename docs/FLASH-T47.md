# t47（r57）· 修 AHB fence 值 + 映射 gmu_user stream 4

> **r57（2026-10-26）**：t47 = t46 底座 + **0126-raphael-fence-stream4**。

## 本轮改了什么（+18）

1. `GMU_AHB_FENCE_RANGE_0` 改回 vendor 值 `0x80a008a0`（`BIT31 | (0xa<<2)<<18 | 0x8a0`）；
2. SMR/S2CR **entry 4 = stream 4/mask 0x400 → context 1**（gmu_user），与 entry 1/5 并列；
3. 释放前写 `GMU_AO_AHB_FENCE_CTRL = 0`（ALLOW 模式）并打印 ctrl/status；
4. 保留 ICACHE gputest。

## 判读

```
stage5: smr1=... s2cr1=0x1 smr5=... s2cr5=0x1
stage5: smr4=0x84000004 s2cr4=0x1
stage6: ahb_fence ctrl=0 status=...
stage6: gputest dtcm=0xdeadbeef 0xbabeface 0xcafebeef   <- 全通
```

| 现象 | 含义 |
|---|---|
| `dtcm=deadbeef babeface cafebeef` | ICACHE 数据读+取指全通 → 厂商固件路径可用，下一步直接跑真固件 |
| `dtcm=deadbeef babeface 0` | 数据读通了，取指仍失败 |
| `dtcm=deadbeef 0 0` | 仍总线故障 → 继续查 SMMU/TBU/stream |

## 回退

```
fastboot boot raphael-t46-icache-debug.img
fastboot boot raphael-t45-cm3test-debug.img
```
