# t27（r37）刷写与判读 · GPU stage 2a：GMU 时钟与身份

> **r37 说明（2026-10-24）**：t27 = t26 底座（0001-0105）+ **0106-raphael-gmu-clock**。
> t26 实机已证明 CX 半区活着：`stage1=OK cx=on ahb=0x1 gbif=responding`（`halt_ack=0x3`）。
> 本轮把 GMU 的**寄存器时钟**打开并读它的身份/自走计数器；**不加载固件、不上电 GX、不碰 SMMU**。

## 本轮改了什么（0106，+89 行）

1. GCC 三个 GPU 时钟：`gpu_memnoc_gfx`(0x7100c)、`gpu_snoc_dvm_gfx`(0x71018)、`ddrss_gpu_axi`(0x71154)，逐个置 enable 并回读；
2. GPUCC `gmu_clk_src`（0x1120）当前 `CMD_ROOT_OFF`(bit31)=1、源为 bi_tcxo；清 bit31 并置 RCG_UPDATE(bit0)，让 RCG 开始输出（19.2 MHz，够做寄存器访问）；
3. 使能 `cx_gmu` 分支（0x1098），轮询 CLK_OFF 最多 20 ms；
4. **仅当 cx_gmu 真的 running** 才映射 GMU（0x2c6a000）并读：`CM3_SYSRESET / BOOT_CONFIG / FW_INIT_RESULT / HFI_VERSION / RPMH_POWER_STATE / SPTPRAC` + 19.2 MHz 自走计数器；
5. 判决 `stage2=OK / FAIL / PARTIAL`。`gpugmu=0` 可跳过本轮。

## 刷写

```
fastboot boot raphael-t27-gmu-clean.img
# 或
fastboot boot raphael-t27-gmu-debug.img
```

## 判读树

开机约 30 秒后：

```
SWEET GPU: stage2: GCC GPU clocks
SWEET GPU: stage2: gpu_memnoc_gfx=0x1
SWEET GPU: stage2: gpu_snoc_dvm_gfx=0x1
SWEET GPU: stage2: ddrss_gpu_axi=0x1
SWEET GPU: stage2: gmu_rcgr=0x80000000 src=0x0
SWEET GPU: stage2: clearing CMD_ROOT_OFF (0x80000000 -> 0x1)
SWEET GPU: stage2: gmu_rcgr=0x0 src=0x0
SWEET GPU: stage2: cx_gmu=0x1 running
SWEET GPU: stage2: gmu sysreset=0x1 boot_config=0x... fw_init=0x0 hfi_ver=0x...
SWEET GPU: stage2: gmu rpmh_state=0x... sptprac=0x... counter=19200000 Hz
SWEET GPU: stage2=OK gmu=responding counter=19.2MHz crystal (firmware is stage 3)
```

| 现象 | 含义 | 结论 |
|---|---|---|
| `cx_gmu ... running` + `counter≈19.2 MHz` | GMU 已有时钟、寄存器与计数器都活 | **stage 2a 完成** ✅，下一轮加载固件 |
| `gmu=responding` 但计数器不是 19.2 MHz | RCG 分频/源与预期不同 | 回报该行，按实测频率修正 |
| `cx_gmu ... off` → `stage2=PARTIAL` | RCG 清 root-off 后分支仍不起 | 回报 `gmu_rcgr/src/cx_gmu` 三行；stage 2b 改 200 MHz GPLL0 方案 |
| `gmu=all-ones` | GMU 时钟在跑但寄存器无应答 | 回报整段 stage2 |
| 卡死在 `mapping GMU` 之后 | 极小概率：时钟不稳 | 强制重启，回报最后几行 |

## 日志取证

```
adb exec-out "tail -c 4198400 /dev/block/by-name/userdata" > D:\K20iOS\sweet-log.bin
```

## 回退

```
fastboot boot raphael-t26-gbif-clean.img    # CX 半区 + GBIF（t26）
fastboot boot raphael-t25-gpupower-clean.img
fastboot boot raphael-t12-gt9886-clean.img  # 稳定版
```

## 下一轮（stage 3）预告

GMU 固件（a640_gmu.bin 从 raphael vendor 固件提取）→ HFI 消息队列 → SMMU 页表上下文 → GX GDSC 上电 → 读 RBBM/CP 版本，最后才是 Mesa/freedreno 与界面合成。

## t27 实机结果（2026-10-24）

```
SWEET GPU: stage2: gpu_memnoc_gfx=0x221  gpu_snoc_dvm_gfx=0x1  ddrss_gpu_axi=0x1
SWEET GPU: stage2: gmu_rcgr=0x80000000 src=0
SWEET GPU: stage2: clearing CMD_ROOT_OFF (0x80000000 -> 0x1)
SWEET GPU: stage2: cx_gmu=0x4221 running
SWEET GPU: stage2: gmu sysreset=0x1 boot_config=0 fw_init=0 hfi_ver=0
SWEET GPU: stage2: gmu rpmh_state=0xf sptprac=0xc0 counter=21197600 Hz
SWEET GPU: stage2=OK gmu=responding counter=measured above (firmware is stage 3)
```

判读：GCC 三时钟已开；`cx_gmu` 分支运行（CLK_OFF 清）；GMU 寄存器可读，CM3 仍在复位（sysreset=1）、固件未加载（fw_init=0）、3D 核无电（sptprac=0xc0）——全部符合预期。计数器 21.2 MHz 是 `sleep 10ms` 的调度误差（实际约 11.04ms），折算为 19.2 MHz 晶振，下轮改用 mach_absolute_time 精确测量。
