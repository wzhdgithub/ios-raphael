# t60（r70）· 移植 HeliumStudio raphael 驱动（构建集成 spike）

> **r70（2026-10-28）**：t60 = t59 底座 + **0140-raphael-hkadreno-port**。

## 背景

t59 排除了厂测值（cmd_db 地址与我们一致、ACTLR/fence/chipid 无效、B0 仍挂）。HeliumStudio 的 `raphael-bringup` 分支在 raphael 上已跑通 FD640，因此直接移植其 `hakios.adreno.c`（22 步、a640 运行时地址重映射）。

## 本轮内容

1. 新文件 `bsd/dev/hakios.adreno.c`（4,250 行，含移植 shim）：
   - `hakios_is_raphael()=true`；
   - `hakios_image_file()` 返回内嵌的 **a640_gmu.bin**（zap/SQE 后续步骤再加）；
   - `sleep_us/ms`（内核等待实现）、`announce/say` 走 printf 进 sweetlog；
   - `loader_return / loader_framebuffer / MiniIOMobileFramebufferShow / HakiosGPUStart/Stop / hakios_tl / pe_arm_get_soc_base_phys` 为 stub；
   - 移除 `sys/hakios.input.h`、`mdevlookup` 等本树没有的依赖。
2. `bsd/conf/files` 加入该文件（上下文与 0103 的 sweet_gpu 行一致）。
3. `sweet_gpu.c` 加钩子：cmdline `hkadreno=1` 时调用 `hakios_adreno_init()` 并跳过自家 stage 代码。
4. 本轮 cmdline：`hkadreno=1 adreno=15 adreno_pause=0 adreno_skip=0x1c00`
   —— 只跑到 **step 15（GMU  firmware 启动）**，跳过 10-12（只读/TZ）。

## 判读

- 日志出现 `adreno: step 1..15` 与 **`THE GMU'S FIRMWARE RUNS`** → 它的 raphael 启动路径在我们的内核/loader 上也成立，M3 有望直接跨过；
- `adreno: 15: the GMU's firmware does not come up` → 对比其 step 13/14 的日志（SMMU 页表、固件块地址）找差异；
- 若早期步骤（4/5/6）失败 → 我们的 loader 留下的 GPU 状态与他们的不同，下一步按其 step 日志补齐前置状态。

## 回退

```
fastboot boot raphael-t59-vendor-values-debug.img
```
