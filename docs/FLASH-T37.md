# t37（r47）刷写与判读 · 把 GMU 200 MHz 时钟提前到任何 GMU 读取之前

> **r47（2026-10-25）**：t37 = t36 底座 + **0116-raphael-gmu-clock-early**。
> t36（a640 正确寄存器表）出现回归：探针在 30 秒唤醒后整机挂死、日志不再落盘。原因已定位：**stage 2 在 200 MHz 时钟还没配好时就去读 0x7e0xx/0x7e2xx 的 GMU 寄存器**，而 `gmu_clk_src` 的 RCG 仍是 root-off → GMU 寄存器访问把 NoC 挂死。t35 没事是因为它读的是 a660 的错误偏移（空地址）。
> t35 对照已确认环境与线程调度正常（5 秒能打印完整 stage 0）。

## 本轮改了什么（0116，+26）

- 把 200 MHz 时钟配置（GCC `0x52004` bit16 → RCG `ROOT_EN` → `CFG=0x602` → `UPDATE` → 清 force-enable）从 stage 6 **提前到 stage 2**，位置在使能 `cx_gmu`/读取任何 GMU 寄存器之前；
- stage 6 保留同样的幂等配置（重复写无害）；
- 新增日志 `stage2: gmu clock cmd=... cfg=... (200 MHz)`。

## 刷写

```
fastboot boot raphael-t37-earlyclk-clean.img
# 或
fastboot boot raphael-t37-earlyclk-debug.img
```

## 判读树

```
stage2: GCC GPU clocks
stage2: gmu clock cmd=0x0 cfg=0x602 (200 MHz)
stage2: gmu_rcgr=0x0 src=0x0
stage2: cx_gmu=0x4221 running
stage2: gmu sysreset=0x1 ... counter=19200000 Hz
stage2=OK gmu=responding ...
stage5=OK fw=loaded ...
stage6: fw_init=0xbabeface → stage6=OK fw=running
```

| 现象 | 含义 | 结论 |
|---|---|---|
| stage2 全行 + `counter=19200000 Hz` | GMU 时钟/寄存器读取正常，回归修复 | 继续看 stage5/6 |
| 卡死在 `stage2: gmu clock` 之后 | 仍是 GMU 访问时序问题 | 回报最后一行 |
| `stage2: gmu_rcgr=0x80000000` | RCG root 仍未开 | 回报 cmd/cfg 行 |
| `stage6: fw_init=0xbabeface` | **GMU 固件启动** | stage 6 完成 |
| `fw_init=0`（20 s）| CM3 仍未执行 | 回报 stage5 窗口行 + stage6 行 |

## 日志取证

```
adb exec-out "tail -c 4198400 /dev/block/by-name/userdata" > D:\K20iOS\sweet-log.bin
```

## 回退

```
fastboot boot raphael-t35-clk-clean.img
fastboot boot raphael-t34-diag-clean.img
fastboot boot raphael-t12-gt9886-clean.img
```
