# t34（r44）刷写与判读 · CM3 启动诊断：20 秒等待 + 复位/配置回读

> **r44（2026-10-25）**：t34 = t33 底座 + **0113-raphael-gmu-start-diag**。
> t33 实机：`stage6=PARTIAL fw_init=0 intr=0 hfi_ctrl=0 sptprac=0xc0 rpmh=0xf`，CM3 释放后 3 秒内没有写 `FW_INIT_RESULT`。
> 可能原因：①GMU 时钟仍在 19.2 MHz（RCG root-off 没清掉），固件启动比 200 MHz 慢 10 倍；②CM3 没真正执行（复位/BOOT_CONFIG/时钟）。本轮把等待延长到 20 秒，并回读释放后的 `CM3_SYSRESET/BOOT_CONFIG/CM3_CFG/AO_INTR/AHB_FENCE` 来区分。

## 本轮改了什么（0113，+8/-1）

- 轮询 `FW_INIT_RESULT` 从 3 秒延长到 **20 秒**；
- 释放后回读：`CM3_SYSRESET`（0=已释放）、`CM3_BOOT_CONFIG`（应为 2）、`CM3_CFG`（应为 0x4052）、`AO_HOST_INTERRUPT_STATUS`（0x24c14）、`AHB_FENCE_STATUS`（0x24c4c）；
- 新增判决行 `stage6: after release sysreset=... boot_config=... cm3_cfg=... ao_intr=... ahb_fence=...`。

## 刷写

```
fastboot boot raphael-t34-diag-clean.img
# 或
fastboot boot raphael-t34-diag-debug.img
```

## 判读树

| 现象 | 含义 | 下一步 |
|---|---|---|
| `fw_init=0x100` | 20 秒内起来了（只是慢）| stage 6 成功；下轮把 GMU 时钟拉到 200 MHz 再确认 |
| `sysreset=0 / boot_config=2 / cm3_cfg=0x4052` 但 `fw_init=0` | CM3 已释放、配置写进去了，但没执行固件 | 查 GMU 时钟（RCG root-off/GPLL0 路径）或固件/TCM 入口 |
| `sysreset=1` | 复位释放写没生效 | 时钟/寄存器写问题，回报该值 |
| `boot_config≠2` / `cm3_cfg≠0x4052` | 配置写被硬件改写/忽略 | 回报三值 |
| `ao_intr≠0` | GMU 的 always-on 部分有事件（如错误）| 回报该值与 `ahb_fence` |
| 卡死 | 固件跑飞 | 强制重启，回报最后几行 |

## 日志取证

```
adb exec-out "tail -c 4198400 /dev/block/by-name/userdata" > D:\K20iOS\sweet-log.bin
```

## 回退

```
fastboot boot raphael-t33-boot-clean.img
fastboot boot raphael-t32-fw-clean.img
fastboot boot raphael-t12-gt9886-clean.img
```

## 下一轮

若确认是时钟：用 GCC 的 GPLL0 分频 + GPUCC `gmu_clk_src` MND（1.5 → 200 MHz）把 GMU 时钟拉起来，再启动 CM3。
