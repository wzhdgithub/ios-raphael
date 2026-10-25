# t36（r46）刷写与判读 · a640 GMU 寄存器表纠正（真正的根因）

> **r46（2026-10-25）**：t36 = t35 底座 + **0115-raphael-gmu-a640-map**。
> 之前的 GMU 代码全部沿用 HackiOS（a660）的寄存器偏移，而 a640 的映射完全不同。厂商 `a6xx_reg.h`（字索引）×4 的真实字节偏移：
> ```
> ITCM_START 0x6d000  DTCM_START 0x71000   SYSRESET 0x7e000  BOOT_CONFIG 0x7e004
> FW_INIT 0x7e070     CM3_CFG 0x7e0b4      HFI_CTRL 0x7e600  HFI_QTBL 0x7e610/0x7e614
> HFI_SFR 0x7e608     RPMH_CTRL 0x7e3a0    AO_COUNTER 0x7e220  AHB_FENCE_RANGE_0 0x8ec44
> ```
> 并且 GMU 的 TCM 地址空间是：**ITCM 0x0..0x3fff、DTCM 0x40000..0x43fff**（不是 0x10004000），`0x4000` 其实是 **ICACHE** 区（经 SMMU 映射）、`0x57000` 是 **DCACHE** 区。
> 这意味着此前所有 TCM 写入、复位释放、BOOT_CONFIG、fw_init 轮询都打在无效地址上——CM3 从未拿到代码。

## 本轮改了什么（0115，+41/-49）

1. 全部 GMU 寄存器常量换成 a640 偏移（`a6xx_reg.h`）；
2. TCM 窗口：ITCM `0x6d000`、DTCM `0x71000`；DTCM 范围 `0x40000..0x43fff`；
3. 固件块分类按厂商 `gmu_vma`：ITCM/DTCM 走窗口，其余（`0x4000` ICACHE、`0x57000` DCACHE）经 SMMU 映射内存；DDR 块计数期望从 4 改为 2；
4. 启动序列按厂商 `a6xx_gmu_configure`：FW_INIT_RESULT=0、BOOT_CONFIG=2（TCM_BOOT）、HFI_QTBL_ADDR/INFO、AHB_FENCE_RANGE_0、CM3_CFG=0x4052、HFI_SFR_ADDR=0x06040000、PWR_COL_CP_MSG=日志页、power config、WFI_CONFIG=0；
5. 释放前保持 200 MHz 时钟修复；
6. 轮询 **`FW_INIT_RESULT == 0xBABEFACE`**（厂商值），而不是 0x100 掩码。

## 刷写

```
fastboot boot raphael-t36-a640-clean.img
# 或
fastboot boot raphael-t36-a640-debug.img
```

## 判读树

```
stage5: fw 0x38bc bytes at 0x400 -> ITCM 窗口
stage5: fw 0x1b20 bytes at 0x40000 -> DTCM 窗口
stage5: fw 0x3a58 bytes at 0x4000 -> memory (icache)
stage5: fw 0x308 bytes at 0x57000 -> memory (dcache)
stage6: gmu rcg after cmd=0x0 cfg=0x602
stage6: fw_init=0xbabeface ... → stage6=OK fw=running
```

| 现象 | 含义 | 结论 |
|---|---|---|
| `fw_init=0xbabeface` | **GMU 固件真正启动** | stage 6 完成，进入 HFI/GX |
| `fw_init=0`（20 s） | CM3 仍未执行 | 回报 stage5 全部窗口行 + rcg 行；查 ICACHE/DCACHE 映射或固件镜像 |
| `fw_init≠0` 但非 0xbabeface | 固件写了别的状态 | 回报该值（可能是固件错误码）|
| `tcm_wrong>0` | TCM 回读不一致 | 回报 wrong 数与地址 |
| 卡死 | 固件跑飞 | 强制重启，回报最后几行 |

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
