# t59（r69）· 采用 HeliumStudio raphael 驱动的厂测值：cmd_db BCM 地址 + CB ACTLR + chipid + fence

> **r69（2026-10-28）**：t59 = t58 底座 + **0139-raphael-vendor-gmu-values**。

## 背景（重大发现）

`HeliumStudio-Dev/hackios-xnu-kernel` 的 `raphael-bringup` 分支已在其 README 记录 **FD640 在 raphael 上三角形绘制+读回成功（GL error 0）**；其 `xnu/files/bsd/dev/hakios.adreno.c` 在 a640 路径下把 GMU/GPUCC/GBIF/SMMU/RSCC/CX_MISC 运行时重映射到 SM8150 地址，并给出了我们缺失的厂测值。

## 本轮改动（全部来自其 raphael 路径）

| 项 | 我们之前 | 本轮 |
|---|---|---|
| PDC 的 RPMh BCM 地址 | 硬编码 `0x30010/0x30000/0x30090` | 从 **cmd_db@0x85f20000** 解析 `mx.lvl/cx.lvl/xo.lvl`（失败才回退），并打印 |
| CB ACTLR (a640 `qcom,actlr=<0 0x407 0x303>`) | 未写 | `gpu_smmu_cfg` 写 **0x303**，A 行读回 |
| HFI_SFR_ADDR（固件识别 chip） | `0x06040000` | **`0x06040100`** |
| AHB fence range | `0x802800A0` | **`0x80A008A0`**（其 `GMU_FENCE_RANGE_MASK`），F 循环最后也停在该值 |

其余判别保持：B0（HFI 窗口 BYPASS 读）、B 翻译、C 写、E 无效 TTBR、D FAULT、GX、A2/B2、固件释放、`fw_init`。

## 判读

- `stage6: cmd_db` 打印三个真实地址；若与我们旧常量不同 → t44 的 PDC 程序一直指向错误 BCM，GMU 的 RPMh 投票全失败；
- **B0 若完成** → 外部通路打开（fence/ACTLR/chipid 之一生效），B/C/D 继续看翻译；
- `fw_init=0xBABEFACE` → M3 完成（36%），转入 HFI/GX/CP（可直接沿用其实现）；
- 若仍全挂 → 按既定路线**移植它的完整驱动**（已在计划中）。

## 回退

```
fastboot boot raphael-t57-bypass-window-debug.img
```
