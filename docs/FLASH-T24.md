# t24（r34）刷写与判读 · GPU 探针 stage 0（只读）

> **r34 说明（2026-10-24）**：t24 = t23 底座（0001-0102 全保留）+ **0103-raphael-gpu-probe**。
> 本轮不是"GPU 已经能跑"，而是回答第一个问题：**XNU 能不能碰到 SM8150 的 GPU 子系统**。
> 参考实现是 HackiOS（OOSexGang，2026-10-22 开源）的 Adreno 驱动：内核里自建 GPUCC/GMU/SMMU + Mesa/freedreno，Metal 覆盖率 321/456。那份代码是 SM7325（yupik）的，raphael 要按 SM8150 重写——先做探针，再谈上电。

## 本轮改了什么

- **0103（新文件 bsd/dev/sweet_gpu.c，273 行）**：开机约 30 秒后（boot-arg `gpuprobedelay` 可调）在一个独立线程里：
  1. 读 GCC `gpu_cfg_ahb`（0x71004）；若未运行就置 enable 位并轮询——这是让 GPUCC 寄存器文件应答的时钟；
  2. 读 GPUCC（0x2c90000）的电源开关/时钟分支/GMU 时钟源，**全部只读**；
  3. 仅当 CX 已上电且 AHB 时钟真的在跑，才读 GBIF（0x2c0f000）；仅当 `gpu_cc_cx_gmu_clk` 也在跑，才读 GMU（0x2c6a000）的身份寄存器和 19.2 MHz 自走计数器。
- 3D 核（0x2c00000，GX 域）**一次都不碰**——GMU 固件没加载前它没有电。
- `gpuprobe=0` 关闭探针；默认开启。

## 地址依据（全部来自 raphael 官方源码）

| 块 | 地址 | 依据 |
|---|---|---|
| 3D core（Adreno 640，chipid 0x06040000）| 0x02c00000 | sm8150-gpu.dtsi `kgsl-3d0` |
| GBIF（CX 半区）| 0x02c0f000 | 3D base + 4×0x3c00（a6xx.xml）|
| GMU | 0x02c6a000 | sm8150-gpu.dtsi `qcom,gmu` |
| GPUCC | 0x02c90000 | sm8150.dtsi `qcom,gpucc`，0x9000 |
| CX GDSC / GX GDSC | +0x106c / +0x100c | sm8150-gdsc.dtsi（0x2c9106c / 0x2c9100c）|
| GCC gpu_cfg_ahb | 0x71004 | gcc-sm8150.c `gcc_gpu_cfg_ahb_clk` |

## 刷写

```
fastboot boot raphael-t24-gpuprobe-clean.img   # 日常验证
# 或
fastboot boot raphael-t24-gpuprobe-debug.img   # 排障：屏幕控制台常开
```

## 判读树

启动约 30 秒后（不用等锁屏），日志出现：

```
SWEET GPU: stage-0 read-only probe in 30 s (gpuprobe=0 disables)
SWEET GPU: mapping GCC 0x100000, checking gpu_cfg_ahb
SWEET GPU: gcc gpu_cfg_ahb=0x... (running)
SWEET GPU: mapping GPUCC 0x2c90000, reading clock/power state
SWEET GPU: pll0=0x... pll1=0x... cxo_aon=0x... gx_sw_reset=0x...
SWEET GPU: gx_gdscr=0x... cx_gdscr=0x... gx_gmu=0x... ahb=0x... crc_ahb=0x...
SWEET GPU: cx_snoc=0x... cx_gmu=0x... cxo=0x... gmu_rcgr=0x... src=0x...
SWEET GPU: cx_hw_ctrl=0x... gx_domain_addr=0x...
SWEET GPU: state cx=on gx=off ahb=... cx_gmu=...
SWEET GPU: verdict stage0=OK gpucc=responding cx=on gx=off (gx off is the bootloader state)
```

| 现象 | 含义 | 结论 |
|---|---|---|
| `gpu_cfg_ahb=0x1`（或 enable 后变 running） | GPUCC 寄存器时钟通了 | 第一关通过 ✅ |
| `verdict stage0=OK ... cx=on gx=off` | GPUCC 应答；CX 已上电；GX 未上电（ABL 留下的正常状态）| **探针成功**：XNU 能碰到 GPU 子系统，下一轮做 GMU 上电 |
| `cx=off` | GPUCC 能读但 CX 域没电 | 下一轮必须先做 RPMh CX 投票，再做 GMU |
| `verdict stage0=FAIL gpucc=all-ones` | 总线返回全 1，GPUCC 没应答 | 回报八行原始值；需要先补 RPMh/GDSC 上电路径 |
| `gcc gpu_cfg_ahb never came on` | GCC 分支时钟置位无效 | 回报该行 + `gx_gdscr` 行 |
| `GMU not started (cx_gmu off)` | 预期结果：驱动没加载固件 | 正常，不是失败 |
| 计数器 `19.2 MHz crystal, GPU BLOCK ALIVE` | GMU 固件恰好已被上一级引导留下并在跑 | 额外惊喜，记录 |

### 卡死怎么办

如果整机在 **30 秒左右**卡死（黑屏/无响应，日志最后一行是 `SWEET GPU: mapping GPUCC ...`），说明 GPUCC 的首次读取把 NoC 楔住了——这是 CX 电源域未上电的典型 signature，不是刷机失败：

1. 长按 **电源 + 音量下** 强制重启（`fastboot boot` 不落盘，重启即回安卓）。
2. 进 TWRP 抓日志（见下），把那最后几行发我。
3. 下一轮改法：先用 RPMh 把 CX/MX 投上去，再碰 GPUCC。

## 日志取证（不变）

```
adb shell dd if=/dev/block/by-name/userdata of=/tmp/sweet-full.bin bs=4096 skip=$(( $(blockdev --getsize64 /dev/block/by-name/userdata) / 4096 - 1026 )) count=1026
```

（TWRP/OrangeFox 下可用 `tail -c 4198400 /dev/block/sda31 > /tmp/sweet-log.bin`，再 `adb pull`。）

## 本轮日志重点抓取清单

1. `SWEET GPU:` 开头的**全部行**（八行原始寄存器 + verdict）。
2. 若卡死：日志最后 10 行（尤其是 `mapping GPUCC` 之后有没有任何输出）。
3. 顺带确认 30 秒前后触摸/亮度/充电没有回归（0103 不碰它们）。

## 回退

```
fastboot boot raphael-t23-touchfix-clean.img   # 上一轮（触摸重试阶梯）
fastboot boot raphael-t12-gt9886-clean.img     # 稳定版 r28
```

## 下一轮（stage 1）预告

在探针证明 GPUCC 可访问后：CX/MX RPMh 投票 → GPUCC 分支时钟 → GMU 固件（a640_gmu.bin）+ HFI → SMMU 页表 → 3D 核上电读 RBBM。每一步都会先加只读/回读校验，沿用 t19 的"降级中止"原则：失败就停在锁屏，不重启循环。
