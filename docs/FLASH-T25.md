# t25（r35）刷写与判读 · GPU stage 1a：CX 半区上电 + GBIF

> **r35 说明（2026-10-24）**：t25 = t24 底座（0001-0103 全保留）+ **0104-raphael-gpu-power**。
> t24 实测（stage 0）：GPUCC 可从 XNU 访问、GCC `gpu_cfg_ahb` 写入生效、CX/GX GDSC 均为 collapsed、全部 GPU 时钟关闭、读 GPUCC 不楔死总线。
> 本轮把 **GPU 的 CX 半区**按厂商 `gdsc-regulator` 的时序打开，并读 GBIF 作为"CX 半区真的活着"的证据。**不碰 GMU 时钟源、不加载固件、不碰 3D 核（GX）**——那些是 stage 2。

## 本轮改了什么（0104，sweet_gpu.c +102 行）

1. 读 `GPUCC_CX_GDSCR`(0x106c)；若 bit1（硬件控制）被置位则放弃（不是我们能操作的开关）；
2. 清 SW_COLLAPSE(bit0)，轮询 `GPUCC_CX_HW_CTRL`(0x1540) bit31 最多 100 ms；
3. 若 3D 核 `GX_GDSCR` 意外已有电（上一个内核留下的脏状态）→ 立即停手（HackiOS 实测这种状态不可用且关它会挂机）；
4. 使能 CX 半区五个分支：`ahb`(0x1078)、`crc_ahb`(0x107c)、`cx_apb`(0x1088)、`cxo`(0x109c)、`cxo_aon`(0x1004)，逐个回读；
5. 读 GBIF（0x2c0f000）的 `scache_cntl0` / `halt` / `halt_ack`，全 0xffffffff 判 FAIL；
6. `gpupower=0` 可退回 stage 0（只读探针）。

SM8150 的 `gpu_cc_ahb_clk` 是 `BRANCH_HALT_DELAY`（CLK_OFF 位不可信），所以 AHB 不以其状态位判成败，GBIF 的读取才是真正证据。

## 刷写

```
fastboot boot raphael-t25-gpupower-clean.img   # 日常验证
# 或
fastboot boot raphael-t25-gpupower-debug.img   # 排障：屏幕控制台常开
```

## 判读树

开机约 30 秒后（stage 0 的 12 行之后）：

```
SWEET GPU: verdict stage0=OK gpucc=responding cx=off gx=off
SWEET GPU: stage1: clearing CX GDSC collapse (0x222001 -> 0x222000)
SWEET GPU: stage1: cx_gdscr=0x80000001 cx_hw_ctrl=0x80005ff4
SWEET GPU: stage1: CX is on
SWEET GPU: stage1: ahb=0x1 off-bit not trustworthy
SWEET GPU: stage1: crc_ahb=0x1 running
SWEET GPU: stage1: cx_apb=0x1 running
SWEET GPU: stage1: cxo=0x1 running
SWEET GPU: stage1: cxo_aon=0x1 running
SWEET GPU: stage1: mapping GBIF 0x2c0f000
SWEET GPU: stage1: gbif scache_cntl0=0x... halt=0x0 halt_ack=0x0
SWEET GPU: stage1=OK cx=on ahb=0x1 gbif=responding (GMU clock/firmware is stage 2)
```

| 现象 | 含义 | 结论 |
|---|---|---|
| `stage1: CX is on` + `gbif=responding` | CX 半区上电、寄存器和总线接口活了 | **stage 1a 全胜** ✅ |
| `stage1=FAIL CX does not come on` | 开关没起来（父轨/时序问题）| 回报 `cx_gdscr/cx_hw_ctrl` 两行，stage 2 前先解决供电 |
| `the CX switch is under hardware control` | bit1 被置位，固件在管这个开关 | 回报该行，改走硬件控制路径 |
| `the 3D core has power already` | 上一次运行留下的 GX 脏状态，HackiOS 实测不可用 | 重启手机再试；回报 `gx_gdscr` 值 |
| `stage1=FAIL <clk> does not come on` | 某个分支时钟没起来 | 回报整段 stage1 行 |
| `stage1=FAIL ... gbif=all-ones` | CX 上电但 GBIF 无应答 | 回报整段日志 |
| 卡死在 `clearing CX GDSC collapse` 之后 | 极少见（父轨关着时操作开关）| 强制重启，抓日志尾行 |

> 判读核心：`stage0=OK` 说明寄存器可达；`stage1=OK` 说明**GPU 的 CX 半区真实带电**。只有到这一步，stage 2（GCC GPU 时钟 + GMU 时钟源 + a640_gmu.bin 固件 + HFI + SMMU + GX 上电）才有意义。

## 日志取证（不变）

```
adb exec-out "tail -c 4198400 /dev/block/by-name/userdata" > D:\K20iOS\sweet-log.bin
```

（Git Bash/CMD 均可；文件应正好 4,198,400 字节，前 8 字节为 `SWEETLOG`。）

## 本轮重点抓取清单

1. `SWEET GPU: stage1:` 开头的**全部行**；
2. `stage1=OK` 或 `stage1=FAIL` 判决行；
3. 若卡死：最后 10 行（`clearing CX GDSC collapse` 之后有没有输出）。

## 回退

```
fastboot boot raphael-t24-gpuprobe-clean.img   # stage 0（只读）
fastboot boot raphael-t23-touchfix-clean.img   # 上一轮功能版
fastboot boot raphael-t12-gt9886-clean.img     # 稳定版 r28
```

## 下一轮（stage 2）预告

GCC：`gpu_memnoc_gfx` / `gpu_snoc_dvm_gfx` / `ddrss_gpu_axi` 三时钟 + GPLL0 分频 → GPUCC `gmu_clk_src` 200 MHz + `cx_gmu` 分支 → 读 GMU 19.2 MHz 自走计数器（活体证据）→ 再谈 a640_gmu.bin + HFI + SMMU + GX。

## 文件校验

| 文件 | 大小 | SHA256 |
|---|---|---|
| `raphael-t25-gpupower-clean.img` | 17,358,959 B | `31671eed4435c1859cb170bbe38430b649c817c4d7beb221cb11adc4a5cee379` |
| `raphael-t25-gpupower-debug.img` | 17,358,959 B | `183df83cf0fcf57d0298012d879f707f781f20dda059832d6c0a1be4d45c1aab` |

内核：`kernel.development.raphael` 17,256,352 B / `ea3baa9eb12f2af18aca7f2d203d476b9af38c8e70d2e32fdbda19eed5a819c0`（CI run 40103285547）。
构建：r33 clean 模板（shim/DTB/ADT/cmdline 逐字节复用）；clean/debug 仅差 `sweetfb_noconsole` 1 字节（镜像偏移 0x33172）。
