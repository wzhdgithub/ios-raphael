# t62（r72）· zap + SQE + 显示：一次 CI 冲刺"GPU 画一帧到屏幕"

> **r72（2026-10-28）**：t62 = t60 移植底座 + **0141-raphael-zap-sqe-display**。

## 目的

在 t60 已让 GMU 固件跑起来的基础上，走 HeliumStudio 驱动的 step 16–22：
HFI 队列（16）→ GX 上电 + 3D core 寄存器（17）→ 3D core 地址空间 + ring + **a630_sqe.fw**（18）
→ **zap**（a640_zap.mdt + .b02，经 PAS_MEM_SETUP / AUTH_AND_RESET）（19）→ CP 启动（20）
→ 出 secure + 写一个词（21）→ **GPU 画三色图并显示（22）**。

## 本轮代码

- `bsd/dev/hakios_fw_extra.h`：内嵌 a640_zap.mdt(6,860 B)、a640_zap.b02(1,968 B)、a630_sqe.fw(32,304 B)；
- shim 的 `hakios_image_file()` 现在返回全部四个固件；
- `MiniIOMobileFramebufferShow()` 用 `PE_current_console()` 取启动 framebuffer（物理 v_baseAddr），
  把 1080×2400/stride 4352 的 BGRA 图画拷进我们 1080×2340 的 framebuffer（取顶部 2340 行）；
- cmdline：`hkadreno=1 adreno=22 adreno_pause=0 adreno_skip=0x1c00`。

## 判读

- 出现 **`A PICTURE DRAWN BY THE GPU IS ON THE PANEL`** → GPU 首次出图成功（M7 验证），屏幕上应看到橙/白/蓝三段；
- step 19 失败（PAS 拒绝）→ zap 的固定保留区 `0x98f15000` 在我们 boot 的 ADT 里未必保留，下一步改为自分配页并在 TZ 拒绝时回退；
- step 21 失败 → 3D core 仍处 secure（zap 未生效）；
- step 22 校验像素失败 → 2D blit/目标地址问题。

## 回退

```
fastboot boot raphael-t60-hkadreno-debug.img
```
