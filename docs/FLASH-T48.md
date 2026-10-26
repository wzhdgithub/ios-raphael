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

## 结果（2026-10-26 22:21）

```
stage6: gputest dtcm=0xdeadbeef 0x11 0 (want deadbeef babeface cafebeef)
```

- DTCM[0]=0xdeadbeef → ITCM 执行；
- DTCM[1]=0x11 → 已到达 `blx`；
- DTCM[2]=0 → **ICACHE 区（0x4100）的取指失败**。

即：CM3 只能执行 ITCM，无法执行 ICACHE 区的代码——厂商固件的主代码就在 ICACHE（函数指针 0x71bd），
所以它永远写不出 FW_INIT。

**下一步假设**：GMU 用的 SMMU context 可能不是 **CB1**（HackiOS 用 1），而是 raphael vendor kgsl
给 GMU 分配的第一个 context bank **CB0**。t49/0128 把 context 号做成 `gpucb` boot-arg，
先用 `gpucb=0` 验证。
