# t31（r41）刷写与判读 · SMMU context bank 页（0x8000）直读

> **r41（2026-10-25）**：t31 = t30 底座 + **0110-raphael-gpu-smmu-cb**。
> t29 证明直接读 GR1 的 CBAR(0x1000) 会卡死线程；t30 证明 SCM IO 读被 TZ 拒绝（status=-2）。
> 厂商 `kgsl_iommu.c` 给出另一条事实：**context bank 寄存器在 `base + 0x8000 + n*0x1000`**（`KGSL_IOMMU_CB0_OFFSET=0x8000`），且厂商对它们是**直接 MMIO 读写**。本轮绕开 GR1，直接读 CB 页。

## 本轮改了什么（0110，+21/-26）

- 保留 SCM IO 读 IDR0 一次（记录拒绝状态），**删除所有 GR1/CBAR 读取**；
- 依次读 CB0..CB4 的 `SCTLR`(0x0)、`TTBR0`(0x20)、`TCR`(0x30)、`FSR`(0x58)，地址 `0x8000 + n*0x1000`；
- 判决 `stage4=OK/PARTIAL ... cb_regs=responding/all-ones`；仍然全部只读。

## 刷写

```
fastboot boot raphael-t31-cb-clean.img
# 或
fastboot boot raphael-t31-cb-debug.img
```

## 判读树

```
SWEET GPU: stage4: smmu idr0=0x4c017e06 ...
SWEET GPU: stage4: scm_io read idr0 status=-2 value=0 (direct 0x4c017e06)
SWEET GPU: stage4: cb0 sctlr=... ttbr0=... tcr=... fsr=...
SWEET GPU: stage4: cb1 ...
SWEET GPU: stage4=OK smmu=responding matches=6 contexts=5 cb_regs=responding
```

| 现象 | 含义 | 结论 |
|---|---|---|
| `cb_regs=responding`，CB0/CB1 有合理值 | CB 页可直接访问 | **SMMU 编程通道成立**：下一轮写 GMU 的 CB1 + SMR/S2CR + 页表 |
| `cb_regs=all-ones` | CB 页不响应 | 需要 hyp/SMMU_PROGRAM 接口，回报原始值 |
| 卡死在某个 `cb%u` 之后 | 该 CB 页读取不返回（如 HYP 保护的 CB5）| 回报卡住的 CB 编号；下一轮只读 CB0/1 |
| CB 值全 0 | 未初始化（skip-init）| 正常，编程时写入 |
| `scm_io status=-2` 仍出现 | 与 t30 一致 | 预期，不再依赖它 |

## 日志取证

```
adb exec-out "tail -c 4198400 /dev/block/by-name/userdata" > D:\K20iOS\sweet-log.bin
```

## 回退

```
fastboot boot raphael-t30-scmio-clean.img
fastboot boot raphael-t29-smmu-clean.img
fastboot boot raphael-t12-gt9886-clean.img
```

## 下一轮（stage 3a）

若 CB 页可读：为 GMU（stream 5、SMR 匹配值 1、CB1）写二级页表（4K 页，映射固件 DDR 段 + HFI/日志页），写 CB1 的 TCR/TTBR0/SCTLR，写 SMR[?]/S2CR[?] 把 stream 5 指到 CB1，再装固件、放 CM3 出复位。
