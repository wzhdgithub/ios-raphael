# t46（r56）诊断 · 经 SMMU 访问 ICACHE 区（读 + 执行）

> **r56（2026-10-26）**：t46 = t45 底座 + **0125-raphael-icache-test**。

## 本轮改了什么（+40）

`gputest=1` 的 ITCM 程序（替换固件入口）现在做三件事：

1. DTCM[0] ← 0xDEADBEEF（证明 ITCM 执行）；
2. DTCM[1] ← 从 **ICACHE 区 0x7280** 读一个 word（固件里该地址是 0xBABEFACE）——验证
   CM3 经 GMU SMMU 的**数据读**；
3. `blx` 到放在 **ICACHE 区 0x4100** 的小程序，它写 DTCM[2] ← 0xCAFEBEEF——验证
   CM3 经 GMU SMMU 的**取指执行**。

主机写 ICACHE 小程序到宿主页 `icache_mem+0x100` 并 clean；释放 CM3 后打印
`gputest dtcm=<w0> <w1> <w2>`。

## 判读

| 打印 | 含义 | 结论 |
|---|---|---|
| `deadbeef babeface cafebeef` | ITCM、ICACHE 数据读、ICACHE 执行全部通过 | SMMU/ICACHE 路径正常，继续查固件其它依赖 |
| `deadbeef babeface 0` | 数据读通过，**取指失败** | ICACHE 执行路径（cache/属性）问题 |
| `deadbeef 0 0` | **数据读就失败** | SMMU 对 CM3 的访问不可用 |
| `0 ...` | ITCM 都没跑 | 不应发生（t45 已证） |

## 回退

```
fastboot boot raphael-t45-cm3test-debug.img
fastboot boot raphael-t44-rpmh-debug.img
```
