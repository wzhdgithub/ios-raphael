# t42（r52）刷写与判读 · GMU 用 context 1，流表项 1 + 5

> **r52（2026-10-26）**：t42 = t41 底座 + **0121-raphael-smmu-context-entry**。

## 本轮改了什么（0121，+26/-10）

1. **S2CR 改回 `1`**：TYPE[17:16]=0（翻译）、CBNDX[7:0]=1（GMU 的 context 1）；
2. **同时写流表项 1 和 5**：HackiOS 把 GMU 的 stream 5/mask 0x400 放在 **entry 1**（entry 0 留给 3D 核），
   我们此前只写 entry 5；两处都写，谁被 hypervisor 接受都能让 GMU 走 context 1；
3. TLBSYNC 后**轮询 TLBSTATUS**（bit0 清零）并打印，确认旧翻译已失效；
4. 打印 entry1/entry5 的 SMR/S2CR 回读值，直接看 hypervisor 是否接受写入；
5. 保留 0120 的 QSMMU safe-toggle、RSC 唤醒、TCU 只读探测、GMU 日志页/FSR 转储。

## 刷写

```
fastboot boot raphael-t42-ctx1-debug.img
```

cmdline：`gpuread=1 ufslogsecs=1`。

## 判读树

```
stage5: qsmmu safe-toggle status=0
stage5: tcu xxxxxxxx ...
stage5: smmu cb1@0x9000 ttbr=... tlbstatus=0
stage5: smr1=0x80040005 s2cr1=0x1 smr5=0x80040005 s2cr5=0x1 (want ...)
stage5=OK fw=loaded ...
stage6: rsc status=... rscc_ack=...
stage6: GMU config registers written
stage6: gmu log wptr=... smmu_fsr=...
stage6: fw_init=0xbabeface
```

| 现象 | 含义 | 结论 |
|---|---|---|
| `fw_init=0xbabeface` | CM3 固件运行 | **stage 6 完成**，进入 HFI |
| `s2cr1=0x1` 且 fw_init=0 | 翻译已配置但 CM3 仍不跑 | 看 gmu log / rsc / safe-toggle |
| `s2cr1=0` 或读回不符 | hypervisor 没接受写入 | 需要 hyp/SCM 的 S2CR 接口 |
| `tlbstatus` 非 0 | TLB 同步不完成 | QSMMU TLB 仍被锁 |

## 日志取证

```
adb exec-out tail -c 4198400 /dev/block/by-name/userdata > D:\K20iOS\sweet-log-t42.bin
python D:\ios-raphael\tools\sweet-log.py D:\K20iOS\sweet-log-t42.bin D:\K20iOS\sweet-log-t42.txt
grep -a "SWEET GPU" D:\K20iOS\sweet-log-t42.txt
```

## 回退

```
fastboot boot raphael-t39-rebase-debug.img
fastboot boot raphael-t38-gate-debug.img
fastboot boot raphael-t12-gt9886-clean.img
```

## 结果（2026-10-26 01:30，开 ~3.5 分钟后拉取）

ring：**total 93,442 B / flush #20**，最后一行停在 `stage6: GMU registers in 11 s`。

```
stage5: qsmmu safe-toggle status=0 answer=0          <- SCM 调用成功
stage5: tcu 00000000 00000000 00000000 00000000 01000000 00000000 01000000 00000000
stage5: smmu cb1@0x9000 ttbr=0x100014983c000 tlbstatus=0
stage5: smr1=0x84000005 s2cr1=0x1 smr5=0x84000005 s2cr5=0x1 (want ...)
stage5=OK fw=loaded tcm_wrong=0 ddr_blocks=2 CM3=HELD_IN_RESET
stage6: hfi table 210 words, 3 queues ...
stage6: rsc status=0 rscc_ack=0
stage6: rsc wake requested ack=0                     <- ACK 没来
stage6: GMU registers in 11 s (flush window before the writes)   <- 之后系统死亡
```

**结论**：
1. **S2CR/流表项修复完全正确**：SMR/S2CR 回读与写入一致（`0x84000005` / `0x1`），TLBSTATUS=0；
   hypervisor 接受 entry 1 与 entry 5 的写入，GMU 的 stream 5 现在走 context 1。
2. QSMMU safe-toggle SCM 调用成功（返回 0/0）。
3. 系统死在 11 秒窗口内，**配置写和 CM3 释放都没执行到**；窗口前唯一的新动作是
   **RSCC_CONTROL_REQ=2 唤醒请求（ack=0）**——极可能是它让 RSC/NoC 挂死（我们还没有
   配置 PDC 序列，vendor 是在 `a6xx_gmu_rpmh_init` 配好 PDC 之后才唤醒 RSC 的）。
4. t43/0122：RSC 唤醒改为 `gpursc=0` 默认关闭；配置写分三组并在每组后 1.5 s 落盘窗口。
