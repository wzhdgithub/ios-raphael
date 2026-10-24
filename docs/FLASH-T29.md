# t29（r39）刷写与判读 · GPU stage 4 前置：SMMU 只读探针

> **r39（2026-10-25）**：t29 = t28 底座 + **0108-raphael-gpu-smmu-probe**。
> t28 已确认：计数器 19,200,001 Hz、`tz=available`、PAS init/auth 两调用可用（`supported` 查询返回 0 但无驱动依赖它）。
> GMU 固件的 DDR 段（22,676 B @ 0x4000/0x40000/0x43fe0/0x57000）必须经 **GPU SMMU（stream 5）** 才能装载，所以先读清 SMMU 的现状。

## 本轮改了什么（0108，+68 行，纯只读）

- 映射 GPU SMMU `0x2ca0000`（0x10000，sm8150 kgsl-smmu）；
- 读 `IDR0/1/2/7、SCR0、SGFSR`，由 IDR1 推导页大小（4K/64K）、全局页数、context 数、stream match 数，并与 DTS 的 0x10000 对账；
- 读 stream 表 `SMR[0..7]/S2CR[0..7]`、`CBAR[0..]`（第二页）、context 0 的 `SCTLR/TTBR0/TCR/FSR`；
- 判决 `stage4=OK smmu=responding matches=… contexts=…`；`gpusmmu=0` 跳过。**不写任何 SMMU 寄存器。**

## 刷写

```
fastboot boot raphael-t29-smmu-clean.img
# 或
fastboot boot raphael-t29-smmu-debug.img
```

## 判读树

```
SWEET GPU: stage4: smmu idr0=0x... idr1=0x... idr2=0x... idr7=0x... scr0=0x... sgfsr=0x...
SWEET GPU: stage4: smmu matches=... contexts=... page=0x1000 pages=8 regspace=0x10000
SWEET GPU: stage4: smmu smr[0]=... s2cr[0]=...
...
SWEET GPU: stage4: smmu cb0 sctlr=... ttbr0=... tcr=... fsr=...
SWEET GPU: stage4=OK smmu=responding matches=... contexts=...
```

| 现象 | 含义 | 下一步 |
|---|---|---|
| `stage4=OK`，`matches/contexts` 合理（MMU-500 常见 8/8 或 16/16）| SMMU 可访问、布局符合 DTS | 按 SMR/S2CR 现状写 GMU stream 5 的上下文与页表 |
| `regspace != 0x10000` | IDR1 解释与 DTS 不符 | 回报 idr1 原值，按实际调整解析 |
| `smmu cb0 ... fsr≠0` | ABL 留下的旧上下文有故障记录 | 记录，不影响重编程 |
| `smmu=responding` 但 SMR 全 0 / S2CR 全 0x… | 流表为空（skip-init），需要自建 | 正常预期 |
| 卡死在 `mapping SMMU` 之后 | 极少见（时钟未开）| 强制重启，回报最后几行 |

## 日志取证

```
adb exec-out "tail -c 4198400 /dev/block/by-name/userdata" > D:\K20iOS\sweet-log.bin
```

## 回退

```
fastboot boot raphael-t28-tz-clean.img
fastboot boot raphael-t27-gmu-clean.img
fastboot boot raphael-t12-gt9886-clean.img
```

## 下一轮（stage 3a）

用读到的 SMR/S2CR 现状 + IDR 布局，为 GMU（stream 5、匹配值 1）建一套 4K 页表：把固件的 DDR 段映射到我们分配的连续内存，写 CBAR/CBA2R/S2CR/SMR，然后装 TCM+DDR、配置 CM3 boot、释放复位、等 `FW_INIT_RESULT`。
