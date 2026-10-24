# t26（r36）刷写与判读 · GPU stage 1b：GBIF 证据（放宽 Cx 时钟判据）

> **r36 说明（2026-10-24）**：t26 = t25 底座（0001-0104）+ **0105-raphael-gpu-gbif**。
> t25 实测已证明最关键的一步：`cx_gdscr 0x222001 -> 0xf8222000`（bit31=1）、`cx_hw_ctrl=0x80001fe0`（bit31=1）、`stage1: CX is on`、`ahb=0x1 running`。
> 但 `crc_ahb=0`、`cx_apb=0x80000000`（enable 位写不进去）被上一轮的 `must=true` 判成失败并中止，GBIF 没读到。本轮只做一件事：**放宽这两个硬件门控时钟的判据，先 CXO 后其余，然后无条件读 GBIF**，拿到“CX 半区真的活了”的最后证据。

## 本轮改了什么（0105，13 增 14 删）

- 时钟顺序改为 `cxo_aon → cxo → ahb → crc_ahb → cx_apb`（CXO 是其余分支的时钟源）；
- `crc_ahb`/`cx_apb` 不再致命：SM8150 上它们由硬件控制，enable 位不可写（t25 实证），只记录状态；
- 时钟段之后**无条件读 GBIF**（0x2c0f000）的 `scache_cntl0/halt/halt_ack`；
- 判决行 `stage1=OK/FAIL ... gbif=responding/all-ones`。

## 刷写

```
fastboot boot raphael-t26-gbif-clean.img
# 或
fastboot boot raphael-t26-gbif-debug.img
```

## 判读树

开机约 30 秒后：

```
SWEET GPU: stage1: clearing CX GDSC collapse (0x222001 -> 0x222000)
SWEET GPU: stage1: cx_gdscr=0xf8222000 cx_hw_ctrl=0x80001fe0
SWEET GPU: stage1: CX is on
SWEET GPU: stage1: cxo_aon=0x1 running
SWEET GPU: stage1: cxo=0x1 running
SWEET GPU: stage1: ahb=0x1 running
SWEET GPU: stage1: crc_ahb=0 not-running (hw-gated or no status bit)
SWEET GPU: stage1: cx_apb=0x80000000 not-running (hw-gated or no status bit)
SWEET GPU: stage1: mapping GBIF 0x2c0f000
SWEET GPU: stage1: gbif scache_cntl0=0x... halt=0x... halt_ack=0x...
SWEET GPU: stage1=OK cx=on ahb=... gbif=responding
```

| 现象 | 含义 | 结论 |
|---|---|---|
| `CX is on` + `gbif=responding` | CX 域上电、总线接口真实应答 | **stage 1 完成** ✅，下一轮做 GMU 时钟/firmware |
| `gbif=all-ones` | CX 上电但 GBIF 不应答 | 回报 GBIF 三值和 ahb/cxo 行 |
| `CX does not come on` | GDSC 时序失败（父轨/硬件控制）| 回报 cx_gdscr/cx_hw_ctrl |
| `the 3D core has power already` | 上次运行留下的 GX 脏状态 | 重启重试，回报 gx_gdscr |
| 卡死在 `mapping GBIF` 之后 | 极少见：CX 上电但 AHB 实际没跑 | 强制重启，回报最后几行 |

## 日志取证

```
adb exec-out "tail -c 4198400 /dev/block/by-name/userdata" > D:\K20iOS\sweet-log.bin
```

## 回退

```
fastboot boot raphael-t25-gpupower-clean.img   # CX 上电轮（t25）
fastboot boot raphael-t24-gpuprobe-clean.img   # stage 0 只读
fastboot boot raphael-t12-gt9886-clean.img     # 稳定版
```

## 下一轮（stage 2）

GCC GPU 三时钟（memnoc/snoc/ddrss）→ GPUCC `gmu_clk_src` 200 MHz + `cx_gmu` 分支 → GMU 19.2 MHz 自走计数器（活体证据）→ `a640_gmu.bin` + HFI + SMMU → GX 上电读 RBBM。
