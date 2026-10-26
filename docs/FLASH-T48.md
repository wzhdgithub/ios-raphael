# t48（r58）· 纯取指测试：CM3 → ICACHE 0x4100

> **r58（2026-10-26）**：t48 = t47 底座 + **0127-raphael-icache-fetch-test**。

## 本轮改了什么（+6/-9）

gputest 的 ITCM 程序改为：

```
DTCM[0] = 0xDEADBEEF      ; ITCM 执行
DTCM[1] = 0x11            ; 已到达跳转点
blx 0x4101                ; 跳到 ICACHE 区的例程
例程 @0x4100: DTCM[2] = 0xCAFEBEEF; b .
```

不再做 ICACHE 数据读（t46/t47 的 LDR 可能访问了只允许取指的区域）。

## 判读

| `gputest dtcm=` | 含义 |
|---|---|
| `deadbeef 11 cafebeef` | **ICACHE 取指正常** → 厂商固件路径可用，回去跑真固件 |
| `deadbeef 11 0` | 已到跳转点但取指失败 → GMU 外部内存路径真的不通 |
| `deadbeef 0 ...` | ITCM 没跑到跳转（不应发生）|

## 回退

```
fastboot boot raphael-t47-fence-debug.img
fastboot boot raphael-t46-icache-debug.img
```
