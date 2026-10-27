# t54（r64）· GMU AHB 通路：真 BYPASS 探针 + GBIF 解除 halt + GX 上电

> **r64（2026-10-27）**：t54 = t53 底座 + **0134-raphael-gmu-ahb-path**。

## t53 结果（0133，12:46 boot，ring 242,496 B / flush #206）

| 项 | 值 | 判读 |
|---|---|---|
| baseline | `cbar1=0x1f300 smr1=0x84000005 s2cr1=0x1` | SMMU 接受并回读我们的编程（时钟投票后读取可信） |
| test 0（全匹配 SMR→CB1 翻译读 HFI） | `dtcm deadbeef 0 0 0 fsr=0x400 sgfsr=0` | 外部读不完成 |
| test 1（t53 版“全 BYPASS”） | `dtcm 0 0 0 0` | **无判别力**：程序根本不写 DTCM，且 SMR 全无效时 S2CR 不参与匹配 |
| test 2（全匹配 + S2CR.Type=FAULT） | `dtcm deadbeef 0 0 0 fsr=0x400 sgfsr=0` | **零 SGFSR** → 事务没到本 SMMU |
| test 3（写 HFI 页） | `dtcm 0 0 0 0` | 外部写同样不完成；`host sees feedface` 是空检验（host 自己种的值） |
| 固件 | `fw_init=0`，GMU log 空 | `stage6=FAIL fw=did-not-start` |
| stage6 转储 | `tbu0 00002000 …`；`tbu_pwr=0` | 只读了 TBU0；`tbu_pwr` 是普通读，vendor 用 SCM IO 读且 TCU v1.0 的 TBU status-reg 不可访问 → **该 0 不可信** |

**结论**：CM3 核心正常（ITCM 执行、DTCM 写入正常），但所有外部访问（读/写、翻译/FAULT 配置）都挂死且不产生任何 SMMU fault。阻塞点在 SMMU **之前**：GMU AHB 主口 / NoC / GBIF，而不是流表、CB 或页表。Raphael 的 SID 4/5 走 **gfx_1_tbu（0x2CC9000，status 0x2CC2208）**，此前从未读过。

## 本轮判别（一次启动）

| test | 配置 | 期望/判读 |
|---|---|---|
| A | 有效全匹配 SMR + `S2CR.Type=BYPASS`；探针读 **HFI 页物理地址**（<4 GB 时）→ DTCM[1]，再读 SMMU IDR0 → DTCM[3] | `11 <hfi 值> 22 4c017e06` = AHB/NoC/DRAM 通，问题在翻译 |
| B | 全匹配 + `S2CR=1`（CB1 真页表）读 HFI 虚拟地址 | `deadbeef feedface 11 cafebeef` = 翻译通 |
| C | B 的映射 + CM3 写 HFI 页；host 先种 `0xabcd0001` | host 变 `feedface` 且 dtcm0=0x11 = 写方向通 |
| D | 全匹配 + `S2CR.Type=FAULT` | SGFSR/FSR 变化 = 事务到达本 SMMU |
| GBIF | HALT=0 后回读 HALT/ACK（t53 stage1 读到 ack=0x3） | ack 清零 = CX arbiter 未停；仍 0x3 = halt 未解除 |
| GX | GX GDSC 上电 + `gx_gmu` 时钟（GMU 节点的第二个电源） | A2/B2 是否变化 |

之后恢复真固件 ITCM/DTCM，保留全匹配 SMR→CB1，释放 CM3 轮询 `fw_init`。

## 判读

- **A 成功**（读到 DRAM 值）→ GMU AHB 主口/NoC/DRAM 都正常，下一步专攻 SMMU 翻译（流表/CB/QSMMU vault）。
- **A 挂住且 GBIF ack 仍 0x3** → GBIF CX arbiter 未解除 halt，下一步查 RBBM/GBIF 解除 halt 的正确写法。
- **A 挂住、GBIF ack 已清零** → GMU AHB 主口未就绪：看 A2/B2 是否因 GX 上电而改变；仍挂 → `GMU_SYS_BUS_CONFIG`/AHB 时钟/安全属性（XPU、PAS_INIT_IMAGE）。
- **B 读到 feedface** → 翻译通，直接看 `fw_init` 是否 0xBABEFACE。
- `fw_init=0xBABEFACE` → M3 完成（总进度 36%），转入 HFI 队列 + GX/RBBM/CP。

## 回退

```
fastboot boot raphael-t53-smmu-path-debug.img
```
