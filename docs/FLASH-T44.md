# t44（r54）刷写与判读 · 先加载 RPMh/PDC 微码，再启动 CM3

> **r54（2026-10-26）**：t44 = t43 底座 + **0123-raphael-rpmh-ucode**。

## 本轮改了什么（+95）

1. 按 vendor `_load_gmu_rpmh_ucode()` 在 stage 6（11 秒窗口之后、配置写之前）写入：
   - RSC：`RSC_STATUS0=BIT(24)`、`PDC_SLAVE_ID=1`、HIDDEN_TCS 三组、`OVERRIDE_START=0`、
     `PDC_SEQ_START=0x4520`、`MATCH_LO/HI=0x4510/0x4514`、`SEQ_MEM[5]`；
   - PDC seq（0xB490000）：`SEQ_MEM[5]`；
   - PDC cfg（0xB290000）：TCS1（power-up）与 TCS3（power-down）命令、`SEQ_START=0`、`ENABLE=0x80000001`；
2. `gpurpmh` boot-arg（默认 1）可关；
3. 冷启动**不做** RSC 唤醒（`gpursc` 仍默认 0）；
4. 打印 `rsc_status / seq0 / pdc_en / tcs1` 回读。

## 刷写

```
fastboot boot raphael-t44-rpmh-debug.img
```

cmdline：`gpuread=1 ufslogsecs=1`。

## 判读

```
stage6: rpmh ucode loaded: rsc_status=... seq0=0xa7a506a0 pdc_en=0x80000001 tcs1=7
stage6: GMU registers in 11 s
stage6: config group 1/2/3 written
stage6: GMU configured ..., releasing CM3
stage6: fw_init=0xbabeface        <- 目标
```

| 现象 | 含义 |
|---|---|
| `seq0=0xa7a506a0`、`pdc_en=0x80000001` | ucode 写入被接受 |
| `fw_init=0xbabeface` | **CM3 运行**，M3 完成 |
| 仍 `fw_init=0` | 看 gmu log / rsc_status / 是否还有启动前提 |

## 回退

```
fastboot boot raphael-t43-norsc-debug.img
fastboot boot raphael-t42-ctx1-debug.img
```
