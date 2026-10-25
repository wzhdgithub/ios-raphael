# t40（r50）刷写与判读 · SMMU S2CR 修复，让 CM3 能取指

> **r50（2026-10-25）**：t40 = t39 底座 + **0119-raphael-gmu-smmu-s2cr**。
> t39 已证明 GMU CSR/TCM/时钟全部正常、stage 5 通过、stage 6 配置被接受，
> 但 CM3 不执行（`fw_init=0`）。t39 的固件分析显示 `0xBABEFACE` 的写入代码位于
> **ICACHE 区（GMU VMA 0x4000）**，该区是经 GMU SMMU 映射的宿主内存；
> 而 stage 5 的 S2CR 写的是 `1u` —— ARM SMMU-500 里那是 **TYPE=BYPASS、CBNDX=0**，
> 不是注释所说的“stream 5 → context 1”。于是 CM3 从物理 0x4000 取指（垃圾），
> HFI/log 访问也绕过翻译 → 永远写不出 FW_INIT。

## 本轮改了什么（0119，+30/-6）

1. **S2CR(5) = 1 << 8**（TYPE=0 翻译，CBNDX=1）；日志打印写入值；
2. 补回 vendor 启动序列里的 `GMU_PWR_COL_CP_RESP = 0`（0115 误删）；
3. AHB fence 值改为 a640 上游语义：`BIT31 | 0xa<<18 | 0xa0`（原值来自 HackiOS a660）；
4. 启动前把 GMU 日志页清零并 clean，失败后 dump 前 16 个 word + GMU log 写指针 + SMMU CB FSR；
5. 其余（CSR rebase、固件布局、窗口）不变。

## 刷写

```
fastboot boot raphael-t40-s2cr-debug.img
```

cmdline 仍带 `gpuread=1 ufslogsecs=1`。

## 判读树

```
stage5: smmu cb1@0x9000 ttbr=... smr5/s2cr5=0x100 sent
stage5=OK fw=loaded tcm_wrong=0 ddr_blocks=2
stage6: GMU config registers written
stage6: GMU configured ..., releasing CM3
stage6: gmu log wptr=...  smmu_fsr=...
stage6: gmu log xxxx xxxx xxxx xxxx   <- 前 16 words
stage6: fw_init=0xbabeface -> stage6=OK fw=running
```

| 现象 | 含义 | 结论 |
|---|---|---|
| `fw_init=0xbabeface` | CM3 执行，固件写入 init 值 | **stage 6 完成**，进入 HFI |
| `smmu_fsr` 非 0 | GMU 的 SMMU 翻译故障 | 按 FSR 位查页表/属性 |
| `gmu log` 出现非 0 条目 | 固件跑过并写了日志 | 按日志定位失败点 |
| `gmu log` 全 0 且 fw_init=0 | CM3 仍未执行 | 查 RSCC/启动条件 |

## 日志取证

```
adb exec-out tail -c 4198400 /dev/block/by-name/userdata > D:\K20iOS\sweet-log-t40.bin
python D:\ios-raphael\tools\sweet-log.py D:\K20iOS\sweet-log-t40.bin D:\K20iOS\sweet-log-t40.txt
grep -a "SWEET GPU" D:\K20iOS\sweet-log-t40.txt
```

新鲜度：total 应大于 t39 的 225,565；应出现 `smr5/s2cr5=0x100` 与 `gmu log wptr=`。

## 回退

```
fastboot boot raphael-t39-rebase-debug.img
fastboot boot raphael-t38-gate-debug.img
fastboot boot raphael-t12-gt9886-clean.img
```
