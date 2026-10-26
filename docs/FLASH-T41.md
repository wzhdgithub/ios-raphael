# t41（r51）· 0120 未实机 — S2CR 位域搞反，被 0121 取代

0120（QSMMU safe-toggle + RSC 唤醒 + TCU 探测）已编译（CI 40268262984），但在实机前复核
ARM SMMU-500 语义时发现 stage 5 的 S2CR 写法有误：

- ARM `arm-smmu.h`：`S2CR.TYPE = GENMASK(17,16)`，`S2CR.CBNDX = GENMASK(7,0)`；
- HackiOS `hakios_adreno.c` 的 `space_on()` 直接写 `S2CR(match) = space->context`（GMU 用 1），
  并按 `(s2cr & 0x300ff) == context` 校验；
- 因此 t39 的 `S2CR(5)=1u`（context 1 + 翻译）**本来是对的**；t40 的 `1u<<8` 把 GMU 指到了
  **context 0**，CB1 的 FSR=0x400 也不能作为 GMU 的故障证据（CB 寄存器读数本就不可靠）。

0120 未用于刷机，直接由 0121（t42）取代。其 safe-toggle / RSC 唤醒 / TCU 探测 / GMU 日志转储
代码全部保留进 0121。
