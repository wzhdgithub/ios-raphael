# t28（r38）刷写与判读 · GPU stage 3 前置：精确计数器 + TrustZone(SCM) 探测

> **r38 说明（2026-10-25）**：t28 = t27 底座（0001-0106）+ **0107-raphael-gpu-tz-probe**。
> t27 已证明 GMU 寄存器可读、`cx_gmu` 在跑、AO 计数器按固定 10ms 窗口算出 21.2 MHz（折算回 19.2 MHz 晶振）。
> 本轮两件事：①用 `mach_absolute_time` 精确测计数器频率；②发**只读 SCM 调用**问 TrustZone 是否可用、GPU zap 镜像是否受支持——zap 是 GX 上电前必须经 TZ 加载的签名镜像，XNU 目前没有 SCM 通道，这一步决定 stage 3 的路线。

## 本轮改了什么（0107，+88/-8）

- 计数器测量改为 `mach_absolute_time()` + `absolutetime_to_nanoseconds()`，输出真实频率；
- 新增 `gpu_scm()`（qcom_scm-smc 约定：x0=fnid，x1=参数描述符，x2..x4=参数，x5/x7=0，x6=session，`smc #0`）；
- 只读问三件事：SCM "is-call-available" 本身是否可用；PAS_INIT_IMAGE / PAS_AUTH_AND_RESET（zap 签名与启动）是否可用；PAS_IS_SUPPORTED(GPU=13) 是否支持；
- `gputz=0` 可跳过 TZ 探测（其余照常）。

## 刷写

```
fastboot boot raphael-t28-tz-clean.img
# 或
fastboot boot raphael-t28-tz-debug.img
```

## 判读树

开机约 30 秒后：

```
SWEET GPU: stage2: gmu rpmh_state=0xf sptprac=0xc0 counter=19200000 Hz
SWEET GPU: tz: asking whether it can be asked (smc 0x... )
SWEET GPU: tz: returns 0, answer 0x1
SWEET GPU: tz=available
SWEET GPU: tz: asking for the image signature call ...
SWEET GPU: tz: asking for the authenticate+start call ...
SWEET GPU: tz: asking whether the GPU zap image is supported ...
SWEET GPU: tz: gpu zap status=0 supported=1
```

| 现象 | 含义 | 结论 |
|---|---|---|
| `counter=19200000 Hz` | 精确测量确认 19.2 MHz 晶振 | GMU 时钟链彻底闭环 ✅ |
| `tz=available` + `gpu zap status=0 supported=1` | TZ 可调用、GPU zap 受支持 | stage 3 按 HackiOS 路线（TZ 加载 zap）实施 |
| `tz=available` 但 `supported=0` 或 status≠0 | TZ 在，但不认 GPU zap | 需要 zap 的替代方案（预签名/跳过 secvid），回报四行结果 |
| `tz=unavailable status=...` | SCM 不通（hypervisor 配置/调用约定不符）| 回报原始行；stage 3 先做 GMU 固件（不依赖 TZ），GX 上电另想办法 |
| 卡死在 `tz: asking` 之后 | TZ 调用未返回（极罕见）| 强制重启，回报最后几行 |

## 日志取证

```
adb exec-out "tail -c 4198400 /dev/block/by-name/userdata" > D:\K20iOS\sweet-log.bin
```

## 回退

```
fastboot boot raphael-t27-gmu-clean.img     # stage 2a（GMU 时钟/身份）
fastboot boot raphael-t26-gbif-clean.img    # CX + GBIF
fastboot boot raphael-t12-gt9886-clean.img  # 稳定版
```

## 下一轮

若 TZ 可用：stage 3a = GMU 固件（a640_gmu.bin 37,680 B）载入 TCM + CM3 boot + 等 `FW_INIT_RESULT=0xbabeface`；随后 stage 3b = SMMU 页表 + HFI + zap + GX 上电。
