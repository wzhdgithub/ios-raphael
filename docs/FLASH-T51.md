# t51（r61）· 外部内存路径 + ICACHE/DCACHE 配置扫描

> **r61（2026-10-27）**：t51 = t50 底座 + **0131-raphael-extpath-cache-scan**。

## 背景（t50 结果）

t50 扫描 (CB 0–4) × (SMR 0–2) **15/15 全部失败**（`deadbeef 11 0`）——
SMMU 的 context/stream 组合全部排除。必须区分：
(a) GMU 经 SMMU 的整条外部内存路径坏；(b) 只有 ICACHE 窗口特殊。

## 本轮改了什么

1. gputest 程序升级：
   - DTCM[0]=0xDEADBEEF（ITCM）
   - DTCM[1] ← 读 HFI 页 `0x60006F00`（宿主写入 0xFEEDFACE）→ **验证 GMU→SMMU→宿主内存**
   - DTCM[2]=0x11（到达跳转）
   - DTCM[3] ← ICACHE 0x4100 例程写入 0xCAFEBEEF（验证取指）
2. 扫描增加 cache 维度：ICACHE/DCACHE 配置 = 1/1 或 0/0（各 5 CB × 3 SMR × 2 cache = 30 组）；
3. 转储 SGFSR、TBU_PWR_STATUS、SYNC_INV_TBU_ACK、SAFE_WAIT、CB0–4 FSR/FAR/FSYNR0、TBU 寄存器。

## 判读

| `scan ... -> w0 w1 w2 w3` | 含义 |
|---|---|
| `deadbeef feedface 11 cafebeef` | 全通：外部路径与 ICACHE 都 OK（某组配置）|
| `deadbeef feedface 11 0` | 外部路径 OK，ICACHE 取指特殊 |
| `deadbeef 0 11 0` | **整条 GMU 外部内存路径不通** |
| `deadbeef 0 0 0` | ITCM 都没跑（不应发生）|

## 回退

```
fastboot boot raphael-t50-scan-debug.img
```
