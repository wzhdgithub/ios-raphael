# t35（r45）刷写与判读 · GMU 时钟拉到 200 MHz，再启动 CM3

> **r45（2026-10-25）**：t35 = t34 底座 + **0114-raphael-gmu-clock-200m**。
> t34 debug 实机给出了决定性证据：CM3 复位已释放（`sysreset=0`）、BOOT_CONFIG/CM3_CFG 都写进去了（`0x2 / 0x4052`），但 20 秒内 `fw_init` 始终为 0 —— **CM3 没有时钟，固件无法执行**。根因：`gpu_cc_gmu_clk_src` 的 RCG root 一直 off（`gmu_rcgr=0x80000000`），`cx_gmu` 分支的“running”是假状态位。

## 本轮改了什么（0114，+31）

在释放 CM3 之前，按 `gpucc-sm8150`/`clk-rcg2` 的厂商序列把 GMU 时钟拉起来：

1. GCC `0x52004` 置 bit16（`gcc_gpu_gpll0_div_clk_src` 门，父源 GPLL0/2 = 300 MHz）；
2. GPUCC `CMD_RCGR(0x1120)` 置 `ROOT_EN`(bit1)，轮询 `ROOT_OFF`(bit31) 清除；
3. `CFG_RCGR(0x1124) = 0x602`（源 6 = GPLL0_OUT_MAIN_DIV，div 1.5 → **200 MHz**）；
4. `CMD_RCGR` 置 `UPDATE`(bit0)，轮询其清除；最后清 `ROOT_EN`（force-enable 退出，root 保持开启）；
5. 回读并打印 `gmu rcg before/after cmd/cfg`；`gpuclk=0` 可跳过。

## 刷写

```
fastboot boot raphael-t35-clk-clean.img
# 或
fastboot boot raphael-t35-clk-debug.img
```

## 判读树

```
stage6: gcc 0x52004=0x... (gpll0_div gate)
stage6: gmu rcg before cmd=0x80000000 cfg=0x0
stage6: gmu rcg after  cmd=0x0 cfg=0x602
stage6: GMU configured (chip 0x6040000), releasing CM3
stage6: fw_init=0x100 ...
stage6=OK fw=running
```

| 现象 | 含义 | 结论 |
|---|---|---|
| `rcg after cmd` 的 bit31=0、`cfg=0x602`，且 `fw_init=0x100` | **时钟通了、GMU 固件跑起来** | stage 6 完成，进 stage 7（HFI/PDC/GX）|
| `rcg after` 仍 `cmd=0x80000000` | RCG 写被忽略（父源/保护）| 回报 before/after 与 `0x52004` 值；改查 GPLL0 父路径 |
| `cmd` 清了但 `fw_init` 仍 0 | 时钟已给但固件未执行 | 回报 rcg 两行 + fw_init；查固件入口/BOOT_CONFIG |
| `fw_init & 0xe00` | 固件运行并报错 | 回报 fw_init 与版本行 |
| 卡死 | 固件跑飞 | 强制重启，回报最后几行 |

## 日志取证

```
adb exec-out "tail -c 4198400 /dev/block/by-name/userdata" > D:\K20iOS\sweet-log.bin
```

## 回退

```
fastboot boot raphael-t34-diag-clean.img
fastboot boot raphael-t33-boot-clean.img
fastboot boot raphael-t12-gt9886-clean.img
```
