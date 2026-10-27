# t53（r63）· 流表三判别 + 真固件直启

> **r63（2026-10-27）**：t53 = t52 底座 + **0133-raphael-smmu-path-tests**。

## 背景（t52 结果）

非缓存窗口（0x60000000）读写都不完成；任何 SID 的故障注入都无 FSR/SGFSR 证据；SMMU 时钟投票无效。

## 本轮测试（一启动作完）

| test | 配置 | 期望/判读 |
|---|---|---|
| 0 | SMR 全匹配（MASK=0x7fff），S2CR 全 → CB1 真页表 | CM3 读 HFI → `feedface` = 流表写入生效且翻译通 |
| 1 | SMR 无效，S2CR 全 = BYPASS | CM3 读物理 0x02ca0020（SMMU IDR0=0x4c017e06）/ 0x02c7e0b4（CM3_CFG=0x4052）；读到 = GMU AHB 主口正常 |
| 2 | SMR 全匹配，S2CR 全 = FAULT | SGFSR/FSR 变化 = 事务确实到达本 SMMU |
| 3 | test 0 映射 + CM3 写 0xFEEDFACE 到 HFI 页 | 主机读回 = 写方向通 |

之后：恢复真固件 ITCM/DTCM，保留全匹配 SMR→CB1，直接释放 CM3 并轮询 `fw_init`。

## 判读

- test 0/3 成功 → 外部通路打通，真固件有望直接启动（`fw_init=0xBABEFACE`）。
- test 1 读到 IDR0/CM3_CFG 而 test 0 失败 → GMU AHB 正常，翻译/流表是问题。
- test 1 也挂住 → GMU AHB 主口本身未就绪（端口/时钟/安全属性），与 SMMU 无关。
- test 2 有 SGFSR 变化 → 事务到达 SMMU；无变化 → 不经本 SMMU。

## 回退

```
fastboot boot raphael-t52-smmu-clock-debug.img
```
