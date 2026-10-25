# t38（r48）刷写与判读 · 跳过 stage 2 的 GMU 读取，给 stage 6 留日志窗口

> **r48（2026-10-25）**：t38 = t37 底座 + **0117-raphael-gmu-read-gate**。
> t37 的隔离实验（`gpugmu=0`，只跑 stage 0/1）完整打印、系统不挂（flush #63）→ **挂点锁定在 stage 2 的 GMU 部分**：读取 0x7e0xx 的 GMU 寄存器会把 NoC 挂死（t36/t37 都在这里）。
> 之前 t35 没挂，是因为它读的是 a660 的错误偏移（空地址）。

## 本轮改了什么（0117，+8/-1）

1. stage 2 的 GMU 寄存器读取**默认关闭**（`gpuread=0`）；需要时用 boot-arg `gpuread=1` 打开；
2. stage 6 在写 GMU 启动寄存器前增加 **11 秒日志窗口**（`stage6: GMU registers in 11 s`），并在写完配置后打印 `GMU config registers written`——即使随后挂死，落盘日志也能定位到具体阶段；
3. 其余（a640 寄存器表、时钟、TCM、启动序列）保持不变。

## 刷写

```
fastboot boot raphael-t38-gate-clean.img
# 或
fastboot boot raphael-t38-gate-debug.img
```

## 判读树

```
stage2: GMU registers not read (gpuread=0); firmware is stage 3
stage4=OK smmu=responding ...
stage5=OK fw=loaded tcm_wrong=0 ddr_blocks=2 ...
stage6: GMU registers in 11 s (flush window before the writes)
stage6: GMU config registers written
stage6: GMU configured ..., releasing CM3
stage6: fw_init=...
```

| 现象 | 含义 | 结论 |
|---|---|---|
| 日志走到 `GMU config registers written` 且 `fw_init=0xbabeface` | GMU 写寄存器/启动均可用 | **stage 6 完成** |
| 停在 `GMU registers in 11 s` 之后（无 written）| 某条 GMU **写**挂死 | 回报最后一行；下一步逐条写并打印 |
| 停在 `releasing CM3` 之后 | 复位释放/固件执行挂死 | 回报 stage6 行；查固件/SMMU |
| 仍未到 stage2 的 skip 行 | 挂点更早（stage 0/1）| 回报最后一行 |
| `stage5` 行不出现 | stage 3/4/5 早期问题 | 回报最后一行 |

## 日志取证

```
adb exec-out "tail -c 4198400 /dev/block/by-name/userdata" > D:\K20iOS\sweet-log.bin
```

## 回退

```
fastboot boot raphael-t37-earlyclk-clean.img
fastboot boot raphael-t35-clk-clean.img
fastboot boot raphael-t12-gt9886-clean.img
```
