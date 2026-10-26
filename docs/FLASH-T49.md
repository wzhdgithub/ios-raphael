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
