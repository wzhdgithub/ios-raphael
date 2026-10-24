# t30（r40）刷写与判读 · SMMU 被 hypervisor 仿真：改用 SCM IO

> **r40（2026-10-25）**：t30 = t29 底座 + **0109-raphael-gpu-smmu-scmio**。
> t29 实机：SMMU 的 IDR 布局完全正确（4K 页、8 页全局寄存器、5 context），但 `SMR=0 / S2CR=垃圾`，且**直接读 GR1 的 CBAR(0x1000) 会让 GPU 线程卡死**（系统其余部分继续）。
> 厂商 `arm-smmu.c` 的注释给出根因：**这类平台的 SMMU 全局寄存器被 hypervisor 捕获并仿真**，部分寄存器必须走 **SCM IO 调用**（`SCM_SVC_IO=0x5`，read=1 / write=2）。t28 已验证我们的 SCM 通道可用。

## 本轮改了什么（0109，+42/-9）

- 新增 `gpu_scm_io_read(phys, &status)`：`SCM_FAST|SCM_64|SCM_SIP|SCM_CALL(5,1)`，参数=物理地址；
- stage 4 改为：直接读 IDR/SMR/S2CR（保留）→ **SCM IO 读 IDR0 交叉验证** → SCM IO 逐个读 CBAR[0..4] 与 context 0 的 SCTLR/TTBR0/TCR/FSR；
- 判决 `stage4=OK/PARTIAL ... scm_io=working/refused`；不再直接触碰 GR1（避免卡死）。

## 刷写

```
fastboot boot raphael-t30-scmio-clean.img
# 或
fastboot boot raphael-t30-scmio-debug.img
```

## 判读树

```
SWEET GPU: stage4: smmu idr0=0x4c017e06 ...（直接读）
SWEET GPU: stage4: scm_io read idr0 status=0 value=0x4c017e06 (direct 0x4c017e06)
SWEET GPU: stage4: scm_io cbar[0] status=0 value=0x...
...
SWEET GPU: stage4: scm_io cb0 sctlr=... ttbr0=... tcr=... fsr=...
SWEET GPU: stage4=OK smmu=responding matches=6 contexts=5 scm_io=working
```

| 现象 | 含义 | 结论 |
|---|---|---|
| `scm_io=working`，CBAR/CB0 有值 | hypervisor 接受 SCM IO，SMMU 可编程 | **stage 4 完成**，下一轮写 GMU 上下文与页表 |
| `scm_io read idr0 status≠0 value=…` | SCM IO 被拒（地址白名单/权限）| 回报 status；改走 TBU/其它接口 |
| `scm_io=refused` | 后续寄存器不可读 | 同上；保留直接可读的 IDR/SMR/S2CR 信息 |
| 再次卡死在某个 scm_io 之后 | 该地址的 SCM 调用不返回（极罕见）| 强制重启，回报最后一行地址 |
| 之前 t29 的直接 CBAR 卡死不再出现 | 我们已不再直接读 GR1 | 预期 |

## 日志取证

```
adb exec-out "tail -c 4198400 /dev/block/by-name/userdata" > D:\K20iOS\sweet-log.bin
```

## 回退

```
fastboot boot raphael-t29-smmu-clean.img
fastboot boot raphael-t28-tz-clean.img
fastboot boot raphael-t12-gt9886-clean.img
```

## 下一轮（stage 3a）

若 SCM IO 可用：按读到的 CBAR/CB0 现状，为 GMU（stream 5、匹配值 1、context 1）写 4K 页表；用 SCM IO 写 CBAR/CBA2R/S2CR/SMR（或直写若安全），映射固件 DDR 段 → 装 TCM+DDR → 配置 CM3 → 释放复位 → 等 `FW_INIT_RESULT=0xbabeface`。
