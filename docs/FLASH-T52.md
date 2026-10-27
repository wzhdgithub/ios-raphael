# t52（r62）· SMMU 时钟投票 + HFI 读写 + 故障注入

> **r62（2026-10-27）**：t52 = t51 底座 + **0132-raphael-smmu-clock-fault**。

## 背景（t51 结果）

30 组 (CB×SMR×icfg) 全败，w0=deadbeef、w1=0、w3=0；CB FSR 无 fault 位。发现两处缺陷：
① HFI 读取指令立即数写错（读了 ICACHE 0x4100 而不是 HFI 0x60006F00）；
② 从未给 SMMU 投时钟（`gpu_cc_hlos1_vote_gpu_smmu_clk`，GPUCC 0x5000）。

## 本轮改了什么

1. stage1 启用并打印 `smmu_vote`（0x5000）；
2. gputest 修正为真正读取 HFI 页 0x60006F00；
3. 新增写方向测试：CM3 把 0xFEEDFACE 写入 HFI 页，主机读回；
4. 新增**故障注入**：CB1 的 TTBR0 指向全零页表，SID ∈ {5,4,0,1,2} 各跑一次；
5. AHB fence 改为 Linux a640 的 0x802800a0。

## 判读

| 行 | 含义 |
|---|---|
| `t52: host hfi word ... = 0xfeedface` | 主机写入与回读正常（基线）|
| `t52: CM3 write test -> host sees feedface` | CM3→宿主的**写**通路打通 |
| `t52: fault sid=N ... fsr=0x402`（bit1=TF）| CM3 事务**确实经过**我们的 SMR/CB/页表，SID=N |
| `t52: fault sid=N ... fsr=0x400`（全部）| CM3 事务**不经过**本 SMMU 流表（hyp 托管或旁路）|
| `scan ... icfg=0 -> deadbeef feedface 11 cafe` | 全部打通 |

## 回退

```
fastboot boot raphael-t51-extpath-debug.img
```

## 实机结果（2026-10-27 12:18 boot，log 233,120 B / flush #145）

- `stage1: smmu_vote=0x1 running` — SMMU 时钟投票成功，但**不解决**问题。
- 30 组扫描（HFI 读地址已修正）全部 `0xdeadbeef 0 0 0`：读 0x60006F00（非缓存窗口）在 cfg=0/1 下都会挂住 CM3；t51 的 `w2=0x11` 只是读错地址（ICACHE 0x4100）的产物。
- 写测试：`host sees 0xabcd0001`（未被改写）、`dtcm0=0` → CM3 的写也**永远不完成**。
- 故障注入（CB1 全零页表；SID 5/4/0/1/2）：全部 `fsr=0x400 sgfsr=0`，核心均挂在访存 → **无任何 translation fault 证据**；CM3 事务要么不经本 SMMU 流表，要么到不了有响应的从设备。
- AHB fence 用 a640 上游值无影响。

结论：外部（0x60000000 非缓存）窗口的读写都不完成，且不产生 SMMU fault。进入 t53 做流表/旁路/故障三判别并直接尝试真固件。
