# t58（r68）· GMU AHB fence 三值实验 + RBBM GBIF halt 实测

> **r68（2026-10-27）**：t58 = t57 底座 + **0138-raphael-gmu-ahb-fence**。

## 背景（t57 结果）

- **B0：HFI 读（0x60006F00，有效 GMU 窗口）在 `S2CR.Type=BYPASS` 下也挂死**——bypass 不需要 TCU/页表，所以阻塞在 **GMU AHB 主口的出站路径**，不是流表/翻译/TCU；
- 窗口外寄存器地址返回 0xdeadbeef 是 GMU 未映射默认值，不能用于判断 NoC；
- boot_args/DTB 都在 >4 GB，本机没有已知的低物理地址 DRAM 目标。

## 本轮实验

| 实验 | 内容 | 判读 |
|---|---|---|
| F0 | `GMU_AHB_FENCE_RANGE_0 = 0`（禁用）+ `AO_AHB_FENCE_CTRL=0`，HFI 读（BYPASS） | 完成 = fence 是闸门 |
| F1 | range = `0x80A008A0`（vendor a640 值），同上 | 完成 = fence 值错误导致阻塞 |
| F2 | range = `0x802800A0`（0132 继承值），同上 | 对照 |
| RBBM | GX 上电后读/清 `RBBM_GBIF_HALT/ACK`（0x2c00058/0x5c） | 对比 0x2c0f118 恒为 0x3 的 CX ack；GX 侧 halt 是否可清 |
| 常规 | A/B0/B/C/E/D、A2/B2、固件释放、fw_init | 连续性 |

## 判读

- **任一 F 完成** → GMU AHB fence 就是阻塞点；接着用该值重跑翻译/写测试，固件有机会启动。
- **F 全挂、RBBM halt 可清** → 转向 GBIF/NoC；对照 CX ack=0x3 判断是否是 GBIF 的 halt 未真正解除。
- **F 全挂、RBBM 也读不到** → AHB 出站被更上游（CNOC/DDR bus vote、XPU/安全）挡住；下一步实现 RPMh/BCM 投票或检查 XPU。
- `fw_init=0xBABEFACE` → M3 完成（36%）。

## 回退

```
fastboot boot raphael-t57-bypass-window-debug.img
```
