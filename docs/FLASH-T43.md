# t43（r53）刷写与判读 · 关掉 RSC 唤醒，验证 S2CR 修复后的 CM3

> **r53（2026-10-26）**：t43 = t42 底座 + **0122-raphael-rsc-gate**。

## 本轮改了什么（0122，+55/-31）

1. `gpursc` boot-arg（**默认 0**）：t42 的 RSCC_CONTROL_REQ=2 唤醒请求在 ACK 不来后把系统带死，
   默认关闭；需要时用 repack 加 `gpursc=1` 再试；
2. `gpusafe` boot-arg（默认 1）：QSMMU safe-toggle 可关（t42 已证明调用成功）；
3. stage 6 配置写分三组（identity/HFI、fence/CM3/log、cache/RPMH/intr），每组后
   `gpu_wait(1500)` 并打印，若再次挂死可精确到组；
4. S2CR/流表项/TLBSTATUS/日志/FSR 全部保留。

## 刷写

```
fastboot boot raphael-t43-norsc-debug.img
```

cmdline：`gpuread=1 ufslogsecs=1`（不含 gpursc，即默认 0）。

## 判读树

```
stage5: smr1=0x84000005 s2cr1=0x1 ... tlbstatus=0
stage5=OK fw=loaded ...
stage6: rsc wake skipped (gpursc=0)
stage6: GMU registers in 11 s
stage6: config group 1 written (identity/HFI)
stage6: config group 2 written (fence/CM3/log)
stage6: GMU config registers written
stage6: gmu log wptr=... smmu_fsr=...
stage6: fw_init=0xbabeface
```

| 现象 | 含义 | 结论 |
|---|---|---|
| `fw_init=0xbabeface` | CM3 固件运行 | **stage 6 完成** |
| 停在 group 1/2/3 之间 | 对应组写挂死 | 回报组号，下一轮逐条写 |
| 全组通过但 fw_init=0 | CM3 仍不执行 | 看 gmu log / rsc / 时钟 |
| `s2cr1` 回读不符 | hyp 拒绝 | 走 hyp/SCM 接口 |

## 日志取证

```
adb exec-out tail -c 4198400 /dev/block/by-name/userdata > D:\K20iOS\sweet-log-t43.bin
python D:\ios-raphael\tools\sweet-log.py D:\K20iOS\sweet-log-t43.bin D:\K20iOS\sweet-log-t43.txt
grep -a "SWEET GPU" D:\K20iOS\sweet-log-t43.txt
```

## 回退

```
fastboot boot raphael-t42-ctx1-debug.img
fastboot boot raphael-t39-rebase-debug.img
```

## 结果（2026-10-26 02:00 前后）

- 正式 t43（`gpursc=0`）：ring 211,637 B / flush #89，**系统健康**；`rsc wake skipped`；
  config group 1/2/3 全部写出；CM3 释放后 `fw_init=0`、GMU 日志仍全 0。
- 补充实验（repack `gpuclk=0`，bi_tcxo 19.2 MHz）：同样 `fw_init=0`；且 0106 的 ROOT_OFF 清除
  本身没生效（`gmu_rcgr` 仍 0x80000000），所以这个实验不构成“时钟不是问题”的反证。
- **FSR 纠正**：vendor `arm-smmu-regs.h` 里 `FSR_TLBLKF = bit6`，而 0x400 是 **bit10**，
  不在 `FSR_FAULT` 掩码内（bits 1–8, 30–31）。→ CB1 并没有记录翻译故障，SMMU 路径是好的。
- **根因线索（vendor `adreno_a6xx_gmu.c`）**：`a6xx_gmu_fw_start` 在加载 GMU 固件前先调用
  `_load_gmu_rpmh_ucode()`（RSC 5 字序列 + PDC 5 字序列 + TCS1/TCS3 + PDC enable），
  冷启动时 `a6xx_rpmh_power_on_gpu()` 因 `GMU_RSCC_SLEEP_SEQ_DONE` 未置位而**直接返回**。
  我们此前既没加载 ucode，又盲目做了 RSC 唤醒——顺序完全反了。
