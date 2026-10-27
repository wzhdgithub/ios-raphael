# t56（r66）· SMMU 非安全视图 + DTB 低物理地址 BYPASS 探针 + 无效 TTBR walk

> **r66（2026-10-27）**：t56 = t55 底座 + **0136-raphael-smmu-nsview-dtb-probe**。

## 背景（t55 结果）

t55 的四目标 BYPASS 映射（GMU CM3_CFG / GPUCC CX_GDSCR / SMMU IDR0 / SMMU IMPL_DEF1）**全部返回 0xDEADBEEF**，连 AP 能读成 0x4052 的 CM3_CFG 也是；`impl_def1` 读 0，halt→resume 环无效。所以 BYPASS 的"完成"是哑响应，不能证明 AHB/NoC 通。

## 本轮判别

| 步骤 | 内容 | 判读 |
|---|---|---|
| 1 | 读安全视图 `0x0/0x10/0x48` 与非安全视图 `0x400 nsCR0 / 0x410 nsACR / 0x448 nsGFSR`；`nsCR0.CLIENTPD=1` 则清位 | 两套视图不同 = 我们一直在改错误的安全侧；CLIENTPD=1 = 非安全客户端口被关 |
| 2 | `PE_state.deviceTreeHead` 的 PA（FDT 魔数，小端 `0xedfe0dd0`，bootloader 必放低 4 GB）作为 BYPASS 首读，另保留 CM3_CFG/GPUCC/IDR0/IMPL_DEF1 | 读到 `0xedfe0dd0` = GMU AHB→DRAM 真通，哑值只是寄存器块被挡（XPU）；全是 `0xdeadbeef` = AHB 主口没发出真实事务 |
| 3 | **无效 TTBR0=0 + SCTLR on + S2CR=translate** 读 HFI | TCU 真走表应快速报 TF（FSR 0x402）；仍挂 = TCU 不处理翻译请求 |
| 4 | 有效翻译 B / 写 C / FAULT D、GX、A2/B2 | 常规对照 |
| 5 | 恢复固件 TCM、释放、轮询 fw_init | `0xBABEFACE` = M3 完成（36%） |

## 判读分支

- **DTB 读到 0xedfe0dd0** → GMU AHB/DRAM 通；`0xdeadbeef` 是 config NoC/寄存器块被 XPU 挡。主攻 SMMU 翻译：检查 TCU 的 walk 数据通路与 ns 视图。
- **DTB 也是 0xdeadbeef** → GMU AHB 主口未真正发出事务。主攻：AHB 端口时钟、CNOC/DDR bus vote（vendor `gmu_start` 的 “Vote for 300MHz DDR for GMU to init”）、XPU/安全属性。
- **E 行 FSR=0x402** → TCU 会走表，翻译失败在页表/映射；**E 行仍挂** → TCU 不响应翻译请求（时钟/hyp/端口）。
- **nsCR0.CLIENTPD=1 且清除后 B 通** → 根因就是安全视图错位。

## 回退

```
fastboot boot raphael-t55-tcu-resume-debug.img
```
