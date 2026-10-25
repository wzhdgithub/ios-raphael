# t39（r49）刷写与判读 · GMU CSR 偏移 rebase（修复 data abort）

> **r49（2026-10-25）**：t39 = t38 底座 + **0118-raphael-gmu-csr-rebase**。
> t38 的“挂死”实为 **kernel data abort panic**，pc/lr 落在 `gpu_thread`，
> 行号 = `sweet_gpu.c:835` 的 `gpu_wr(gmu5, GMU_CM3_SYSRESET, 1)`：
> 0115 把 a6xx_reg.h 的 GPU 基址相对偏移（0x7e000）直接加到了 GMU ioremap 基址
> 0x2c6a000 上，`0x2c6a000 + 0x7e000 = 0x2ce4000` 超出 0x25000 映射 → 翻译错误。

## 本轮改了什么（0118，+29/-25）

1. 所有 GMU CSR / TCM 窗口偏移统一走 `GMU_CSR(x) = x - 0x6a000`（GPU 基址 → GMU 基址）：
   `SYSRESET 0x14000`、`BOOT_CONFIG 0x14004`、`FW_INIT_RESULT 0x14070`、`CM3_CFG 0x140b4`、
   `ITCM_WINDOW 0x3000`、`DTCM_WINDOW 0x7000`、`AHB_FENCE_RANGE_0 0x24c44`、
   stage6 的 `ao_intr 0x24c14` / `ahb_fence 0x24c4c` 等；
2. **保留** 0115 对固件内存布局的真实修正：DTCM VA 0x40000、ICACHE/DCACHE 经 SMMU、
   `ddr_blocks=2`、`0xBABEFACE` 启动判定；
3. 两个诊断窗口（不改寄存器序列）：
   - stage 2 GMU 读取前 `stage2: GMU read window in 3 s (flush)`；
   - stage 5 第一条 GMU 写前 `stage5: GMU map va=... sysreset=0x14000, write window in 3 s`。

打包时 cmdline 追加 **`gpuread=1 ufslogsecs=1`**：打开 stage 2 的 GMU 读（旧偏移已验证安全，
可见 counter/sysreset），并把日志落盘间隔从 10 s 降到 1 s，panic 前尽量留下现场。

## 刷写

```
fastboot boot raphael-t39-rebase-debug.img
# 无屏幕诊断时用 clean：
fastboot boot raphael-t39-rebase-clean.img
```

## 判读树

```
stage2: GMU read window in 3 s (flush)
stage2: gmu sysreset=0x1 boot_config=0 fw_init=0 hfi_ver=0   <- 期望（同 t27）
stage2: gmu rpmh_state=0xf sptprac=0xc0 counter=~21 MHz
stage4=OK smmu=responding ...
stage5: GMU map va=0xffff... sysreset=0x14000, write window in 3 s
stage5: fw ... -> ITCM / DTCM / memory
stage5=OK fw=loaded tcm_wrong=0 ddr_blocks=2
stage6: GMU registers in 11 s (flush)
stage6: GMU config registers written
stage6: GMU configured ..., releasing CM3
stage6: fw_init=0xbabeface  -> stage6=OK fw=running
```

| 现象 | 含义 | 结论 |
|---|---|---|
| `stage2: gmu counter=~21 MHz` | rebase 正确，GMU CSR 再次可读 | 继续看 stage5 |
| panic 仍在 stage 5 首写 | 映射/偏移还有问题 | 拍下 panic 首行 + `far:` |
| `GMU config registers written` 后卡住 | GMU 写可用，卡在 CM3 释放/固件 | 看 stage6 回读 |
| `fw_init=0xbabeface` | **CM3 固件运行** | stage 6 完成，进入 HFI 队列 |
| `fw_init=0` | CM3 未执行 | 看 fw 布局/TCM 回读 |

## 日志取证

```
adb devices                      # recovery
adb exec-out tail -c 4198400 /dev/block/by-name/userdata > D:\K20iOS\sweet-log-t39.bin
python D:\ios-raphael\tools\sweet-log.py D:\K20iOS\sweet-log-t39.bin D:\K20iOS\sweet-log-t39.txt
grep -a "SWEET GPU" D:\K20iOS\sweet-log-t39.txt
```

新鲜度：头部 total 应大于 t38 run 3 的 123,041；且应出现 0118 标记
`stage5: GMU map va=` 或 `GMU read window in 3 s`。

## 回退

```
fastboot boot raphael-t38-gate-debug.img
fastboot boot raphael-t37-earlyclk-clean.img
fastboot boot raphael-t12-gt9886-clean.img
```
