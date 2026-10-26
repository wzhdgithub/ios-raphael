# t45（r55）诊断 · CM3 自检程序（gputest）

> **r55（2026-10-26）**：t45 = t44 底座 + **0124-raphael-cm3-test**。

## 本轮改了什么（+20）

`gputest=1` 时，在固件加载完成后用 20 字节 Thumb 程序覆盖 ITCM 入口：

```
0000: 803f0400   SP   = 0x43f80
0004: 09000000   reset= 0x9 -> 0x8
0008: 4803 4904  ldr r0,[pc,#12] (0x18=0x40000); ldr r1,[pc,#16] (0x1c=0xdeadbeef)
000c: 6001       str r1,[r0]
000e: e7fe       b .
0010: bf00 bf00 bf00 bf00
0018: 00040000
001c: deadbeef
```

释放 CM3 后，主机通过 DTCM 窗口读 0x40000，打印 `gputest dtcm0=...`。

## 刷写

先正常刷（不带 gputest）验证流程，然后用 repack 打开：
```
fastboot boot raphael-t45-cm3test-debug.img          # gputest=1 已打包
# 或从 t44 镜像 repack：--cmdline "... gputest=1"
```

## 判读

| 现象 | 含义 | 结论 |
|---|---|---|
| `gputest dtcm0=0xdeadbeef` | **CM3 核心确实执行** | 时钟/复位/电源没问题；厂商固件卡在自身启动（ICACHE/SMMU/HFI） |
| `gputest dtcm0=0` | CM3 没有执行 | 核心/时钟/复位/电源路径有问题 |
| 系统挂死 | 释放/写入触发总线问题 | 回报最后一行 |

## 回退

```
fastboot boot raphael-t44-rpmh-debug.img
fastboot boot raphael-t43-norsc-debug.img
```
