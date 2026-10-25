# t33（r43）刷写与判读 · 释放 CM3：GMU 固件启动（等 0xbabeface）

> **r43（2026-10-25）**：t33 = t32 底座 + **0112-raphael-gmu-start**。
> t32 已实锤：TCM 3,723 字回读一致、4 个 DDR 段全部映射、SMMU CB1 盲写成功（`stage5=OK fw=loaded tcm_wrong=0 ddr_blocks=4`）。
> 本轮把 GMU 的启动配置写完、释放 CM3，然后轮询 `FW_INIT_RESULT` 直到 `(val & 0x1ff) == 0x100`（启动成功）或 `val & 0xe00`（固件报错）。

## 本轮改了什么（0112，+128/-1）

- **修正 HFI 页大小**：`GMU_HFI_SIZE` 从 4 KiB 改为 16 KiB（XNU 页大小；表 + 3 条队列按 4K 间隔需要 13 KiB）；
- 在 HFI 页填表：`words/first_queue=6/queue_words/queues=3`，三条队列 `start = 0x60006000 + 848 + 4096*n`、优先级 10/10/40、`status=1`；
- GBIF 解 halt；
- 按 HackiOS step 15 的顺序写 GMU 寄存器：CX_FAL/CX_FALNEXT/GENERAL_7/FW_INIT_RESULT=0/BOOT_CONFIG=2/HFI_QTBL/*/AHB_FENCE_RANGE_0/CM3_CFG=0x4052/PWR_COL_CP_RESP=0/HFI_SFR_ADDR=0x06040000/PWR_COL_CP_MSG=日志页/SYS_BUS/ICACHE/DCACHE/PWR_COL_INTER_FRAME_CTRL=0x9c40400/RPMH_CTRL=0xf11/INTR_CLR=all；
- 释放 `CM3_SYSRESET=0`，轮询 3 秒；
- 打印 `fw_init/intr/hfi_ctrl/sptprac/rpmh` 与 DTCM 版本（core/power/HFI）。

## 刷写

```
fastboot boot raphael-t33-boot-clean.img
# 或
fastboot boot raphael-t33-boot-debug.img
```

## 判读树

```
stage6: hfi table 210 words, 3 queues at 0x60006350/0x60007350/0x60008350
stage6: GMU configured (chip 0x6040000), releasing CM3
stage6: fw_init=0x100 intr=0x... hfi_ctrl=0x... sptprac=0x... rpmh=0x...
stage6: gmu versions core=0x... power=0x... hfi=0x...
stage6=OK fw=running (GMU firmware started; HFI queues are stage 7)
```

| 现象 | 含义 | 结论 |
|---|---|---|
| `fw_init` 的 `(val&0x1ff)==0x100` | **GMU 固件跑起来了** | stage 6 全胜，下一轮 HFI 队列 + PDC/RSCC + GX |
| `fw_init & 0xe00` | 固件报错（启动失败）| 回报完整 `fw_init/intr/sptprac/rpmh` 与版本行 |
| 3 秒超时（PARTIAL）| CM3 未启动或固件卡住 | 回报 `fw_init` 值与 `sptprac`；下轮查 GMU 时钟（19.2 MHz 可能太慢）|
| 卡死在 `releasing CM3` 之后 | 固件跑飞/总线挂起 | 强制重启，回报最后几行 |
| `stage6=FAIL CM3 is not held in reset` | 复位状态不对（stage 5 某步失败）| 回报该行 |

> 注意：本轮才开始"固件真正执行"，若它访问了我们映射错误的内存，可能整机挂起——`fastboot boot` 不落盘，重启即恢复。

## 日志取证

```
adb exec-out "tail -c 4198400 /dev/block/by-name/userdata" > D:\K20iOS\sweet-log.bin
```

## 回退

```
fastboot boot raphael-t32-fw-clean.img
fastboot boot raphael-t31-cb-clean.img
fastboot boot raphael-t12-gt9886-clean.img
```

## 下一轮（stage 7）

PDC/RSCC 唤醒与 HFI 队列启动（HackiOS step 16）→ HFI 消息（GX 上电、频率表、带宽表）→ 读 RBBM/CP 版本 → 之后才是 GPU 命令提交与 Mesa。
