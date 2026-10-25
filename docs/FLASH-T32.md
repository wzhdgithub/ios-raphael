# t32（r42）刷写与判读 · SMMU context 1（盲写）+ GMU 固件落位（不释放 CM3）

> **r42（2026-10-25）**：t32 = t31 底座 + **0111-raphael-gmu-firmware-load**（新增 `a640_gmu_fw.h` 37,680 B + sweet_gpu.c +258 行）。
> 前置事实：SMMU 的 IDR/SCR0 可读但其余寄存器读出垃圾、SCM IO 被拒——**只能盲写**；厂商驱动对 CBAR/SMR/S2CR/CB 寄存器就是直接 writel。GMU 固件 37,680 B：TCM 14,892 B（ITCM）+ DDR 22,676 B（4 段）。

## 本轮做了什么（全部在 stage 5，`gpufw=0` 可跳过）

1. 分配 4K 页表（L1/L2/L3，PTE 共享+AF+非全局+uncached）并建立映射：
   - 固件 DDR 段 `0x4000/0x40000/0x43fe0/0x57000` → 各自 kmem_alloc 的内存；
   - GMU 日志页 `0x60002000`（16K）与 HFI 页 `0x60006000`（4K）；
2. 把 CM3 保持在复位（`CM3_SYSRESET=1`），把 TCM 段**逐字经 GMU ITCM/DTCM 窗口写入并回读校验**；DDR 段 memcpy 进映射内存并 `dc civac` 刷出；
3. **盲写** SMMU context 1（GMU kernel CB）：CBAR/CBA2R → SCTLR=0 → TCR2/TCR/TTBR0/TTBR1/MAIR0/1/FSR → TLBIASID/TLBSYNC → SCTLR=ON → SMR[5]/S2CR[5]（stream 5、mask 0x400、→ context 1）；
4. 判决 `stage5=OK/PARTIAL fw=loaded tcm_wrong=N ddr_blocks=4 CM3=HELD_IN_RESET`。**不释放 CM3**——stage 6 才启动。

## 刷写

```
fastboot boot raphael-t32-fw-clean.img
# 或
fastboot boot raphael-t32-fw-debug.img
```

## 判读树

```
SWEET GPU: stage5: fw 0x30 bytes at 0x0 -> ITCM
SWEET GPU: stage5: fw 0x140 bytes at 0x200 -> ITCM
SWEET GPU: stage5: fw 0x38bc bytes at 0x400 -> ITCM
SWEET GPU: stage5: fw 0x3a58 bytes at 0x4000 -> memory pa=0x...
...（4 个 DDR 块）
SWEET GPU: stage5: 5 tables, root pa=0x..., fw tcm=3723 words wrong=0, ddr=22676 B in 4 blocks, log/hfi 16384/4096 B
SWEET GPU: stage5: smmu cb1@0x9000 ttbr=0x..., smr5/s2cr5 written
SWEET GPU: stage5=OK fw=loaded tcm_wrong=0 ddr_blocks=4 CM3=HELD_IN_RESET (stage 6 releases)
```

| 现象 | 含义 | 结论 |
|---|---|---|
| `stage5=OK fw=loaded tcm_wrong=0 ddr_blocks=4` | 固件已完整落位、SMMU 已盲写 | **stage 5 完成**，下一轮释放 CM3 等 `0xbabeface` |
| `tcm_wrong>0` | TCM 写入回读不一致（时钟/窗口问题）| 回报 wrong 计数与最后一块地址 |
| `ddr_blocks<4` | 有固件块放不下（分配失败）| 回报失败地址 |
| 卡死在 `stage5: smmu cb1` 之前/之后 | GR1/CB 写触发挂起（少见于写）| 强制重启，回报最后一行 |
| 卡死在 TCM 写入 | GMU 时钟未跑/窗口错 | 回报最后一块地址 |

## 日志取证

```
adb exec-out "tail -c 4198400 /dev/block/by-name/userdata" > D:\K20iOS\sweet-log.bin
```

## 回退

```
fastboot boot raphael-t31-cb-clean.img
fastboot boot raphael-t30-scmio-clean.img
fastboot boot raphael-t12-gt9886-clean.img
```

## 下一轮（stage 6）

释放 CM3（`CM3_SYSRESET=0`），轮询 `FW_INIT_RESULT` 直到 `0xbabeface`（或 0x1xx 的 started 掩码）；失败则读 HFI/错误码。成功后进入 HFI 队列与 GX 上电。
