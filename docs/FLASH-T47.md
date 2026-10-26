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

## 结果（2026-10-26 16:54）

```
stage5: smr1=0x84000005 s2cr1=0x1 smr5=0x84000005 s2cr5=0x1
stage5: smr4=0x84000004 s2cr4=0x1
stage6: ahb_fence ctrl=0 status=0
stage6: gputest dtcm=0xdeadbeef 0 0 (want deadbeef babeface cafebeef)
```

仍然 `deadbeef 0 0`：**fence 值与 stream 4 都不是原因**。CM3 对 ICACHE 0x7280 的读仍然总线故障。

**补充分析**：
- 固件 ITCM 0x1f98 有一个函数指针 **0x71bd**（→ ICACHE 区 0x71bc 的 Thumb 代码），
  说明厂商固件确实要执行 ICACHE 区的代码；
- 但 t46/t47 用的是**数据读**（LDR）0x7280。若 ICACHE 窗口对数据访问不开放（只允许取指），
  这个测试会误报。t48/0127 改为**纯取指**测试：ITCM 写 DTCM[0]=deadbeef、DTCM[1]=0x11，
  然后 `blx` 到放在 ICACHE 0x4100 的小程序（写 DTCM[2]=cafebeef）。
