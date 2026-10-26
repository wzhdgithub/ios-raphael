# t49（r59）· GMU 的 SMMU context：试 CB0

> **r59（2026-10-26）**：t49 = t48 底座 + **0128-raphael-smmu-context-arg**。

## 本轮改了什么（+16/-9）

- 新增 `gpucb` boot-arg（0 或 1，默认 1）：选择 GMU 使用的 SMMU context bank；
- CBAR/CBA2R/CB 基址/TTBR ASID/TLBIASID/S2CR(CBNDX) 全部参数化；
- 打印 `using smmu context N (cbN@0x...)`；
- 其余（ICACHE 取指 gputest、RPMh 微码、fence、stream 4/5）不变。

本次镜像 cmdline 用 **`gpucb=0`**（CB0 at 0x8000），因为 CB1 已在 t48 失败。

## 判读

```
stage5: using smmu context 0 (cb0@0x8000)
stage5: smmr1=... s2cr1=0x0 ...（CBNDX=0）
stage6: gputest dtcm=0xdeadbeef 0x11 0xcafebeef   <- CB0 正确
```

| 现象 | 含义 |
|---|---|
| `deadbeef 11 cafebeef` | **CB0 才是 GMU 的 context**，ICACHE 取指通了 |
| `deadbeef 11 0` | CB0 也不对 → 继续查 GMU 外部内存路径 |
| 系统挂死 | CB0 与 secure bank 冲突，回报最后一行 |

## 回退

```
fastboot boot raphael-t48-fetch-debug.img
fastboot boot raphael-t47-fence-debug.img
```

## 结果（2026-10-26 23:10）

```
stage5: using smmu context 0 (cb0@0x8000)
stage5: smr1=0x84000005 s2cr1=0 smr5=0x84000005 s2cr5=0 (want ... s2cr=0)
stage6: gputest dtcm=0xdeadbeef 0x11 0
```

CB0 同样不行：ICACHE 取指仍然失败。TCU 版本寄存器正常（0x18=0x01000000），不是 TCU 没电。
→ VMware 的 CB 号（arm-smmu 动态分配）未知，逐个人工试太慢。

t50/0130 改为**一次启动内扫描**：(CB 0–4) × (SMR 策略 0–2) 共 15 种组合，每次复位 CM3、
重编程 SMMU、重启 ICACHE 测试并读回 DTCM，矩阵结果一次落盘。
