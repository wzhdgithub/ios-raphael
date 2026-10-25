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

## 运行记录（2026-10-25 夜）

| 次 | 时间 | 结果 |
|---|---|---|
| run 1 | 17:43 刷入 | `Booting OKAY` 但内核没跑起来；17:45 拉到的还是 t37 `gpugmu=0` 的旧环形缓冲（无 0117 标记）|
| run 2 | 17:47 刷入 | 内核真跑了（新 ring，Darwin 22.6.0，total 129,750 B / flush #3），但开机约 50 秒就进了 TWRP：ring 停在第 1926 行，正好是 nogmu 日志里 stage 0 的起始行 → **GPU 线程 30 秒定时器还没到，不是 GPU 挂** |
| run 3 | 17:50:34 刷入 | **仍挂**：17:54:41（开机 4 分钟）拉到的 ring 只有 total 123,041 B / flush #3，最后一行在第 1862 行；健康日志（gpugmu=0）里 stage 0 从第 1915 行才开始 → 落盘内容在 30 秒唤醒处停住，GPU 线程的 stage 0 打印没来得及 flush。排除拉取时机因素 |

**结论（run 3）**：把 stage 2 的 GMU 寄存器读取关掉**并没有**解决挂死。t38 = t37 底座 + 0117，stage 2 仍然执行 GCC GPU 时钟、GPLL0 门控、GMU RCG（ROOT_EN/CFG=0x602/UPDATE）、清 ROOT_OFF、使能并轮询 cx_gmu；这些里至少有一个在唤醒时把 NoC 挂死。ring 在 wedge 之后 flush 不了，所以看不到最后一行——必须用 **debug 镜像的屏幕控制台**定位最后打印。

**下一步（不需要重新编译）**：再次 `fastboot boot raphael-t38-gate-debug.img`，从开机 ~30 秒起盯住屏幕，记录最后一行 `SWEET GPU:` 文本（wedge 后屏幕会冻结）。对照：

| 屏幕最后一行 | 含义 |
|---|---|
| `stage1=OK ...` | 挂在 stage 2 之前/入口（不太可能）|
| `stage2: GCC GPU clocks` 或某条 `stage2: 0x...=...` | GCC GPU 时钟使能挂死 |
| `stage2: gmu clock cmd=... cfg=...` 之后 | RCG 的 ROOT_EN/CFG/UPDATE 挂死 |
| `stage2: gmu_rcgr=...` / `clearing CMD_ROOT_OFF` 之后 | ROOT_OFF 清除挂死 |
| `stage2: cx_gmu=... running` 之后 | cx_gmu 使能/轮询挂死（或 0117 的 skip 打印没来得及）|

若屏幕方案不可行：写 0118 给 stage 2 每个动作加细粒度打印 + 500 ms 间隔（只加打印，不改行为），再按屏幕冻结位置定位；或用 boot-arg 逐段跳（需要先加 `gpu2clk` 之类的新开关，现有 `gpuclk=0` 不是干净对照——它跳过 RCG 后仍会走清 ROOT_OFF 的写）。

**关键教训**：`stage-0 ... in 30 s` 这行在 ring 里出现在第 ~161 行（早于 launchd），但 stage 0 的输出要到第 ~1915 行（SpringBoard/backboardd 之后）才出现——线程确实睡满 30 秒。**开机后不到 ~90 秒进 TWRP 会得到一份"看起来什么都没跑"的日志**；但 run 3 证明等 4 分钟也一样，因为 wedge 发生在打印前、flush 被冻结。判断"是否 wedge"要看 ring 的 total/flush 是否在启动早期就停滞，而不是看开了多久。

## 手工取证（若后台 watcher 已随旧会话退出）

```
adb devices                      # 必须显示 recovery
adb exec-out tail -c 4198400 /dev/block/by-name/userdata > D:\K20iOS\sweet-log-t38-final.bin
python D:\ios-raphael\tools\sweet-log.py D:\K20iOS\sweet-log-t38-final.bin D:\K20iOS\sweet-log-t38-final.txt
grep -a "SWEET GPU" D:\K20iOS\sweet-log-t38-final.txt
```

判定这份日志是不是 run 3 的：

1. 头部解码行 `# version 1, ring ..., total ..., flush #N` 的 total 必须大于 run 2 的 129,750，或
2. 含 0117 唯一标记之一：`stage2: gmu clock` / `stage2: GMU registers not read` / `stage6: GMU registers in 11 s` / `stage6: GMU config registers written`。

两者都没有 → 还是旧 ring，重新 `fastboot boot raphael-t38-gate-debug.img` 并等满 3 分钟。

## 回退

```
fastboot boot raphael-t37-earlyclk-clean.img
fastboot boot raphael-t35-clk-clean.img
fastboot boot raphael-t12-gt9886-clean.img
```

## 结案（2026-10-25 18:20）——不是 NoC 挂死，是 **data abort panic**

run 3 用同一镜像重放后，屏幕冻结在 panic：

```
panic(cpu 4 caller ...): Kernel data abort. at pc 0xfffffe00074fc6ec, lr 0xfffffe00074fc6d4
```

用 CI artifact 里的 `kernel.development.raphael` 符号表解析：

| 地址 | 符号 |
|---|---|
| pc `0xfffffe00074fc6ec` | `gpu_thread + 0x1268`（`sweet_gpu.c:835`） |
| lr `0xfffffe00074fc6d4` | `gpu_thread + 0x1250`（`sweet_gpu.c:831`） |

`sweet_gpu.c:835` 是 **stage 5 的第一条 GMU 寄存器写**：

```c
gpu_wr(gmu5, GMU_CM3_SYSRESET, 1);
```

**根因**：0115 的“a640 寄存器表”写的是**相对 GPU 基址 0x2c00000** 的偏移（SYSRESET=0x7e000），
而 `gmu5` 是 `ml_io_map(0x2c6a000, 0x25000)` 的 GMU ioremap。`0x2c6a000 + 0x7e000 = 0x2ce4000`
落在映射之外 → 翻译错误 → data abort panic。旧 a660 表的值 `0x14000` 恰好满足
`0x2c00000 + 0x7e000 == 0x2c6a000 + 0x14000 == 0x2c7e000`，**旧偏移本来就是对的**
（t27/t28 日志可证：`stage2: gmu sysreset=0x1 ... counter=21197600 Hz` 是真实 GMU 寄存器）。

0115 真正有效的修正只有固件内存布局：DTCM VA 0x40000、ICACHE/DCACHE 走 SMMU、`ddr_blocks=2`、
`0xBABEFACE` 启动判定；寄存器偏移那一整块是多余且致命的 −0x6a000 平移。

**结论**：t38 的“GMU 写挂死”分支不成立；0117 的读闸门也没有意义——panic 发生在第一次访问
真正的 GMU CSR 时，而地址根本不在映射内。t39 用 0118 rebase 修复（见 `FLASH-T39.md`）。

注意：t35 之前“stage5/6 正常打印”用的是 GMU 相对偏移（0x14000，合法），所以不 panic；
但它把 DTCM 固件块错误地放进 SMMU 内存，CM3 自然起不来。两个 bug 叠加，互相掩盖。

