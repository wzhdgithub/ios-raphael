# t57（r67）· BYPASS 读有效 GMU 窗口 + 低物理地址 DRAM 探针

> **r67（2026-10-27）**：t57 = t56 底座 + **0137-raphael-bypass-window-lowpa**。

## 背景（t56 结果）

- 读 0x0 返回 **0x43F80 = ITCM 向量字** → GMU 只译码自己的窗口，寄存器地址的 `0xdeadbeef` 很可能只是"未映射地址"的默认哑值，不是 NoC 结论；
- DTB 的 PA = 0x1_024d8000（>4 GB）且 head 字不是 FDT 魔数 → 低物理地址探针作废；
- 无效 TTBR0=0 也挂死（FSR 0x400，无 TF）→ TCU 连走表都没开始。

## 本轮判别

| 步骤 | 内容 | 判读 |
|---|---|---|
| A | BYPASS 映射，首读换成**低物理地址候选**（`PE_state.bootArgs` 的 PA，若能 <4 GB；否则回退 slot=0） | 读到与 host 相同的 word = GMU AHB→DRAM 真通 |
| **B0** | **HFI 读程序（0x60006F00，有效 GMU 窗口）在 `S2CR.Type=BYPASS` 下运行** | 完成（任意值）= TBU/SMMU 对我们的流确实 bypass，阻塞在 TCU 翻译；仍挂 = 事务在 SMMU 之前/之内被挡，或流表不是我们在改（hyp） |
| B | 同一读经 CB1 翻译 | 期望 `deadbeef feedface 11 cafebeef` |
| C | 写方向（host 先种 0xabcd0001） | host 变 feedface 且 dtcm0=0x11 |
| E | TTBR0=0 walk | FSR 0x402 = TCU 会走表；挂 = 不处理翻译 |
| D | S2CR.Type=FAULT | SGFSR 变化 = 事务到达本 SMMU |
| GX | 上电 + gx_gmu，A2/B2 | 对照 |
| 固件 | 恢复 TCM、全匹配 SMR→CB1、释放、fw_init | `0xBABEFACE` = M3 完成（36%） |

## 判读分支

- **B0 完成、B 挂** → SMMU 流表生效、bypass 通，问题在 TCU 翻译（时钟/hyp/TCU 端口）；下一步查 TCU 功能时钟与 QSMMU 的 hyp 策略。
- **B0 也挂** → 事务在 SMMU 之前就被挡住（或流表是 hyp 的）。下一步：CNOC/DDR bus vote（vendor “Vote for 300MHz DDR for GMU to init”）、AHB 端口时钟、XPU/安全属性。
- **A 读到 host word** → GMU AHB→DRAM 真通，哑值只是未映射窗口；主攻翻译。
- `fw_init=0xBABEFACE` → M3 完成，进入 HFI/GX/CP。

## 回退

```
fastboot boot raphael-t56-nsview-dtb-debug.img
```
