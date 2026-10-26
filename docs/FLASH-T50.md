# t50（r60）· 一次启动扫描 SMMU 组合（CB 0–4 × SMR 策略 0–2）

> **r60（2026-10-26）**：t50 = t49 底座 + **0130-raphael-smmu-scan**。

## 本轮改了什么（+70/-8）

- `gpuscan=1` 时，在 stage 6 配置完成后循环 15 种组合：
  - context bank `cb=0..4`（CBAR/CBA2R/TTBR ASID/TLBIASID/S2CR.CBNDX 全参数化）；
  - SMR 策略 `smr=0`（entry 1/4/5）、`1`（0–5 全为 stream 5）、`2`（5/4 交替）；
  - 每次：复位 CM3 → 重编程 SMMU → 重写 ITCM 测试程序/ICACHE 例程 → 清 DTCM →
    释放 CM3 → 读 DTCM[0..2]；
- 扫描后 CM3 保持复位，打印 `scan done`；
- 额外转储：SGFSR、TBU_PWR_STATUS、SYNC_INV_TBU_ACK、SAFE_WAIT、CB0–4 FSR/FAR/FSYNR0、TBU 寄存器。

## 判读

```
stage5: gputest armed (icache_mem=...)
stage6: scan cb=0 smr=0 -> deadbeef 11 0
stage6: scan cb=0 smr=1 -> ...
...
stage6: scan cb=4 smr=2 -> deadbeef 11 cafebeef   <- 找到正确组合
```

- 任意一行出现 `deadbeef 11 cafebeef` → 该 (cb, smr) 正确，下一轮固定它并跑真固件；
- 全部 `deadbeef 11 0` → CB/SMR 组合都不是原因，转查 GMU 的 master 时钟/安全属性；
- 同时看 `smmu sgfsr/tbu_pwr/sync_inv/safe_wait` 与 `cb fsr` 行。

## 回退

```
fastboot boot raphael-t49-cb0-debug.img
fastboot boot raphael-t48-fetch-debug.img
```
