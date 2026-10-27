# t55（r65）· QSMMU TCU 本地 halt 解除 + AHB 从设备映射

> **r65（2026-10-27）**：t55 = t54 底座 + **0135-raphael-tcu-resume**。

## t54 结果（0134，13:30 boot，ring 645,904 B / flush #4561）

| 项 | 值 | 判读 |
|---|---|---|
| `hfi_pa` | `0x1d9228f00` | >4 GB，探针按设计回退读 SMMU IDR0 |
| A BYPASS 探针 | `dtcm 0x11 0xdeadbeef 0x22 0xdeadbeef` | **外部读完成**（两处标记都写到了），读回 0xdeadbeef（不是 IDR0 的 0x4c017e06） |
| B TRANSLATE→CB1 读 HFI | `deadbeef 0 0 0` | 翻译读挂死 |
| C 写（host 种 0xabcd0001） | `0 0 0 0`，host 仍 `0xabcd0001` | 翻译写挂死 |
| D S2CR.Type=FAULT | `deadbeef 0 0 0`，`sgfsr=0` | fault 路径也不回应 |
| GX GDSC | `gx_gdsc=0x222001 → 0x8222000`（上电成功）；`gx_gmu=0x1` | GX 上电后 A2/B2 无变化 |
| GBIF | `halt=0 ack=0x3`，写 0 和写 ACK 后仍 `0x3` | CX arbiter 的 ack 不随 HALT 清除 |
| 固件 | `fw_init=0`、`stage6=FAIL` | 预期（ICACHE 取指依赖翻译） |

**结论**：GMU 的 AHB 事务在 **BYPASS 下能完成**（TBU 直接放行到 NoC），但在 **TRANSLATE/FAULT 下挂死**——TBU 转发了，**TCU 从不回应**。问题从"AHB 通不通"缩小到 SMMU 的翻译引擎（TCU）。

## 关键机制（vendor 源码）

- `drivers/iommu/arm-smmu.c`：QSMMU 的 `qsmmuv2_halt()` 置 **IMPL_DEF1 micro-MMU control（SMMU base + 6×页大小 = 0x02CA6000）bit2 = LOCAL_HALT_REQ**，并等 bit3 = IDLE；`qsmmuv2_resume()` 清 bit2；`qsmmuv2_device_reset()` 固定做 **halt → 写 impl-def 寄存器 → resume**。
- 我们的 XNU 驱动从未碰过 0x02CA6000。若 bootloader/hyp 把 TCU 留在 halt：BYPASS 仍通（不经 TCU），TRANSLATE/FAULT 全部挂死且无 fault——与 t54 完全一致。

## 本轮测试（一次启动）

| 步骤 | 内容 | 判读 |
|---|---|---|
| 0 | AP 读 `0x02CA6000`，打印 halt_req/idle | bit2=1 即 TCU 停在 halt |
| 1 | 清 bit2 并等 idle 掉（未 halt 则做一次 halt→resume 环） | 读回应为 bit2=0/bit3=0 |
| A | BYPASS 下 CM3 依次读 GMU CM3_CFG(0x02c7e0b4)、GPUCC CX_GDSCR(0x02c9106c)、SMMU IDR0(0x02ca0020)、SMMU IMPL_DEF1(0x02ca6000) → DTCM[1..4] | 哪些从设备经 GMU AHB 有响应；0xdeadbeef = 被挡的哑响应 |
| B | CB1 翻译读 HFI 虚拟地址 | `deadbeef feedface 11 cafebeef` = TCU 恢复、翻译通 |
| C | 写方向（host 先种 0xabcd0001） | host 变 feedface 且 dtcm0=0x11 |
| D | S2CR.Type=FAULT | SGFSR 变化 = TCU 处理了事务 |
| GX | 上电 + gx_gmu 时钟，再跑 A2/B2 | 对照 + M5 子步骤 |
| 固件 | 恢复 TCM、全匹配 SMR→CB1、释放、轮询 fw_init | `0xBABEFACE` = M3 完成（36%） |

## 判读

- `impl_def1` bit2=1 → 根因确认；resume 后 B 出现 `feedface` 是直接胜利，固件有真实启动机会。
- bit2=0 → TCU 没被 halt；B 仍挂则问题在 TCU 的**功能时钟/NoC 通路**（TCU 页表 walk 走不出去）或 QSMMU hyp 策略，下一步读 TCU 状态并查 SMMU 的 TCU 时钟/投票。
- A 中 `0xdeadbeef` 只出现在 IDR0/IMPL_DEF1 而 CM3_CFG/GPUCC 正常 → GMU AHB 到 **config NoC 的 SMMU/GMUC 从设备**被 XPU 挡（安全属性），DDR 路径另行验证。

## 回退

```
fastboot boot raphael-t54-ahb-path-debug.img
```
