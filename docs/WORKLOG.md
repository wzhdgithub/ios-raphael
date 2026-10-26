# Worklog

---
Task ID: 1
Agent: Super Z (main agent)
Task: 研究 ios-sweet 项目并将 iOS 16 (XNU) 移植到 Redmi K20 Pro (raphael / SM8150)

Work Log:
- 深度研究了 dipa-studio/ios-sweet：README、NOTES.md（385 行工程日志）、81 个内核补丁、shim、AGENTS.md
- 评估 raphael 可行性：SM8150 与 SM7150 同代（Kryo 400、ARMv8.2 无 PAC、GICv3 双安全态、GENI/QUP、RPMh、SPMI、DWC3、UFS 2.1）
- 获取硬件事实三重来源：用户提供的 V12.5.6.0.RFKCNXM 官方固件（Range 分片并行下载 2.5GB 前缀，提取 boot.img + 4 个 SoC DTB）、Xiaomi raphael 内核源码 DTS（GitHub API 逐文件抓取）、mainline 参照
- 关键硬件验证（boot.img DTB 实证）：GICD 0x17a00000/GICR 0x17a60000、UFS 0x1d84000、SPMI 0xc440000/0xe600000/0xc40a000、RPMh RSC 0x18220000/TCS 0x18220d00、PSHOLD 0xc264000、帧缓冲 0x9c000000(0x2400000)、MPIDR Aff1=核号、定时器 PPI 18/19、imem 重启寄存器 0x146bf65c
- 触摸：Goodix GT9889 @0x5d SE19(0xc88000, wrap2_s2: 投票 BIT6@0x52014, RCG 0x1e3a8)，供电 TLMM GPIO98(3V3)/GPIO100(1V8)，reset GPIO54/IRQ GPIO122，GTX8 0x824E 协议（8字节/点小端坐标）
- 音量上键：所有 DTS 无 KEY_VOLUMEUP，暂缺（withheld），需真机 live DT 确认
- 下载 xnu-8796.141.3 源码（80MB），按序应用全部 81 个 sweet 补丁（含 libpthread/HFS/BearSSL staging，Linux 下伪造 xcrun/SDK stub）
- 编写 raphael 适配（全部 #if ARM64_BOARD_CONFIG_RAPHAEL 门控，SWEET 构建不变）：
  - 补丁 0082-raphael-machine-config（机器配置注册 + 复用 SWEET 驱动集）
  - 补丁 0083-raphael-input（GT9889 协议 + SE19 时钟/引脚 + TLMM 供电 + keys PON-only）
  - 补丁 0084-raphael-display（1080x2340：early console、start.s panic 行、SweetHID 缩放）
  - mkadt.py --raphael（面板/时钟/Kryo485）、shim RAPHAEL=1（FB 高度、imem、boot header v1 + DTB 追加）
  - tools/build-raphael.sh、tools/raphael-dtb-extract.py、tools/package-raphael.py（python-only 打包）
  - stage 脚本放开 MACHINE_CONFIG 门控；README-RAPHAEL.md、NOTES-RAPHAEL.md
- 补丁自测：重置到 pre-raphael 树重放 0082→0084 全部干净应用
- 用户提供的 GitHub token：fork → Gsjsjzhznsz/ios-sweet；推送 main+raphael 分支；注册并激活 Actions；触发三路构建：xnu(RAPHAEL 内核, run 39246750285)、raphael(shim, run 39246795196)、xnu(SWEET 验证, run 39246201208)

Stage Summary:
- 交付物：Gsjsjzhznsz/ios-sweet fork（main = 完整移植），本地 /home/z/my-project/ios-sweet-raphael/ios-sweet
- raphael ADT 生成验证通过（1080×2340/4320、Kryo485 时钟、MPIDR、UFS 基址）
- 三路 CI 运行中，等待产物：kernel.development.raphael + raphael-shim/Image
- 下一步：CI 产物下载 → 本地组装 shim-boot-raphael.img（package-raphael.py）→ 用户真机 fastboot boot 测试 → 按日志迭代

---
Task ID: 2
Agent: Super Z (main agent)
Task: 完成 raphael 移植的 CI 构建闭环并产出首测镜像

Work Log:
- 用户提供了 GitHub token，fork 到 Gsjsjzhznsz/ios-sweet，推送 main+raphael 分支
- CI 迭代修复链（每轮拉取 Actions 日志定位）：
  1. makedefs arm64e 豁免条件写反（ifneq→ifeq filter）→ SWEET 构建回归，修复后 main 分支 SWEET 构建成功
  2. 缺 config/MASTER.arm64.raphael → nos_arm_pmap/mockfs/sweet_hfs 选项缺失 → vm_last_phys 链接失败 → 创建（复用 sweet 选项集）
  3. bsd/conf/Makefile.template 的 SWEET 门控 → pthread/HFS include 路径 → 放开给 RAPHAEL
  4. libkern/conf/Makefile.template 同样的 BearSSL include 门控 → 放开
  5. MACHINE_FLAGS_ARM64_RAPHAEL 需同时定义 ARM64_BOARD_CONFIG_SWEET（驱动文件首行守卫在 include 前求值）
  6. vendor AOSP mkbootimg.py（LineageOS 镜像，gki 签名桩化）；package-raphael.py 参数前缀修复
- 最终结果：xnu.yml run 39257766195 SUCCESS → kernel.development.raphael (17.2MB)；raphael.yml shim 构建成功
- 本地组装：mkadt --raphael --highmem → mkpayload（16.6MB payload，链式重定位已解）→ package-raphael.py 打包
- 交付物（/home/z/my-project/download/）：shim-boot-raphael.img (17.3MB, 结构校验全过：ANDROID! v1 头/DTB magic/SWEETXNU payload)、payload、ADT、内核、测试说明

Stage Summary:
- RAPHAEL 内核在 GitHub macOS runner 上完整编译链接成功；首测镜像就绪
- SWEET 回归验证通过（main 分支构建成功）
- 用户下一步：fastboot boot shim-boot-raphael.img 冒烟测试 → 构建 iOS rootfs → 完整启动

---
Task ID: 3
Agent: Super Z (main agent)
Task: 更新仓库 README + 利用 K20 Pro 公开资源（mainline DTS/内核源码）关闭移植缺口（音量上键/触摸二供/USB），worklog 入库

Work Log:
- 资源调研（K20 Pro 场景优势兑现）：
  - mainline 补丁系列 "[PATCH v2 0/3] Add Xiaomi Redmi K20 Pro (Raphael)"（patchew 20251104221657, pc1598@mainlining.org）→ 音量上键实锤：pm8150_gpios 6 GPIO_ACTIVE_LOW
  - howtocuddle/xiaomi_raphael（6.18 mainline 状态：显示 ams639rq08/触摸/相机）+ ZakoClubROM/android_kernel_xiaomi_raphael lineage-19.0 分支
  - 内核源码 drivers/input/touchscreen/：goodix_driver_gt9886 + fts_521 → 证实 fts_521 是 ST FingerTipS（I2C 0x49，0xFA+4字节大端地址读 0x20000000 芯片ID，FIFO 0x86 读 8 字节事件 b0>>4=enter/motion/leave），此前"FocalTech 0x38"判断为误读
  - mainline sm8150.dtsi + gcc-sm8150.c：USB30_PRIM 的 DWC3 0xa600000/qscratch 0xa6f8800/QUSB2 0x88e2000/GDSC 0xf004 与 sweet 完全相同；仅 cfg-noc 0x502c→0xf078、aggre 0x8201c→0xf07c、phy-ref=USB3_SEC_CLKREF 0x8c028（0x8c010 在 sm8150 是 gpu_iref）
- 编写补丁（xnu-tree 编辑 → git diff 生成，重放验证干净应用）：
  - 0085-raphael-volumeup.patch：keys GPIO 常量按板分级（PPID 0x0c5/RT 0xc510）+ keys_gpio_census() 一次性只读普查全部 PMIC GPIO 块作为真机证据
  - 0086-raphael-touch-stfts.patch：更正二供为 ST FTS；0x49 只读探测打日志；Goodix 失败且 0x49 应答时走 touch_stfts() 只读事件泵；修复轮询 IRQ 引脚硬编码 9 → TOUCH_GPIO_IRQ(122)
  - 0087-raphael-usb.patch：usb_cbcr 表按板分级（含注释差异表），phy 探测门控改为 phy-ref；ue0/ECM/UDP 日志链路全部激活（boot-arg sweetusb=2）
- 文档：README.md 加 raphael 版块；README-RAPHAEL.md 重写状态/缺口/资源清单；NOTES-RAPHAEL.md 补音量键、ST FTS 更正、USB 对照表、外部来源清单
- 工程卫生：Edit 工具把整文件 tab→空格导致 749 行空白噪声 diff，写 scripts/restore_indent.py 以 HEAD 为基准恢复原始缩进（最终 diff 仅 +177/-24）；语法平衡检查通过
- worklog 同步入库为 docs/WORKLOG.md（用户新要求：防记忆丢失）

Stage Summary:
- 三个移植缺口全部关闭（音量上键/触摸二供/USB 日志通道），补丁已验证可干净重放
- 文档三件套与资源链接更新完毕，rme：mainline 化的硬件事实表可直接支撑后续真机调试
- 下一步：push → CI 重建 RAPHAEL 内核 → 重打包 shim-boot-raphael.img → 用户真机测试

---
Task ID: 4
Agent: Super Z (main agent)
Task: CI 重建 RAPHAEL 内核（fecc50a）并重打包首测镜像，交付物刷新，worklog 入库

Work Log:
- 推送 raphael 分支（fecc50a）→ 触发 xnu workflow（machine_config=RAPHAEL, run 39268242631），~9 分钟构建 SUCCESS，head_sha 确认为 fecc50a
- 下载产物 kernel.development.raphael（17,206,056 字节 Mach-O arm64）
- 本地重打包：mkadt --raphael --highmem → mkpayload（cmdline 与上轮一致）→ package-raphael.py（stock boot.img 提取 SM8150 v2 DTB，header v1）
- 结构校验：ANDROID! v1 头、kernel blob 末尾 DTB magic 0xd00dfeed、SWEETXNU payload 在位
- 交付物刷新（/home/z/my-project/download/）：shim-boot-raphael.img (17,301,504)、payload-raphael.bin、sweet-raphael.adt、kernel.development.raphael、README-RAPHAEL-TEST.txt（含 sweetusb=2 直播日志指引，已修正过时的"已知限制"段）
- worklog 同步 docs/WORKLOG.md 并推送仓库

Stage Summary:
- 含 0085-0087 的首测镜像就绪：真机 fastboot boot 即可验证音量上键/触摸二供/USB 直播日志三条新链路
- 验证顺序建议：sweetusb=1 看时钟/GDSC 报告 → sweetusb=2 + sweet-usbnet.sh 直播日志 → cpus=1→8 → rootfs

---
Task ID: 5
Agent: Super Z (main agent)
Task: README 双语化（默认中文）+ 仓库全面清除 Note 10（sweet/SM7150）内容，仓库定位改为纯 raphael/855

Work Log:
- README.md 全文重写：双语结构（中文默认在上、English 完整镜像在下），内容 100% raphael（K20 Pro / SM8150 / 骁龙 855）
  - 新增：当前状态表（8 子系统）、启动链、三条日志通道说明、真机测试指南（备份/rootfs/CI 内核/打包/fastboot boot/boot-args 速查）、raphael 补丁表 0082-0087、SM8150 硬件事实、K20 Pro 刷机生态资源清单（mainline DTS / howtocuddle 6.18 / pmOS / ZakoClubROM 内核源码 / LineageOS / XDA）、仓库布局、Credits（上游 dipa-studio/ios-sweet 框架署名）
- 删除旧 NOTES.md（433 行，上游 Note 10 工程日志，169 处 sweet 引用）→ git rm
- NOTES-RAPHAEL.md → git mv 为 NOTES.md 并重写净化：删除全部 "identical/mirrors/like sweet" 对照叙述与 sweet(SM7150) 对照列，改为 raphael 单列 + "generic value, no change / generic constant was X" 表述；外部来源列表去掉 sm7150.dtsi/gcc-sm7150.c
- 删除 README-RAPHAEL.md（内容已并入新 README）
- AGENTS.md 重写：指向新文档结构 + "每步完成即 commit+push worklog" 规则（用户要求）
- 交叉引用修正：tools/mkadt.py docstring（SWEET xnu→XNU kernel、sweet live FDT→target device's live FDT、NOTES-RAPHAEL→NOTES）、raphael.yml 注释（README-RAPHAEL→README）
- .gitignore 增加 shim-boot*.img（首测镜像不入库）
- 清除验证：grep note10/SM7150/732G 于 README/NOTES/AGENTS = 0 命中；剩余 sweet 均为代码字面量（fork 仓库名、工具/产物文件名、boot-arg、'xiaomi,sweet' 兼容串、SweetHID 组件名）
- 文档保留范围声明：0001-0081 补丁与工具文件名为上游框架遗产，属代码标识符而非 Note 10 内容，未重命名（避免破坏 CI 补丁序与内核构建）

Stage Summary:
- 仓库文档层已完全定位为 raphael/855 项目；README 双语、中文默认
- 待办不变：真机 fastboot boot 冒烟测试（cpus=1 起步）

---
Task ID: 6
Agent: Super Z (main agent)
Task: 无电脑（手机对手机）测试方案：cpus=1 保守镜像 + 零 PC 逐步指南 + 交付 zip

Work Log:
- 用户环境确认：完全没有电脑，仅有另一台可做 fastboot 主机的安卓手机
- 本地重打包 shim-boot-raphael-safe.img（scripts/repack-raphael-safe.sh）：同一内核（fecc50a，含 0085-0087）+ 同一 ADT，cmdline 改为 cpus=1、去掉 sweetfb_noconsole/sweetfb_hz（屏幕控制台常显）、不启用 sweetusb —— 首测最大可见性
- 结构校验：ANDROID! header v1、ramdisk == payload cmp 一致、payload cmdline 复核（cpus=1，无 fb mute）
- 交付 zip：raphael-p2p-test.zip（12MB）= safe 版 + 8 核版 + 测试指南 txt
- 撰写 docs/TEST-NO-PC.txt（零 PC 指南）：Termux(F-Droid/GitHub) + android-tools + OTG fastboot boot 流程、预期现象与拍照留证、OrangeFox 终端一行 dd 提取 userdata 末尾 4MiB 日志环（备选 Termux adb pull）、结果自查清单、常见问题（OTG 不识别/未解锁 not allowed/黑屏但 Booting OK）、风险边界（boot 不刷分区）
- README.md 中英两区增加"没有电脑？"入口链接 docs/TEST-NO-PC.txt
- 交付物（/home/z/my-project/download/）：shim-boot-raphael-safe.img、raphael-p2p-test.zip、无电脑测试指南-手机对手机.txt

Stage Summary:
- 用户可全程无电脑完成冒烟测试：手机 A(Termux) --OTG--> K20 Pro fastboot boot
- 硬门槛已在指南标注：K20 Pro 必须 BL 已解锁（小米解锁依赖 Windows 工具）
- 等待用户回传：屏幕照片 / sweet-log.bin；后续 rootfs 阶段拟用云 Linux 服务器代跑 mkiosroot

---
Task ID: 7
Agent: Super Z (main agent)
Task: 首轮真机测试结果分析（黑屏+日志空）→ 闪存面包屑诊断镜像（v1+v2）

Work Log:
- 用户回传：fastboot boot OKAY（主机手机 OTG 成功），K20 Pro 全程黑屏无背光，无自动重启；tail -c 4194304 sda31 提取 4MiB
- 日志分析（scripts/analyze-sweetlog.py + decode-log.py）：4MiB 内零 SWEETLOG 头、零 SWEET/Darwin/panic 标记，前 2.1MiB 可打印率 37%（Android 遗留 FS 数据），0x205000 后全零 ⇒ 本次启动未写过任何内核日志；且用户 tail 少了日志头块（区域=[头 4K][环 4M]），sda31=userdata 待 by-name 核实
- 证据推理：shim 首屏动作是 con_clear(RED)（红屏）；异常路径会画蓝屏并 40s 自动重启，用户需长按电源 ⇒ 极早期崩溃（ABLL未交控制权/shim 首 C 调用前）或面板在 ABL 交接时被关闭，证据不足
- shim 面包屑实现（013048e）：ufs.c SCSI 增加方向参数+WRITE(10)；ufs_crumb() 把阶段记录写到 userdata 最后 4KiB（MMU off、Device 内存、复用 test_ufs 的 UTRL 接管流程+GPT 找 userdata 尾块）；shim_main 入口 MAIN、exc_handler EXC(slot/ESR/ELR/FAR)、loader DTB/PLD/SEG/JUMP；SWEET 构建 no-op 不受影响
- CI 异常排队 30+ 分钟后首个 run 39279520451 成功（GCC 15.2），artifact Image 200,704 字节；本地工具链方案（xpack/deb）备而未用
- 打包：payload-raphael-diag.bin（cmdline=safe 版+sweetusb=2）；shim-boot-raphael-diag.img（header v1）+ shim-boot-raphael-diag-v2.img（header v2 兜底实验，DTB 独立段）；双双 ANDROID! 头校验
- 交付：raphael-diag-round2.zip（12MB，双镜像+第二轮指南）；docs/TEST-NO-PC-DIAG.txt（补拉日志头/USB 枚举 sysfs 检查/面包屑判读表）

Stage Summary:
- 第二轮测试三证据通道：闪存面包屑（核心）+ sweet-full.bin 日志头 + 主机 sysfs USB 枚举
- 等待用户回传 crumb.bin / sweet-full.bin；JUMP 后无内核日志 ⇒ 内核极早期问题，有 EXC ⇒ shim 可定位

---
Task 8
Agent: Super Z (main agent)
Task: 第二轮证据解读 + shim 阶段码诊断（diag4）交付

Work Log:
- 证据更正：crumb.bin 仅 22 字节 = 提取命令失败（tail -c 4096 必产 4096），"无面包屑"结论未定；sweet-full.bin 乱码 = 内核 UFS 驱动未通（预期，0047 日志线程挂在 UFS probe 后）；18d1/Android = ABL fastboot gadget 残留（XNU 应枚举 1209/Sweet iOS），加 fastboot devices 判别法
- 确认 0084 无 DSI/面板初始化 → fastboot boot 后 ABL 关显示，黑屏是结构性预期，不是崩溃信号
- shim 阶段诊断（6b2b23c + 修复 b5c9e04，CI run 39292316444）：ramoops 区（0xa1dff000/0xa1601000，原厂 DTS pstore 请求 0xc00000 > 区 0x800000 永不绑定）写 STGD 二进制+ASCII 记录（阶段码/面包屑故障码/ESR/ELR/FAR）；shimstage=1 boot 不跳 XNU，按 20+20×阶段 秒自动重启（140s=阶段6→XNU侧问题；40s=异常；200s=校验失败；永不=极早期挂死）
- ufs_crumb_diag()：面包屑失败原因码（host 未就绪/列表接管/TUR/GPT/无 userdata/写失败 rc）
- fdw 修复：fdt_chosen_str 曾覆盖 fdt_chosen_u64 导致 CI 链接失败，已恢复双函数；stage ASCII 96→128
- TEST-NO-PC.txt dd 提取改为 tail -c 4198400（真机实测 TWRP 下 blockdev 负数）；新增 docs/TEST-NO-PC-DIAG3.txt
- 交付 raphael-diag-round3.zip（diag4 v1+v2 + 指南）

Stage Summary:
- 三条零依赖证据链（秒表解码/devmem DDR 记录/闪存面包屑+故障码）+ USB 判别法就绪
- 等用户回传：crumb.bin（wc=4096）、diag4 掐表结果、devmem 四值、fastboot devices 判别输出

---
Task ID: 9
Agent: Super Z (main agent)
Task: 第三轮真机结果解读（diag4-v2 点亮屏幕）+ diag5 XNU 直通镜像交付

Work Log:
- 屏幕输出逐行核对 shim.c/loader.c 源码：SHIM milestone 4 → INITRD/PHYS/VIRT/ENTRY/SEGS/BOOTARG/JUMP 全部来自 loader.c，"RESET now" 是 reset() 的计划内打印（con_str("RESET","now")，shim.c:120）
- 结论：diag4 的 shimstage=1 在跳转前故意拦停（loader.c:162 stage_diag_final()，20+20×6=140 秒后打印 RESET now 并 PSCI 复位）——XNU 从未运行，本轮零崩溃，shim 6 阶段全绿
- v1 死寂/v2 点亮的差异 = boot 头格式：v1 为 header v1（DTB 拼接 kernel），v2 为 header v2 + 独立 DTB section；raphael 小米 ABL 只接受 v2 格式（之前多轮 header-v1 镜像很可能从未执行）
- payload-raphael-diag4.bin 头部实解：phys_base=0x100000000、mem_size=0xFCC00000、virt_base=0xFFFFFE0006000000、entry_pa=0x10148B488——屏幕上 "PHYS base=00000000" 是 con_hex2 的 uint32 截断显示，非真值
- 屏幕点亮证明 ABL 显示管线在 fastboot boot 后持续自刷新，写 splash framebuffer 即可上屏（更正第二轮"结构性黑屏"判断）；fb 0x9c000000 与 RAM 0x100000000 在 16K granule 下同属 V=P L1 index 0 的 64GB 窗口，0010 的 fb bootstrap 映射（L2 idx 78 < RAM idx 128）在 raphael 上无冲突
- XNU 侧诊断资产核查（fecc50a 已含）：start.s stage-0 白条（MMU off 直画）+ sweet_early.c 早期文本控制台 + sweet_early_mark(1..8)（arm_init 七个埋点 + TTBR0 切换）+ sweet_early_panic；0084 已适配 2340 高度
- diag5 打包（scripts/package-raphael-diag5.sh）：同一 shim/内核/ADT，cmdline 去掉 shimstage=1，header v2；cmp 校验 = 与 diag4-v2 仅差 12 字节 shimstage 区 + 20 字节 build id
- 交付 shim-boot-raphael-diag5.img + payload-raphael-diag5.bin + docs/TEST-NO-PC-DIAG5.txt（含白条/SWEET EARLY 1-8 结果对照表、6GB 机型预警、掐表要求）

Stage Summary:
- shim 链路真机验证完毕；下一轮 XNU 首跳，XNU 内置三段早期诊断开箱即用，无需新内核构建
- 关键认知：屏幕 = 第一诊断通道；v2 头 = 唯一被 ABL 接受的格式；18d1/Android = ABL fastboot gadget 残留
- 等 diag5 回传：白条是否出现、SWEET EARLY 走到第几行、定格画面全景、掐表时间线

---
Task ID: 10
Agent: Super Z (main agent)
Task: t7 实测"其他全绿、触摸仍死"日志分析 → CI 级根因（push 构建 SWEET 回退）→ 修复 + t8 交付

Work Log:
- sweet-log.bin（4MiB 环）解码：唯一一次启动，boot-args = t7 r15（cpus=8/sweetfb_noconsole=1/sweetfb_hz=60/sweettouchflip=0）；触摸线：gcc vote=0x4bc0000 rcg=0x80000000、分支 0x18008 HW-gated(0x80000002) ok / 0x18144 still halted(0x80004220) 继续、se7 fw=0x303 proto=3 status=0 tx/rx 32 位 FIFO 全过、"need ABL-loaded I2C firmware and 32-bit FIFOs; probes skipped"
- 寄存器语义（Linux geni-se.h 核对）：0x68=GENI_FW_REVISION_RO、0xe24/0xe28=HW FIFO 参数、0x908=GENI_IOS、0x64=GENI_IF_DISABLE_RO（bit0=接口禁用）→ skip 触发点
- 关键矛盾：日志分支偏移 0x18008/0x18144 ∈ SWEET 列表，而 raphael 列表应为 0x1e004/0x1e008/0x1e3a4
- CI 日志取证（run 39469756410，a4ca660=Head of r15）：event=push；937/937 条 clang 命令仅 -DARM64_BOARD_CONFIG_SWEET。对照 fecc50a（t2-t6 时代内核，run 39268242631）= workflow_dispatch + machine_config=RAPHAEL，962/962 带 -DRAPHAEL —— 0089 commit message 引用的 "0x1e008 halted" 正是 r14（dispatch、RAPHAEL）t6 日志的真实 wrap2 s_ahb 卡点；r15/t7（push）退化成 SWEET，0089 从未被测试
- 根因：xnu.yml push 触发时 inputs 为空 → MACHINE_CONFIG 回退 'SWEET' → 所有 raphael #if 增量（触摸 SE19/GT9889/TLMM 供电/音量上键/USB/0086/0089）预处理剔除；显示/ADT/UFS 因板级无关而幸存，掩盖了问题
- 修复 ea69ad1：push 默认也 RAPHAEL + Build 改 tee build.log + "Verify board macro" 步骤（RAPHAEL 构建下编译命令零 -DRAPHAEL 即 fail）；新内核 run 39517478755 复验 937/937 带 -DRAPHAEL，sweet_touch.c 双宏齐备
- t8 打包：t7.img 提取 shim/ADT/DTB/cmdline（tools 复用 repack-boot-v2 解析）→ mkpayload 新内核（entry 0xfffffe000748b488 与启动谱系一致，nsegs=10）→ 模板头装配（scripts/t8_build.py：仅 ramdisk_size 变化，shim/DTB/ADT/cmdline 逐字节不变）；payload 字符串普查确认 RAPHAEL 触摸打印全在、SWEET 触摸打印全消
- Release raphael-touch-r16：raphael-t8-clean.img（1bcdfbb9…）、raphael-t8-debug.img（f6bcfc9f…，+sweethidtest=15）、FLASH-T8.md、T8-SHA256SUMS.txt；README 状态/下载表更新；t8_extract/t8_build 工具入库
- 新工具：scripts/t8_extract.py（boot v2 组件提取）、scripts/t8_build.py（模板装配+六重校验）；ST FTS 澄清：raphael 二供为 ST FingerTipS（fts_521@0x49），非 FocalTech

Stage Summary:
- 触摸失灵定性：既非 SE19 固件未加载、非 I2C 地址错误、也非二供批次——raphael 触摸代码根本没编进 t7 内核
- t8 为首次"RAPHAEL 编译 + 0089"组合；预期日志：分支 0x1e004/0x1e008(HW-gated ok)/0x1e3a4 + TLMM reset(gpio54) 供电 + probe 0x5d ver-reg / probe ST FTS 0x49
- CI 侧回归防线就位：宏断言防复发；回退 = 任意 t 系列直启

---

Task ID: 11
Agent: Super Z (main agent, post-handover session)
Task: 项目交接（session 更替）——凭据/仓库/CI/Release 全量盘点，本地工作环境恢复，worklog 双写机制重建

Work Log:
- 接收用户指示：GitHub 凭据保留使用（token 已内嵌于 git remote URL，`git remote get-url origin` 可取；用户明确豁免安全顾虑）；裸 token 不入 git（AGENTS.md 卫生规则）
- Clone Gsjsjzhznsz/ios-raphael main @ c1da721（= r16/t8 发布提交），工作树 clean，与 origin/main 同步
- 通读交接材料：README.md（r16/t8 状态表 + 下载表 + 刷机指南）、AGENTS.md（worklog 双写 + 即时 push 协议）、NOTES.md（SM8150 硬件事实 / GT9889 协议 / ST FTS 二供 / USB 表）、docs/WORKLOG.md Task 1-10 全量、docs/FLASH-T8.md（t8 判读树）
- CI 核查（API）：最新 xnu push run 39517478755（ea69ad1）success —— 即 t8 内核（RAPHAEL 宏 937/937 复验通过的那次构建）；build-ios-rootfs 39371434600 success
- Release 核查：共 9 个（ios-rootfs-16.7.16-12 … raphael-touch-r16）；最新 raphael-touch-r16（2026-10-21T00:10Z）= t8-clean/debug + FLASH-T8.md + T8-SHA256SUMS，与 README 下载表哈希一致；无 open issues
- 本地环境恢复：/home/z/my-project/download/ 为空（新会话起点，t 系列镜像按需从 Release 取）；重建 /home/z/my-project/worklog.md 并与 docs/WORKLOG.md 同步
- 交接点确认：t8 已发布未实测 —— 等待用户真机 fastboot boot raphael-t8-clean.img（或 debug 版）回传：触摸现象 + sweet-log.bin；判读路径 = FLASH-T8.md 决胜行（probe 0x5d / ST FTS 0x49 / probes skipped）
- 挂起事项继承：熄屏渐慢（sweetfb_hz 30→60 对照，t7/t8 cmdline 已带 60）待 t8 回报时一并观察

Stage Summary:
- 交接完成，无状态丢失；项目处于 r16/t8 待实测节点
- 下一步：用户刷 t8 → 回传日志/现象 → 按 FLASH-T8.md 判读 → 触摸收尾或转入下一子系统迭代

---

Task ID: 12
Agent: Super Z (main agent)
Task: t8 实测回归（卡 bootstrap done，t7 正常）→ 取证定位 → t9 修复发布 + 发行版清理（保留 t7/ios16）

Work Log:
- 用户回报：t8 卡 bootstrap 而 t7 正常；要求恢复 bootstrap done 之后的启动链；清除发行版只保留 t7 与 ios16
- 取证基建：下载 t8 内核 CI 构建树产物（run 39517478755 xnu-buildtree，含已应用全部 89 补丁的源码）；下载 t2/t6/t7/t8 四代镜像（SHA256 与发布值一致）；趁删除前抢救 R11 交叉定位文档 TEST-MATRIX-R11.md
- 代码侧排查（全链路）：git diff 333512b..ea69ad1 实锤 t6→t8 树差异仅 sweet_touch.c（线程路径，45s 延迟启动，所有等待带上界）；sweet_early/SweetHID/keys/usb 的 raphael 段逐一审计均无害 → 单纯代码差异无法解释 t7/t8 分野
- 决定性实证（镜像字符串标记 + ADT 逐节点解析）：
  1) t2/t8 = RAPHAEL 编译（TLMM reset(gpio/census 在），t6/t7 = SWEET 编译（Yellowstone/L11C 在）—— t5–t7 全是 SWEET 内核，RAPHAEL 编译从未在 t 系列成功启动；纠正 Task 10 "r14=dispatch/RAPHAEL" 的记载（t6 镜像为 SWEET 编译）
  2) t 系列 ADT（t6/t7/t8 逐字节同，f50510e0）与 t2 ADT 唯一结构差异 = CPU 节点 1→8（fbcon 9c000000+0x9a4000、/product、其余全部相同）
  3) R11 矩阵复盘：diag5/r11-sc/t8 三个 RAPHAEL 启动全部卡在同一卡点（UFS probe 之前），唯一例外 t2；t2 差异 = ①匹配表 kryo470→485（只做过二进制就地补丁，从未入库：KernelConfigTables.cpp:47 仍是 kryo470 vs ADT 声明 kryo485）②ADT 仅 1 CPU 节点；两变量从未隔离（t3/t4 未测）
- t9 修复（t2 配方正式入库）：
  - 补丁 0090-raphael-cpu-match：IONameMatch 'qcom,kryo470'→'qcom,kryo485' + cpuName "Kryo 470"→"Kryo 485"（从构建树 git diff 生成，提交 51af6b9，CI run 39557186698 SUCCESS，内核 SHA 32327c91…，字符串验证 kryo485 在/kryo470 消/0089 在）
  - ADT：scripts/mkadt-1cpu.py 对已发布 t8 ADT 外科手术删 cpu1–7（adt.py 解析/序列化，其余节点逐字节保序）= 7044 字节，与 t2 ADT 同尺寸同拓扑；mkadt.py 增加 --cpus=N 入口（再生成路线因 d22ap.adt 缺失不可用，已记录）
  - 打包 scripts/t9_package.py：t7 镜像骨架 + 新内核 + 1-CPU ADT；clean cmdline 与 t7 逐字节一致（330B）；debug = 去 sweetfb_noconsole=1（屏幕控制台常开，r11 取证法）；六重校验全过（ANDROID!v2/shim/DTB==t7/ADT/cmdline/markers/entry 0xfffffe000748b488 与谱系一致）
- 发行版操作：删除 7 个 Release+tag（r16/r14/r11/r12/r13/sc 单核/sc 全核），保留 raphael-touch-r15（t7）与 ios-rootfs-16.7.16-12；发布 raphael-touch-r17（t9 clean/debug + FLASH-T9.md + T9-SHA256SUMS）
- 文档：README 状态区改写（t8 回归实证 + t9 待测）、下载表裁剪（t9 推荐 + t7 回退 + rootfs）、刷机指南改指 t9；docs/FLASH-T9.md 入库；worklog 双写

Stage Summary:
- 根因画像：卡 bootstrap done ⇔ RAPHAEL 编译 ∧（匹配表 kryo470 失配 ∨ 8-CPU ADT）—— t2 是唯一通关组合，两变量随 t9 一并入库，归因可后补隔离
- t9 一次刷机闭环设计：修好 → 直接锁屏（顺带看触摸）；仍卡 → debug 版拍最后几行即得精确卡点
- 若 t9 通关：下一步仅改 ADT（无需重编内核）逐级回加 CPU 节点 2/4/8 找死点，恢复 8 核；触摸判读沿用 FLASH-T8.md 树


---
Task ID: 13
Agent: Super Z (main agent)
Task: t9 实测解读（触摸仍死 + 卡顿）→ 触摸全链路静态审计 + t10（0091 总线诊断 + 8 核回归）发布

Work Log:
- t9 日志解码（sweet-log.bin 4MiB 环）：t9 完整启动进锁屏（bootstrap-done 修复成立）；`SWEET CPU: requested=8 selected=1` = 单核 → "很卡"定性为 t2 配方副作用；按键 HID 全通
- 触摸判决行：probe 0x5d err=60（ETIMEDOUT）seen=0x40000000（TX 水位触发、2 字节入 FIFO）tx=2 rx=0 status=0（无活动命令）+ abort 干净（无 abort failed、dead=0）→ 序列器活着但总线上零活动；probe ST FTS 0x49 err=5 tx=0 seen=0 → 命令根本没发出
- Bug 实锤（0086 遗留）：touch_msg_run 的 `nt > 3 → EIO(5)` 硬上限拒绝 5 字节 ST FTS 命令（0xFA+4 地址字节）；touch_stfts 运行时 nt=1+nr=8 同样撞 `(nt && nr)` 拒绝 —— ST FTS 探针与轮询全是结构性死代码
- 全链路静态审计（对齐 Linux torvalds 源码 + Lineage raphael vendor 源码 + 原厂 DTB）：SLV_ADDR_SHFT=9/STOP_STRETCH=BIT(2)/I2C 长度寄存器 0x26c/0x270/打包值 0x7f8fe,0xffefe（=geni_se_config_packing(8,4,msb_to_lsb=TRUE) 正是 Linux I2C 取值）/初始化序列（CGC/OUTPUT_CTRL/FIFO 模式/水位/SER_CLK_EN/SCL 计数）逐项等价 —— 驱动配置无错
- 硬件事实三级验证：原厂 DTB pinctrl（qupv3_se19_i2c_active 绑 i2c@0xc88000）、vendor sm8150-qupv3.dtsi（SE19@0xc88000 = wrap2_s2）、vendor sm8150-pinctrl.dtsi（qup19 i2c = gpio57+gpio58）、pinctrl-sm8150.c PINGROUP(57/58) func2=qup19、vendor raphael-sm8150.dtsi goodix@5d（reset=54 irq=122 vdd=98 avdd=100 power-on-delay=300ms）—— 全部与 SWEET 驱动一致；板级触摸节点在 dtbo（基础 DTB 无触摸节点）
- 结论：嫌疑收敛物理层 —— ① avdd（GPIO100）上拉轨没真正起来（ios=0 但采自配置前，无证据力）② 总线被拖死。需要配置后电平证据
- 0091 补丁（a99dc24，CI run 39572579833 success，内核 c8fdc459）：冷上电循环（拉住 reset，98/100 双轨 off 100ms → on）；`bus idle`（vdd/avdd/sda/scl/reset/ios，复位拉住时）+ `bus live`（复位释放+300ms 后）两条判读行；TX 上限 3→6 + 双字 FIFO 打包；touch_stfts 拆写+读两条消息（burst 上限 10 事件保 nr≤90）；停止行带 ios 终值；往返 apply 自检通过
- t10 打包（scripts/t10_package.py）：t7 骨架 + 0091 内核 + t8 的 8 核 ADT（f50510e0，12588B，从 t8 镜像提取并解析验证 8 CPU 节点）；六重校验全过（ANDROID!v2/shim/DTB==t7/ADT 8 核/cmdline 330B==t7/markers/entry 0xfffffe000748b488）；clean e8cd3bc1… debug 4cc1e957…
- 8 核回归的安全性论证：t7（SWEET 编译）用同一个 8 核 ADT 正常启动 → ADT 的 8 核拓扑本身无害；t8 卡死根因 = kryo470 失配（0090 已入库）而非 CPU 数
- 发布 raphael-touch-r18（419348086）：t10 clean/debug + FLASH-T10.md + T10-SHA256SUMS；README 状态区/下载表/刷机指南全面更新；发行版现为 rootfs + r15/t7 + r17/t9 + r18/t10

Stage Summary:
- t9 两定性：触摸 0x5d 超时=总线零活动（软件全对→物理层）；卡=单核
- t10 一次刷机三信息：8 核回归成败 + 总线电平判读（idle/live 分离上拉轨 vs 引擎时钟两大理论）+ ST FTS 真探针
- 判读树：bus idle sda/scl=1 → 嫌疑转串行时钟（DFS/RCG）；=0 → avdd 供电轨问题；probe 0x5d err=0 → 触摸活
- 若 t10 卡 bootstrap → 8 核 ADT 是第二变量 → 退 t9，下轮 2/4 核折半

---
Task ID: 14
Agent: Super Z (main agent)
Task: t10 实测解读（触摸仍死 + 电池识别错误）→ 触摸根因定位（TLMM SOUTH tile）+ t11（0093 触摸根因 + 0092 电池）发布 + 发行版清理

Work Log:
- t10 日志解码（sweet-log.bin 4MiB，本次上传）：8/8 CPUs initialized（8 核回归成功，bootstrap 卡死与卡顿双双关闭）；`bus idle vdd=0 avdd=0 sda=0 scl=0 reset=0 ios=0` + `bus live sda=0 scl=0 irq=0 ios=0`（六个电平读数全零）；probe 0x5d err=60（TX 已发但总线无应答）；probe ST FTS 0x49 err=60（0091 的 5 字节修复生效，命令真的发出去了，同样超时）→ 两个地址完全一致的超时 = 总线物理层死
- 电池问题命中：`SWEET BATTERY: static fallback, 100% 4200mV external=1 charging=0` + boot-args `sweetbattery=0` —— 电池驱动从未开启，iOS 里恒定 100% 是写死的假数据，不是"识别错误"
- 触摸根因定位（0093，f5d231a）：SM8150 TLMM 不是平坦一块 —— GPIO 寄存器 = TLMM 基址 + tile 偏移 + 0x1000×编号（WEST 0x00100000 / EAST 0x00500000 / NORTH 0x00900000 / SOUTH 0x00D00000，pinctrl-sm8150.c 实锤）；触摸 6 GPIO（54/57/58/98/100/122）全在 SOUTH tile（PINGROUP 逐条验证；102-104=NORTH、105=WEST 交错排布，证实必须逐 GPIO 查表）→ 真窗口 = 0x03D00000；驱动从 0083 起映射 0x03000000（整个 TLMM 区开头 0x100000 的未实现空洞，读全零写无效）→ 触摸 IC 从未上电、SDA/SCL 从未复用到 qup19 —— 与 t10 全零判决行完美互证
- 上轮遗留问题修正：work/ 里遗留的 0093 补丁草稿是针对 pre-0091 树生成的（内含已入库的 0091 内容，直接用会冲突）→ 在 work/t11/0093base 重建 post-0091 基准（baseline 构建树 + 应用 in-repo 0091），从 work/xnu 工作树重新提取纯 diff；追加 `TLMM-south` 运行时正向标记（启动行 printing mapping GCC/TLMM-south/SE7），往返 apply 校验通过
- 0092 复核（fa957b0 已在上轮提交并 CI 成功）：PM8150B fg-gen4 QG（SPMI SID 2，peripheral 0x40）真实电压/电流/电量 + SMB5 充电器状态；SweetBattery IOLog 判读行 = `SWEET BATTERY: PM8150B QG / estimated SOC, XX% XXXXmV external=? charging=?`（真实）vs `static fallback`（假数据）
- t11 打包（scripts/t11_package.py 更新）：t7 骨架 + 0092+0093 内核（CI run 39582204279，内核 SHA 63dba04e…，二进制标记逐项验证：TLMM-south 在/旧串消/PM8150B QG 在/PM6150 消/kryo485 在）+ t8 8 核 ADT + cmdline sweetbattery=0→1；六重校验全过（ANDROID!v2/shim/DTB==t7/ADT 8 核/cmdline battery ON/markers 0090-0093/entry 谱系一致）；clean bd75bc2d… debug df4690b6…
- 发布 raphael-touch-r19（419415295）：t11 clean/debug + FLASH-T11.md + T11-SHA256SUMS；同时删除被取代的 r17/t9（单核）与 r18/t10（诊断使命完成）—— 发行版现为 rootfs + r15/t7 + r19/t11（落实用户"只保留 t7 和 ios16 + 当前版"指令）
- 文档：README 状态区（8 核实机确认/触摸根因/电池真相）、下载表（t11 推荐 + t7 回退）、刷机指南改指 t11、docs/FLASH-T11.md 入库；worklog 双写

Stage Summary:
- 本轮双根因：触摸 = TLMM SOUTH tile 基址错误（0083 起所有 GPIO 写入落空）；电池 = sweetbattery=0 静态假数据
- t11 一次刷机双判决：触摸看 `TLMM-south` 标记 + bus idle/live 电平 + probe 0x5d（err=0 即活）；电池看锁屏百分比（不再恒 100% 即活）+ `SWEET BATTERY: PM8150B QG / estimated SOC` 行
- 若触摸仍超时且 sda/scl=0 → GPIO 使能≠电源轨真起来，下轮查供电轨真身（RPMh/LDO 投票）；若 err=6/NACK → 总线活了但器件地址或 IC 异常
- 电池若仍 static fallback → 回报 SPMI 判读行（mapping failed / unsupported arb / timeout），下轮排查 SPMI arbiter 配置

---
Task ID: 15
Agent: Super Z (main agent)
Task: t11 实测解读（触摸总线已通但帧不动 + 电池仍 fallback）→ 爬透 raphael 逆向资源 → 0094 GT9886 配置下发 + 0095 电池 sid 修复 → t12 发布

Work Log:
- t11 日志解码（sweet-log.bin 4MiB）：8/8 CPUs（延续）；`mapping GCC/TLMM-south/SE7` 0093 生效；`bus idle vdd=0x3 avdd=0x3 sda=0x1 scl=0x1`（供电轨+总线全通）；`probe 0x5d ver-reg err=0`（IC 应答！32 字节真实读回）但 `probe raw 0x8240: 00 00 02 d2 ff ff f9 ad...` 非 ASCII；`selected GT9889` 后每次轮询 `report err=5 irq=0x40000001`（DONE+TX_WM、无 RX、无 NACK）；电池 `SPMI arb=0x50000000 APIDs=512` 但仍 `static fallback`（无 timeout 行 = 静默失败）
- 逆向研究（用户点名要求）：爬 LineageOS/android_kernel_xiaomi_sm8150（raphael 内核）—— drivers/input/touchscreen/xiaomi/goodix_driver_gt9886 全套（core/i2c/cfg_bin/gtx8_update）+ raphael-sm8150.dtsi 触摸节点 + firmware/goodix_gt9886_cfg_f11.bin.ihex（vendor 配置 blob 就在内核仓库）+ drivers/power/supply/qcom/qg-reg.h + pm8150b.dtsi + Linux 主线 spmi-pmic-arb.c + i2c-qcom-geni.c，全部下载到 work/ref/ 供后续轮次引用
- 触摸三大发现：① GT9886 "normandy" 寄存器图来自 cfg bin reg 表（vendor 直接使用）：pid@0x4535=ASCII "9886"、sensor-id@0x4541(mask 0x0f)、版本块@0x452c(72B 和零)、命令@0x6f68、配置窗口@0x6f78、**触摸帧@0x4100**（bit7=触摸事件、b[1]&15=点数、8字节点、整帧 4+8n 字节和为零、清帧=写 0）—— t8 以来读的 0x8240/0x824E 只是碰巧 ACK 的地址；② IC 每次开机必须下发 784 字节面板配置（0xff→0x80→0x82→写 784B→0x83→0xff 命令握手 + doze 0xAA/0xBB 协议），不下发就没有触摸帧；解析 vendor blob（ihex→bin→2×905B 包=const(56B)+reg表(65B)+784B 配置，双供应商 sensor0/sensor1，44 bag 校验全零）→ 生成 bsd/dev/gt9886_cfg.h 嵌入内核；③ t11 轮询全灭元凶 = 0x49 ST FTS 探针 NACK 后 TX FIFO 残留 5 字节投毒（每次后续传输先发陈旧字节→DONE 提前→sent!=nt→err=5，与 t11 日志完美互证）→ 探针删除 + touch_msg_run TX 改水位驱动流式写入（单条写上限 6→800 字节，配置写 786B 单条完成）
- 电池根因：SPMI 观察者命令对照 Linux pmic_arb_fmt_cmd_v2 = `opcode<<27 | sid<<20 | addr<<4 | len-1`，我们漏了 sid 位域 —— SWEET（PM6150=SID0）无害，raphael PM8150B=SID2（pm8150b.dtsi fg-batt-soc@4000 + 中断声明双重确认）每次都读错 PMIC → type!=0x0D → 静默 fallback；修复 + SweetBattery::start 加一次性 11 字段探针（type/sub/st1/st2/v/i/usb/path/chg/err/temp 逐字段带错误码）
- 0094 补丁（834b776）：GT9886 全套 bring-up（pid 校验非"9886"则带证据停止——固件更新路径未建；sensor-id 选配置；sensor0=x2x+y2y 翻转 per DT；doze 禁用后保持唤醒适配轮询模式；request 事件记一次+清帧）；0095 补丁（130937a）：sid 位域 + 探针；CI run 39588429883 一次通过，内核 78cbcaaf…，标记逐项验证（GT9886 新行全在/0x49 与 GT9889 旧串全消/两份 784B 配置字节级嵌入验证）
- t12 打包（scripts/t12_package.py）：t7 骨架 + 0094+0095 内核 + 8 核 ADT + cmdline 同 t11；六重校验 + 配置嵌入断言全过；clean 3725736f… debug 8dc5b5a9…
- 发布 raphael-touch-r20（419459117）+ 删除 r19 → 发行版现为 rootfs + r15/t7 + r20/t12；README 状态区/下载表/刷机指南/docs/FLASH-T12.md 全面更新；worklog 双写

Stage Summary:
- 本轮双根因均来自对 vendor 实现的忠实对照（不再猜测）：触摸 = 寄存器图错 + 缺配置下发 + 0x49 投毒三重叠加；电池 = SPMI 命令缺 sid 位域
- t12 判决：`GT9886 pid err=0 [39 38 38 36]` → 身份确认；`cfg send … err=0` → 配置入 IC 上手滑；`SWEET BATTERY: probe type=0/0xd` → 电池读通；方向不对回报 sensor id/flip 值
- 兜底：pid 非 9886 → 固件下发路径（gtx8_update.c 已在 work/ref 备好）；cfg send 卡 0x82 → 回报握手行；电池 probe 有 -1 → APID/observer 层问题（dump APID 表）

---
Task ID: 16
Agent: Super Z (main agent)
Task: t12 实测解读（触摸闭环 + 电池仍 fallback）→ fg-gen4 真根因 → 0096 电池重写 → t13 发布（含 CI 假内核事故修复）

Work Log:
- t12 日志解码：触摸判决行全绿（pid [39 38 38 36]=ASCII "9886"、sensor id=0x1、cfg send sensor1 784B err=0 → 用户确认触摸正常）；电池探针 `type=0/0xd sub=0/0x10 st1=0/0 st2=0/0xce v=0/0 i=0/0 usb=0/0x26 path=0/0x2 chg=0/0x47`（格式 err/值）→ sid 修复生效、读路径通，但 V/I 全零仍 fallback
- 真根因研究（fetch LineageOS sm8150 fg 全家桶到 work/ref/fg/：qpnp-fg-gen4.c 213KB + fg-util.c + fg-reg.h + smb5-reg.h + smb5-lib.h）：PM8150B 无 QG（那是 sm8250+），是 fg-gen4 双外设：fg-batt-soc@0x4000（monotonic SOC +0x09/+0x0A 影子、0-255、vendor 百分比公式）+ fg-batt-info@0x4100（SYS_BATT +0x07、VBATT +0xA0/+0xA6、IBATT +0xA2/+0xA8；换算 uV=raw×122070/1000、uA=sign16×488281/1000）；t12 的 `st2=0xce` 就是 SOC 206/255≈80%，数值一直都在被误读；0x4008=SLEEP_SHUTDOWN_STS 恒 0 → t12 "无电池"判定直接 fallback
- 0096 编写：SweetBattery raphael 段全面重写（影子校验读 fgReadShadowByte/Word、fgMonotonicPercent、SMB5 语义 external=USBIN_PLUGIN bit4、charging=状态机 1-4、full=TERMINATE、时间剩余估算、真低电警告/临界仅实测有效、百分比变化日志、publish 增加 timeRemaining 参数）+ sweet_spmi.c 白名单补 0x41
- CI 事故一（假内核）：v1 补丁在本地树（历史 Edit 工具 tab→空格污染）上生成，CI 规范树 tab 缩进 → `patch failed: SweetBattery.cpp:55` 被 dxb 静默跳过 → run 39593077146 绿灯但内核无 0096（靠 t13_package 的标记校验抓到）；修复：下载该 run 的 xnu-buildtree 作为规范基，脚本化重建（scripts/rebuild_0096.py，to_tabs 转换 + token 级等价校验 + 往返验证）
- CI 事故二（我方 YAML）：防呆闸步骤名含冒号破坏 YAML → run 39594849054 无 job 即败 + dispatch 422；修复：引号包名 + 防呆闸限定 NNNN-(raphael|sweet)- 模式（blacktop 上游补丁 TrustCache/skywalk_sonoma 等每次都良性跳过）
- CI 事故三（真编译错）：publish 声明带默认参数 `int timeRemaining = -1` 但定义漏列第 10 参数 → 函数体 timeRemaining 解析为基类 getter（IOPMPowerSource::timeRemaining）两个报错；修复：定义补全参数（v3）
- v3 构建（run 39596471069 SUCCESS，内核 c094d9ed…）14 项标记逐项验证全过；t13 打包六重校验全过（clean 3d31006c… / debug 8a5fe04f…）
- 发布 raphael-touch-r21（419516832，4 资产）+ 删 r20 → 发行版 = rootfs + t7 + t13；download/ 刷新为 t13 件；README 状态区/下载表/刷机指南/补丁链更新；NOTES.md 存档 fg-gen4 全套研究 + DRV8846 升降摄像头马达（TLMM 21/33/35/9/130 + PM8150L LPG ch1，ramp 1.6kHz/谐振 19.2kHz，差 pm8150l.dtsi 考证）+ 振动马达线索（leds-qpnp-vibrator-ldo，PM8150L）+ 流程教训

Stage Summary:
- 电池根因三连击闭环：0092 假数据 → 0095 sid → 0096 fg-gen4 真寄存器图（vendor 逐字节对齐）
- t13 判决：`probe type=0/0xd sys=0/0x0 msoc=0/0xXX,0xXX(NN%)`（两值相等=影子过）+ 锁屏不再恒 100% + 插线闪电 → 全胜；百分比冻结但电压动 → 下轮 fg-memif 电池 profile
- 工程加固：补丁必须对 CI buildtree 规范基生成；CI 对板级补丁跳过即红
- 彩蛋已备料：升降摄像头马达全协议在案（NOTES.md）

---
Task ID: 27
Agent: Z.ai (main)
Task: t13 重建版真机不亮屏 → 根因定位（v1 头打包错误）→ 用户已知良好 t12 接替 t7（r15）稳定发行版地位（r28）

Work Log:
- t13 重建版/ t19 双双黑屏的根因（镜像解剖三重对照）：用户正常 t12 与 t17 上传镜像均为 **boot header v2**（DTB 独立段，dtb_addr=0x1F00000），而我用 package-raphael.py 打包的 t19/t13 均为 **v1**（DTB 拼进 kernel 段）——diag 轮早已实证"v2 头 = 唯一被 ABL 接受的格式"（v1 下 shim 拿不到独立加载的 DTB，直接黑屏）。教训：打包工具必须与历史可用镜像同构，改打包链前先解剖已知良好样本。
- 用户 t12 上传件 sha256 3725736f… 与 r20 原版 release 资产逐字节一致；用户亦将该文件直接上传至仓库根（commit ee4d2d1）。
- "改成 8 核"经三重验证为无需修改：cmdline cpus=8 + ADT 8×kryo485 CPU 节点（cpu0-7）+ t11 真机日志 8/8 CPUs。对已知良好资产零改动 = 最大安全。
- r28 稳定版构建：clean = 用户 t12 逐字节拷贝（零改动）；debug = 仅 1 字节补丁（payload cmdline sweetfb_noconsole 1→0，v2 结构/shim/DTB/内核全不动），构建脚本断言：源哈希 == r20 记录、全文件恰 1 字节差异、payload 魔数与 t12 身份标记（GT9886/cfg send/DWC3 GSNPSID 在位；fg-gen4/SUSPHY cleared 不在）。
- 发布 raphael-touch-r28（Latest）：4 资产 api_size 与公网回读 sha256 全等（Task 20 纪律）；删除 r15 release+tag（t7 稳定位由 t12 接替）；发行版 = r28(t12 稳定) + r27(t19, 标注勿刷) + rootfs。
- README 双语更新：状态表降级 t14~t19 实验功能（显示缩放/亮度/息屏/充电轮询/SPMI 门/USB 全部标注已回退，稳定版不含；电池降为探针读通——fg-gen4 系 t13），下载表 t12 升为当前首刷推荐、t19 标注勿刷（v1 头）、t7 行移除，哈希块补 t12-debug 与勿刷标注。
- 注：t14~t19 各轮详情见 git 历史（0f84efc..1999311）；仓库树已回退至 t13 基线（a7a925e），稳定镜像为 t12 时代内核（补丁 0001-0095）。

Stage Summary:
- 稳定发行版 = r28/t12：8 核 + GT9886 触摸闭环 + 电池探针 + v2 头正确打包；clean 与当年实测可用文件逐字节一致
- v1 头黑屏根因已归档（t13 重建失败、t19 黑屏、乃至 t8 时代"v1 不被接受"三次事故同源）
- 后续若重启 USB 线：先用 v2 工具链（repack-boot-v2）重打包再谈内核功能

---
Task ID: 28
Agent: Z.ai (main)
Task: 用户指令"t12 不要加 USB 了" —— 冻结 t12 稳定线，禁止任何 USB 添加

Work Log:
- 核实 r28 双资产（clean/debug）cmdline 均 sweetusb=0：USB 探测/gadget 在 boot-arg 层面彻底关闭；内核中的基线 USB 代码（0072/0073/0087，0085 轮产物）保持休眠，与实测可用的原 t12 行为逐字节一致
- 文档扫描：FLASH-T12.md 仅废弃说明 + 电池探针 usb= 遥测字段（PMIC 充电类型，非 USB gadget）；README 无任何 USB 计划，USB 行已标 ⛔ 已废弃
- 决策记录：t12 稳定线（r28 及后续 t12 系资产）**永久不做 USB**——不启用 sweetusb、不打包 v2 USB 镜像实验、不在稳定资产上做任何 USB 变体；仓库树 0001-0096（含休眠 USB 基线补丁）保持不动（与稳定内核血缘一致，删除反而制造分叉）

Stage Summary:
- t12 稳定线冻结于无 USB 状态；任何未来 USB 尝试（如有）必须走独立实验分支，绝不触碰 r28 稳定资产

---
Task ID: 29
Agent: Z.ai (main)
Task: 用户指令"好了开始出t13" —— 从回退树（a7a925e，补丁 0001-0096）重建失传的 t13（fg-gen4 硬件电量计）镜像，v2 工具链正确打包

Work Log:
- 环境再次重置（GH_TOKEN 丢失、.git/config token 被 REDACTED）；仓库树仍在 e20ac84（= a7a925e 回退树 + 用户 t12 上传 + r28 文档），xnu/patches 99 文件（0001-0096 完整，0097-0104 已随回退移除）。
- 确认 CI 已替我们建好内核：xnu run #36（run id 39730368579，a7a925e，2026-10-22 06m16s success）产物 xnu-kernel（artifact id 12404184990，60MB，未过期，约 10-29 失效）；r21（原 t13）release 已删除只剩 tag，原版镜像/内核在公网不可恢复。
- 根因考古（t19_package.py 源码实锤）：v1 黑屏事故 = package-raphael.py 用 --header_version 1 且把 DTB 拼进 kernel 段；正确 v2 链 = mkpayload.py + mkbootimg.py --header_version 2（kernel=shim、ramdisk=payload、DTB 独立段 @0x1f00000、文件在 DTB 末端截断）。
- v2 工具链字节级证明（scripts/t13_prove_v2_toolchain.py）：从黄金 t12（3725736f…）抽取 shim/payload/DTB → mkbootimg（--base 0 --pagesize 4096 --cmdline 'console=null' --os_version 11.0.0 --os_patch_level 2021-10）+ 截断规则 → 复现 t12 除 20B id[] 记账字段（原管线对重打包前输入计算、ABL/shim 均不读，README-repack 有档）外逐字节一致；材料与 Task 27 的 t12-materials 三重交叉全等（shim 9fbe8fb8 / DTB 2a1ec87b / ADT f50510e0=12588B 8 核）。
- 标记判别集审计（scripts/t13_marker_audit.py，对照 t12 内核区(0095) 与本地 t19 内核(0104)）：'SWEET FB: AppleCLCD'/'QSCRATCH HS_PHY_CTRL'/'SWEET RPMH:'/'VBUS override'/'ACTIVE vote complete' 在 t12 休眠基线里早已存在，不是 t14+ 判别子；已剔除。终版：PRESENT = t12 身份（GT9886/cfg send/DWC3 GSNPSID）+ 0096 fg-gen4（PM8150B fg-gen4 / probe sys= / msoc=）三项；ABSENT = 0097-0104 每补丁至少一串共 17 项。
- 生产线就绪（scripts/t13_build.py）：mkpayload(t13 内核, t12 ADT, t12 cmdline 逐字) → mkbootimg + 截断 → debug=1 字节（sweetfb_noconsole 1→0）；六重校验（v2 头律/shim+DTB 逐字节/payload 魔数+cmdline/ADT 逐字节+内存几何/标记在场+缺席/sha256）。负向冒烟（t19 内核喂入）：机械链全过、标记门以全 17 项泄漏正确拒绝 —— 管线双向验证。
- 阻塞项：GH_TOKEN（拉 CI artifact、push、发 r29 均需）。fetch/release 脚本已备好（scripts/t13_fetch_kernel.py / scripts/t13_release.py，后者 make_latest=false 保 r28 稳定位）。

Stage Summary:
- t13 重建万事俱备：内核在 CI run 39730368579（未过期）、v2 打包链已字节级证明、构建/校验/发布脚本三件套就绪、t12 材料交叉验证一致
- 一旦 GH_TOKEN 到位：fetch → build（六重校验）→ 文档（FLASH-T13/README/WORKLOG）→ push → r29（make_latest=false）→ download/ 刷新
- 教训归档：mkpayload 非幂等（chained fixups 原地改写、payload 内核区不可回喂）；t14+ 判别标记必须双内核对照审计后使用

---
Task ID: 30
Agent: Z.ai (main)
Task: 用户送达 GH_TOKEN（"以后要保留凭据"）→ t13 重建流水线全程执行：fetch → build → 文档 → push → r29 发布 → download/ 刷新

Work Log:
- 凭据持久化：GH_TOKEN 写入 .env（.gitignore 补 .env 防泄漏）+ work/ios-raphael/.git/config remote URL；token 三重验证通过（api /user → Gsjsjzhznsz；repo 访问 200；fetch 后远程 main = 本地 = e20ac84 零漂移）。
- fetch（run 39730368579 / artifact 12404184990，60MB zip）：内核选型事故与修复——原"取最大 kernel.*"策略选中 .link/ 下 134MB ar 归档（kernel.development.raphael.a）而非顶层 17.2MB 真 Mach-O，t13_build 魔数断言正确拦截；t13_fetch_kernel.py 已改为 SHA256SUMS manifest 优先 + Mach-O 魔数（cf fa ed fe）校验 + 排除 .link/.dSYM/.a。内核 = kernel.development.raphael 17,206,304B，sha256 bd6ad8be…（与 CI manifest 全等；与原版 r21 内核 c094d9ed… 不同 = runner 工具链漂移，按预案以标记集判定，功能同一补丁集 0001-0096）。
- build（t13_build.py）：六重校验全过（v2 头律 / shim+DTB 逐字节=t12 / payload 魔数+cmdline / ADT 逐字节+内存几何 / 标记 t12 身份+0096 fg-gen4 全在场、0097-0104 全 17 项缺席 / sha256）；双镜像均 17,326,191B；debug 恰 1 字节 @0x33172（sweetfb_noconsole 1→0，与 t12 debug 同位）。clean = 82c71576…，debug = 4f1f5f65…。
- 文档：FLASH-T13.md 升级为 r29 版（重建说明块 + 原版判读树全保留；回退点 t7 → t12/r28；内核哈希行更新）；README 双语五处（状态表电池行 ×2、下载表 t13 两行 ×2、哈希块 t13-orig/r29 三行）；WORKLOG 补档 Task 28/29/30。
- 发布：raphael-touch-r29（make_latest=false，r28/t12 保持 Latest）；4 资产（clean/debug/FLASH-T13.md/T13-SHA256SUMS.txt）api_size + 公网回读 sha256 双通道全等（Task 20 纪律）。
- download/ 刷新为 t13 四件套（t12 四件套移除；t12 仍可从 r28 release 获取）。

Stage Summary:
- t13 以 r29 重回测试轮：功能与失传的原版完全同一补丁集（0001-0096），打包首次用字节级验证的 v2 链
- 凭据新常态：GH_TOKEN 常驻 .env + git config（.env 已进 .gitignore），后续会话免再索要
- 教训：CI artifact 内可能混有链接中间产物（.link/*.a 134MB > 真 Mach-O 17MB），选核必须走 manifest + 魔数双验证，"取最大"不可靠

---
Task ID: 31
Agent: Z.ai (main)
Task: 用户指令"好了把屏幕缩放搞一下" —— 重建 t14 时代被回退的整幅缩放（sweetscale），仅缩放子集，发布为 t20/r30

Work Log:
- 考古（a7a925e^ 树）：恢复 0097-raphael-battery-charging（不取）与 0098-raphael-display-scale-brightness（四合一：缩放/亮度 DCS/即时息屏/externalMethod 调参日志）+ FLASH-T14.md；实锤"历轮 USB 楔死都发生在 t14 功能用户态可见之前"——**缩放从未被肉眼验证，t20 是首次真机视觉测试**；t12 cmdline 无 sweetscale → 补丁默认 scale=1 即缩放开。
- 拆分（scripts/t20_make_patch.py）：规范基 = work/xnu 文件反向应用 0098（零 fuzz）→ 0001-0096 基线；KEEP 缩放 hunks（74 注释/104 成员/176 映射表构建/359 present 重构）+ presented-as 日志行（混合 hunk 197 切片）；hunk 359 捎带的 setPanelBrightness 函数块经"s->release() 尾段恢复"手术剔除；diff -u 生成新补丁 0097-raphael-display-sweetscale.patch（5 hunks，200 行），往返+纯度+排除指纹三重验证。纯度误报两例修正：`scalarInput[0]` 撞基线合法用法（改指纹 `(%llx %llx %llx %llx)`）；补丁生成中 diff 退出码 1 语义修正。
- CI 三折：push a7aed26（0097 补丁）→ dispatch 误杀在跑的 push run #37（并发组 cancel-in-progress）；dispatch #38/#39 两连死（runner 秒分配即取消、零步骤=池故障）；重派 #40 success（内核 7440577a…，run id 39761972114）。**"push 触发已死"被证伪**：`branches: [main]` 一直在文件里，"ain]" 是显示管道吞 "[m" 的 ANSI 伪影（od 实锤）；一次据错误认知的"修复"提交因 sed 空操作+git nothing-to-commit 被拦，未污染仓库。
- 判别标记定版：PRESENT = t12 身份 3 + 0096 fg-gen4 3 + `sweetscale`/` (scaled)`（对 r29 内核原始扫描验证纯净）；ABSENT 16 项（0097-charging + `SWEET FB: brightness`=拆分证明 + 0099-0104）。原始扫描 PASS；负向冒烟：r29 内核喂入 t20 管线被标记门正确拒绝（missing: sweetscale pair）——管线双向验证。
- 构建（t20_build.py，v2 链）：双镜像 17,326,191B；六重校验全过（v2 头/shim/DTB 逐字节=t12/payload 魔数+cmdline 与 t12 全等/ADT+几何/标记/sha256）；debug 恰 1 字节 @0x33172（与 t12/t13 debug 同位）。clean e32618b9… / debug 6c272111…。
- 文档：FLASH-T20.md（判读树 A 显示层全表：顶角完整=全胜、`(scaled)` 判决行、颗粒感=预期、Display Zoom 真实生效、撕裂回报；回退 t12/r28）；README 双语 7 锚点（显示行 ×2、t13 降上一轮 ×2、t20 双行 ×2、哈希块）；NOTES 存档（设计/拆分/判别/CI 教训）；本 WORKLOG。
- 发布：raphael-touch-r30（make_latest=false，r28 保持 Latest）；download/ 刷新为 t20 四件套。

Stage Summary:
- t20 = 0001-0096 + 0097-sweetscale；功能首次上真机，判决 = 锁屏顶角时间/电量完整 + 启动行 `(scaled)` + Display Zoom 有视觉变化
- 教训双档：①CI"沉默跳过"防闸继续救命（本轮是 runner 池故障三连，第 4 次尝试通过）；②终端显示会吞 "[m"（ANSI 伪影），字节级判断必须 od -c，勿凭 grep/cat 改工作流

---
Task ID: 32
Agent: Z.ai (main, 接替 GLM 5.3 继续维护)
Task: 用户指令"把亮度/息屏和电池充电状态修复一下" —— 恢复 t14 时代被回退的 0098 亮度/即时息屏子集 + 0097 充电监视子集，发布为 t21/r31

Work Log:
- 考古（5e53979 = r22/t14 提交）：取出历史 0097-raphael-battery-charging（214 行）与 0098-raphael-display-scale-brightness（476 行）；0098 的缩放子集已在 t20 作为新 0097 回归，本轮取余下全部：DSI ctrl0 DCS 路径（0x51 亮度 11 位大端 ≤2047 / 0x28-0x29 息屏点亮）、即时息屏（RequestPowerChange(0) 一次 memset + 面板真睡眠，sweetfb_fade=1 回旧渐暗）、sweetfb_bright= 启动档位、setProperties 亮度钩子、externalMethod 标量日志。
- 补丁重建法：从创建补丁（0046/0062）提取 pristine 文件，按序回放链条到当前树（FB: 0054,0056,0061,0067,0079,0097；BATT: 0092,0095,0096），历史补丁 dry-run 定位冲突（FB 11 hunks 中 5 个撞 0097 已有内容；BATT 干净全过）；FB 剔除缩放 hunks 后其余照抄，hunk-7 的 IOLog 尾巴与 0097 冲突 → DSI init 部分手工移植。
- 两处超出历史补丁的强化（NOTES 存档）：① IOUserClient::setProperties 基类不转发（Apple 自家 IOHIDEventServiceUserClient 手动转发 _owner 为证），历史钩子只挂 AppleCLCD 永远够不到 → IOMobileFramebufferUserClient::setProperties 转发 fFB；② 历史 0097 的 inputCheck 抢先更新 fLastPlugin/fLastChgState，poll() 的转移日志块比较恒等 → 插拔一行 dump 永不触发，本版 inputCheck 只比较不缓存。首版曾加 sel12 探针，CI 编译失败实证 kSweetFBRequestPowerChange == 12（用户日志 boot 后的 "sel 12 in 1/0" 即 display-on）→ 探针删除，日志门升级为每 selector 前 3 次 + 标量值（亮度通路猎取）。
- 全链回放验证：99 补丁（0001-0097 + 新 0098/0099）对 pristine 双文件零 FAIL，最终文件与手改版逐字节一致。
- CI：push 9f3bdfe 首轮编译失败（duplicate case 12）→ d13299c 修复后 run 39788229692 success；kernel.development.raphael 17,223,216B（cdd15b5b…）；LC_DYLD_CHAINED_FIXUPS 实测不存在 → fixups=0 属实（新工具链无 chained fixups，t20 同）。
- 金标准材料：r30 Release t20-clean（e32618b9…，objects.githubusercontent 单线程 ~90B/s 不可用，改 API octet-stream + 12 并行 1MiB 分段 25s/17MB）逐字节校验后抽取 shim(9fbe8fb8)/DTB(2a1ec87b)/ADT(f50510e0,12588B)/cmdline(330B 逐字)；shim+DTB 与 diag5 提取件全等（Task 27/28 档案吻合）。CI artifact 60MB 走 Azure blob 签名 URL 25s 下完（release 资产限速、artifact 不限速）。
- 构建（t21_build.py）：自检模式用 t20 自身四件重组 = 逐字节复现（装配器证明）；正式构建六重校验全过（payload 魔数/nsegs=10/virt+entry 同基址/phys=0x1_00000000 mem=0xFCC00000/ADT 内嵌逐字节/cmdline 逐字/标记 14 项在场 + 9 项 USB 轮缺席判别串全缺席/头页仅 ramdisk_size 4 字节差/debug 恰 1 字节 @0x33172）。clean 312259a1… / debug e7f19586…（17,342,575B）。
- 文档：FLASH-T21.md（判读树：A 显示 DCS 判决行 + 即时息屏表 / B 亮度滑条三通路判读 / C 充电 5s + 转移行全表 / D-G 回归与取证 / H 回退）；README 双语（状态行 ×2、下载表 ×2、哈希块）；本 WORKLOG。
- 发布：raphael-touch-r31（make_latest=false，r28 保持 Latest）；4 资产（clean/debug/FLASH-T21.md/T21-SHA256SUMS.txt）。

Stage Summary:
- t21 = t20 底座 + 0098（亮度 DCS/即时息屏/setProperties+userclient 转发/前3次标量日志）+ 0099（5s 充电输入监视/插拔转移全dump/inputCheck 缓存 bug 修复）
- 亮度滑条本轮目标 = 通路识别（setProperties 或 externalMethod 某 selector，日志必留痕）；息屏目标 = 瞬时全黑 + AMOLED 真睡眠；充电目标 = 插拔 ≤5s 状态栏出闪电
- 教训：①生成补丁一律 diff -u（漏 -u 出 ed 格式）；②Edit 工具会整体展开 tab——补丁链源文件编辑必须用 python 字节级替换；③标记判别串必须先对基础链验纯（GUSB2PHYCFG/QSCRATCH 等在 0074 基础 USB 补丁里就有）；④kSweetFBRequestPowerChange==12，别把 display-on 的 sel12 当亮度

---

## Task 33 (r32/t22) — 亮度真通路 sel 50 + DSI 双控制器探测 + AppleSmartBattery（2026-10-22）

**输入**：用户回报"亮度无法调节，充电状态还是没有"+ 新 sweet-log.bin（t21 实测，8331 行）。

**t21 日志三大判决**：
1. **电池内核侧全通**：probe 全 0 错误；52%→54% SOC 跟踪；插充电器瞬间 `input plugin=1 apsd=DCP(0x8) icl=0xa(500mA) path=0x95 st5=0x99 chg=FULLON(0x3) charging=1`，拔掉回 plugin=0。缺口只剩内核→UI 的**消费端**。
2. **DCS 门禁误杀**：`DSI ctrl0 ctrl=0x20030000 (engine off; DCS disabled)` —— 面板明明亮着，ctrl bit0 不是可靠的存活信号，t21 整个 DCS 路径（亮度+息屏面板睡眠）被自禁。
3. **亮度通路实锤**：Apple Wiki `Dev:IOMobileFramebuffer` selector 表与实机日志互证（sel 6=swapWait (1,0x80000000,0)、sel 12=requestPowerChange、sel 8=getDisplaySize out2、sel 5 swapEnd 1132B 全吻合）→ **sel 50 = setBrightnessCorrection**（iOS 8+ 从 49 改 50），日志捕获 backboardd `sel 50 in 2/0 (10000 0)` ×2 —— iOS 亮度内部 0..10000 满刻度。t21 未实现（default 直通返回 0x0）→ 滑条无效。

**方法**：
- SweetBattery.cpp 重建 = 0062 pristine + 0092/0095/0096/0099 回放（25133B 零 FAIL）；SweetFramebuffer.cpp = 0046→0098 十补丁链回放（891 行）；KernelConfigTables.cpp = apple-oss-distributions xnu-8796.101.5 pristine + 8 补丁回放。
- iPhone X (iPhone10,3) **iOS 16.7.16 (20H392，与 rootfs 同版本)** kernelcache.zip64 Range 下载到手（kernelcache.release.iphone10b，IMG4/bvx2=LZFSE 未解压——本轮无需）。
- **0100**：sel 50 处理器（0..10000 → DCS 0x51 电平 4..2047 感知表映射，低端可用、满档 2047）；DSI 存活改真实命令自证（ctrl0 0x0ae94000 → ctrl1 0x0ae96000 各试 DCS 0x29，先完成 command DMA 者为宿主）；DSI 未通时亮度请求记 pending、亮屏补发；setProperties dump 全部键名；sel 50 每次必记日志、其余 selector 上限 3→8。
- **0101**：personality 加 `'name' = AppleSmartBattery`（类名 SweetBattery 不变）——iOS 状态栏按名找电池；判决行 `registering as AppleSmartBattery`。

**工程**：11 锚点全中；t22_build.py 自检（t20 字节级复现）+ 六重校验（cmdline 逐字/ADT 内嵌逐字节/头仅 ramdisk_size 差/18 标记在场/10 判别串零泄漏/debug 恰 1 字节 @0x33160）；git 协议推送连续 TLS/HTTP2 失败 → **contents API 推送**（c3ae623 + 3fc8f36）；CI run 39804489250 被并发组级联取消（无日志）→ rerun attempt 2 = **success**（39807718995，kernel.development.raphael b02a278b…）；Release `raphael-bright-t22`（id 420942528）3 资产，服务端 digest 与本地逐字节一致（clean 9fade8f0… / debug 0b4fa757…）。

**自动亮度定性**：ALS → CoreBrightness → **同一条 sel 50**。t22 修通 sel 50 = 自动亮度的执行端就绪；缺的是 ALS 传感器数据（I2C 光感驱动 + AppleALS HID 事件）→ 下一轮任务（MiCode raphael-p-oss dtsi 定芯片型号）。

**t25 实机（2026-10-24 22:55 抓取）**：stage 1a 的 GDSC 时序成功——`cx_gdscr 0x222001 -> 0xf8222000`（bit31=1）、`cx_hw_ctrl=0x80001fe0`（bit31=1），`stage1: CX is on`；随后 `ahb=0x1 running`（CLK_OFF 清除）。但 `crc_ahb=0`、`cx_apb=0x80000000`（enable 未置位），被脚本的 `must=true` 判成 FAIL 并中止，GBIF 未读。判断：SM8150 上 crc_ahb/cx_apb 很可能由硬件控制（enable 位不可写），不是故障；0105 将两者降级为非致命并改为先 cxo/cxo_aon 后 ahb/crc_ahb/cx_apb，然后无条件读 GBIF。整机未卡死（SpringBoard 正常跑，日志尾部是 process sampler 的 SMP 采样，不是 panic）。

**构建与产物（2026-10-24 22:52）**：CI run 40103285547 success；kernel 17,256,352 B / `ea3baa9e…`；镜像 `raphael-t25-gpupower-clean/debug.img` 17,358,959 B，clean `31671eed…` / debug `183df83c…`（clean/debug 单字节差 @0x33172，shim/DTB/ADT/cmdline 与 r33 逐字节一致）。已 boot clean 待实机判读。

**下一轮判决点**：①启动行 `DSI ctrl0/ctrl1 probe ok`（谁宿主）；②拖滑条 `sel 50 (NNNN)` → `brightness NNN` 行 + 屏幕是否跟随；③插电 ≤5s 闪电 + `registering as AppleSmartBattery`。

---

## Task 34 (r33/t23) — 触摸冷启动重试阶梯（GT9886 PID 垃圾值破案）（2026-10-22）

**输入**：用户回报"怎么无法触碰了"+ 新 sweet-log.bin（t22 实测，3102 行）。

**t22 日志判决（触摸回归的完整因果链）**：
1. **IC 完全活着**：`probe xfer addr=0x5d` 全部 err=0（ACK + 时钟拉伸 stretch=1，72 字节配置块读回成功）——I2C/GCC/TLMM/SE 全链健康。
2. **但 RAM 镜像是垃圾**：PID `[22 50 96 ba]`（正常 ASCII "9886"）+ 72 字节版本块 sum=0x8a（厂商保证 0）+ sensor id=0x1。PID/版本块都活在 IC 的 RAM 里 → **flash 固件没跑起来**。
3. **根因定性**：0091 冷启动序列 reset 释放后仅等 300ms 就探测；芯片 flash 引导输了竞速。概率性事件——t12→t21 十余次开机全部赢了，t22 输了一次。**与 t22 的 0100/0101 无关**（不碰 I2C/QUP/TLMM；亮度 sel 50/battery 行在日志里全部正常工作）。
4. 顺带战果：0100 亮度通路实机确认（`sel 50 in (10000)` → `brightness 2047`，亮屏补发生效）；0101 判决行 `registering as AppleSmartBattery` 在场，用户本轮未再报充电缺失。

**0102**：PID 校验失败不再直接 `goto out`，改为冷启动重试阶梯——
- try0：原时序（100ms 断电/20ms 稳定/2ms reset 低/300ms 引导窗）不变；
- try1：200ms 断电 / 50ms 稳定 / 10ms reset 低 / 500ms 引导窗；
- try2：300ms 断电 / 100ms 稳定 / 10ms reset 低 / 500ms 引导窗 + **INT 上升沿 strap**（reset 拉低期把 IRQ 配输出高，释放后 1ms 交还输入——经典 Goodix 引导选择带，先于芯片固件驱动该线）；
- 每档判决行 `SWEET TOUCH: retry N cold cycle off=… settle=… low=… boot=… ms` + `GT9886 tryN pid err=… [..] sensor… sum=…`；任一档 PID 变对即继续原 bring-up（doze disable → cfg 784B → 0x4100 轮询）；三败才停（新文案 `pid is not 9886 after 3 cold boots`），日志留全证据。

**工程**：sweet_touch.c 终态重建 = 0060(创建)→0062→0063→0083→0086→0088→0089→0091→0093→0094 十补丁链回放（1162 行零 FAIL，6 锚点全中；幻影段尾空行/\ No newline 元数据解析修复）；0102 双向验证（git apply --check --reverse + patch -p1 应用结果与手改版逐字节一致）；t23 构建自检（t22 字节级复现）。

**下一轮判决点**：①`GT9886 tryN` 任一档 PID 变对 + 实机触摸恢复（全胜）；②三败（固件升级路径：OrangeFox 从 MIUI vendor 提取 goodix 固件）；③亮度/充电回归确认（本轮零改动）。

---

## Task 35 (r34/t24) — GPU 探针 stage 0：XNU 能否碰到 SM8150 GPU（2026-10-24）

**输入**：用户要求"让 GPU 跑起来"的可行性验证。调研对象 = HackiOS（OOSexGang，2026-10-22 开源）：
- 内核侧 `redwood_adreno.c`（4125 行，GPUCC/GDSC/RPMh/GMU 固件/HFI/SMMU-500/TZ zap）+ `RedwoodGPU.cpp`（1295 行，把 Linux msm DRM ioctl 界面搬进 XNU）；
- 用户态 Mesa 25.0.7 + freedreno（`redwood-drm` 假 libdrm）、`eagl.c`（4205 行，CA 走 GLES 上 GPU）；
- Metal：`redwoodmetal.m`（6333 行，`MTLAddDevice` 注册）+ Mesa AIR 前端 `air_metal.c`（6992 行，AIR→NIR/IR3），公开数据 456 个系统模块 321 个到 ir3（271 fragment + 47 compute 全过，135 visible 未过）。
- 结论：他们不是"让 Metal 跑在 Adreno 上"的魔法，而是三层自建；raphael 的 Adreno 640 在 freedreno 支持范围内，但内核驱动是 SM7325 专用，需按 SM8150 重写。

**本轮范围（刻意保守）**：只回答第一个问题——GPUCC 寄存器块能不能被 XNU 读到。3D 核（GX）一次不碰；GMU 只在时钟已在跑时读身份寄存器；唯一写操作是 GCC `gpu_cfg_ahb`（0x71004）的 enable 位（HackiOS step 3 同款，GPUCC 寄存器文件的时钟）。

**0103（bsd/dev/sweet_gpu.c，273 行 + conf/files/sweet_ufs.c 接线）**：
- 地址：3D 0x2c00000 / GBIF 0x2c0f000 / GMU 0x2c6a000 / GPUCC 0x2c90000；CX GDSC +0x106c、GX GDSC +0x100c、GX reset +0x1008、CX hw-ctrl +0x1540（sm8150.dtsi / sm8150-gpu.dtsi / sm8150-gdsc.dtsi）；GPUCC 分支偏移取 vendor gpucc-sm8150.c；GDSCR bit31=power、CBCR bit0=enable/bit31=CLK_OFF（gdsc.c / clk-branch.h）。
- 流程：30s 延迟线程（`gpuprobedelay`）→ GCC gpu_cfg_ahb 检查/使能 → GPUCC 16 个寄存器只读 → CX/AHB 成立才读 GBIF → cx_gmu 也在跑才读 GMU 身份 + 19.2MHz 自走计数器 → verdict。`gpuprobe=0` 关闭。
- 风险与取证：首读 GPUCC 前先打印 `mapping GPUCC ...`；若 CX 域无电把 NoC 楔死，日志最后一行即死点；卡死可强制重启（fastboot boot 不落盘）。

**工程**：
- 重建 CI 同源树：release.json macos-135 → xnu **8796.141.3**（不是 121.2）；`git apply` 在 Windows 克隆下会因 core.autocrlf=true 把补丁变 CRLF 并拒绝上下文——改用 LF 副本后，0028→0077 九个补丁回放出 sweet_ufs.c（823 行）与 conf/files（650 行），调用点 `sweet_ufs_probe` 链完整。
- 0103 双验证：对重建树 `git apply --check` 通过；应用后 conf/files:114 与 sweet_ufs.c:56/664-665 均为预期内容。
- 文档：FLASH-T24.md（判读树：stage0=OK/FAIL、cx=on/off、GMU 未启动属正常、卡死处理）+ 本 WORKLOG。

**构建与产物（2026-10-24 21:04）**：主仓库账号无写权限（存储凭据 wzhdgithub 仅 pull）→ fork `wzhdgithub/ios-raphael`，推送 f281adc 并 dispatch xnu（run 40096011088，8 分钟 success）。日志实证：0103 于 12:58:12 正常 apply，`sweet_gpu.c` 于 13:00:43 编译（仅 declaration-after-statement 警告），补丁防跳过门通过。kernel.development.raphael 17,223,504B / `f02fc248…`。镜像以 r33 clean 为模板：shim/DTB 逐字节复用、ADT 复用 r33 内嵌件（`f50510e0…`, 12588B）、cmdline 与 r33 一致（debug 仅 `sweetfb_noconsole=0`）；clean `741388fa…` / debug `4731e541…`，均 17,342,575B，clean/debug 单字节差 @0x33172。

**t24 实机（2026-10-24 22:30 抓取，开机 30s 探针）**：12 行 `SWEET GPU:` 全在场，整机无回归（8/8 CPU、`DSI ctrl0 probe ok`、`sel 50 (10000)`、`registering as AppleSmartBattery`、GT9886 选中、按键正常）。
- 唯一写生效：`gcc gpu_cfg_ahb=0x1 (running)`（写前为 0；GPUCC 寄存器时钟成功打开）。
- GPUCC 应答且非 all-ones：`gx_gdscr=0x222001 cx_gdscr=0x222001`（bit31=0，两个 GDSC 都 collapsed）、`cx_hw_ctrl=0x5ff4`（bit31=0）、`ahb=0x80000001`（enabled-but-off）、`cx_gmu=0x80004220`、`cxo=0x80000001`、`gmu_rcgr=0x80000000 src=0`、`cx_snoc=0x80000000`、`gx_gmu=0x80000000`；PLL0/PLL1/CXO_AON/GX_RESET 全 0（无电）。
- 按规则跳过 GBIF（ahb off）与 GMU（cx_gmu off），verdict `stage0=OK cx=off gx=off`。
- **关键结论**：CX GDSC collapsed 时读 GPUCC 不会楔死 NoC（本次全程无卡死）→ stage 1 可以安全做电源时序：清 CX GDSC SW_COLLAPSE → 等 CX_HW_CTRL bit31 → GCC 三个 GPU 时钟 + GPUCC 分支 → 读 GMU 19.2MHz 计数器。

**下一轮（stage 1）**：RPMh CX/MX 投票 → GPUCC 分支时钟 → a640_gmu.bin + HFI → SMMU 页表 → 3D 核上电读 RBBM。沿用 t19 降级中止原则。

---

## Task 36 (r35/t25) — GPU stage 1a：CX 半区上电 + GBIF（2026-10-24）

**输入**：t24 实机日志（stage 0 OK）：`gx_gdscr=cx_gdscr=0x222001`（两个 GDSC collapsed）、`cx_hw_ctrl=0x5ff4`（bit31=0）、`ahb=0x80000001`（enabled-but-off）、`cx_gmu=0x80004220`、`cxo=0x80000001`、`gmu_rcgr=0x80000000 src=0`；读 GPUCC 无卡死 → 下一步可安全做上电时序。

**0104（sweet_gpu.c +102 行）**：按 HackiOS step 4 的 gdsc-regulator 时序打开 GPU CX 半区——检查 bit1 硬件控制位（置位则放弃）→ 清 CX_GDSCR bit0 → 轮询 CX_HW_CTRL(0x1540) bit31 100ms → 若 GX_GDSCR 已有电则停手（脏状态，HackiOS 实测不可用且关闭会挂机）→ 使能 ahb/crc_ahb/cx_apb/cxo/cxo_aon 五分支（ahb 是 BRANCH_HALT_DELAY，状态位不可信，must=false）→ 读 GBIF scache/halt/halt_ack（全 0xffffffff 判 FAIL）→ `stage1=OK/FAIL` 判决行。`gpupower=0` 可只跑 stage 0。

**范围克制**：不碰 GMU 时钟源（需要 GCC GPLL0 分频与 RCG 编程）、不加载 a640_gmu.bin、不碰 GX、不碰 SMMU——全部留给 stage 2；本轮只证明 CX 半区能上电并应答。

**工程**：补丁对 0103 终态 `git apply --check` 通过（373 行结果、14 处 stage1）；构建走 fork wzhdgithub + dispatch xnu，沿用 r33 模板打包（shim/DTB/ADT/cmdline 复用，仅 payload 变化）。

**下一轮判决点**：①`stage1: CX is on`；②`gbif=responding`（stage 1a 全胜）；③失效分支按 FLASH-T25 表回报。

---

## Task 37 (r36/t26) — GPU stage 1b：放宽 Cx 时钟判据 + GBIF 证据（2026-10-24）

**输入**：t25 实机——CX GDSC 上电成功（`0x222001 -> 0xf8222000`，hw_ctrl bit31=1），AHB 0x1 running；但 `crc_ahb=0`、`cx_apb=0x80000000` 被 `must=true` 误判为失败并中止，GBIF 未读。整机未卡死（SpringBoard 正常，SMP 行是 process sampler 采样）。

**0105**：时钟顺序改 `cxo_aon→cxo→ahb→crc_ahb→cx_apb`；crc_ahb/cx_apb 降级为非致命（SM8150 硬件门控，enable 不可写）；时钟段后无条件读 GBIF，`stage1=OK` 由 GBIF 是否 all-ones 决定。

**范围**：仍不碰 GMU 时钟源/固件/GX/SMMU——本轮只补齐“CX 半区活着”的证据。

**工程**：0105 对 0104 终态 `git apply --check` 通过（13 增 14 删）。

---

## Task 38 (r37/t27) — GPU stage 2a：GCC GPU 时钟 + GMU 时钟根 + GMU 身份（2026-10-24）

**输入**：t26 实机 `stage1=OK cx=on ahb=0x1 gbif=responding`（scache=0 halt=0 halt_ack=0x3），CX 半区已活。

**0106（sweet_gpu.c +89 行）**：
- GCC 三时钟：`gpu_memnoc_gfx`(0x7100c, BRANCH_VOTED)、`gpu_snoc_dvm_gfx`(0x71018, BRANCH_HALT)、`ddrss_gpu_axi`(0x71154, BRANCH_VOTED)，只置 enable 并回读；
- GPUCC `gmu_clk_src`(0x1120)：实测 `0x80000000`（CMD_ROOT_OFF=bit31，源=bi_tcxo）；写 `(v & ~BIT31) | RCG_UPDATE` 打开根，保留 19.2 MHz 源（够寄存器访问；200 MHz GPLL0 留作优化）；
- `cx_gmu`(0x1098) 分支使能并轮询 CLK_OFF 20 ms；
- 仅当分支 running 才映射 GMU（0x2c6a000）读 CM3/HFI/RPMH/SPTPRAC 与自走计数器；`stage2=OK/FAIL/PARTIAL` 判决；`gpugmu=0` 可跳过。

**范围**：不加载固件、不碰 GX/SMMU；若 cx_gmu 不起则 PARTIAL，不读 GMU（避免无时钟访问楔死 NoC）。

**工程**：0106 对 0105 终态 `git apply --check` 通过（89 增）。

**t27 实机（2026-10-24 23:53 抓取）**：stage 2a 全胜——GCC `gpu_memnoc_gfx=0x221` / `gpu_snoc_dvm_gfx=0x1` / `ddrss_gpu_axi=0x1`；`gmu_rcgr=0x80000000 src=0`（写 `0x1` 清 root-off 后回读仍为 0x80000000，但 `cx_gmu=0x4221 running`，CLK_OFF 已清）；GMU 寄存器全部可读：`sysreset=1 boot_config=0 fw_init=0 hfi_ver=0 rpmh_state=0xf sptprac=0xc0`；自走计数器 21,197,600 Hz（按固定 10ms 推算偏高的原因：线程实际睡 ~11.04ms，折算回 19.2 MHz 晶振；下轮用 mach_absolute_time 精确测量）。整机未卡死。

**固件提取（TWRP，vendor 为 erofs 逻辑分区 dm-1）**：挂载 `/dev/block/mapper/vendor` 到 `/vendor2`（`mount -t erofs`），拉出 `a640_gmu.bin` 37,680B、`a630_sqe.fw` 32,304B、`a640_zap.mdt/.b00/.b01/.b02/.elf`，存 `D:\K20iOSirmware` 并记录 SHA256SUMS。注意 Git Bash 下 adb pull 目标须用 `D:/...` 正斜杠 + MSYS_NO_PATHCONV=1，反斜杠会被吞。


---

## Task 39 (r38/t28) — GPU stage 3 前置：精确计数器 + TZ/SCM 只读探测（2026-10-25）

**输入**：t27 实机 `stage2=OK gmu=responding counter=21197600 Hz`（固定 10ms 窗口的调度误差约 10.4%，折算 19.2 MHz）。

**0107（sweet_gpu.c +88/-8）**：
- `gpu_counter_hz()`：`mach_absolute_time()` 前后夹住 10 ms 等待，`absolutetime_to_nanoseconds()` 换算真实 Hz（stage 0 与 stage 2 两处都改）；
- `gpu_scm()`：qcom_scm-smc 约定（x0=fnid[SCM_FAST|SCM_64|SCM_SIP|call]，x1=参数描述符，x2..x4 参数，x6=session，`smc #0`，被打断用 x0=1 重试）；
- 只读探测：SCM_IS_CALL_AVAILABLE 自身 → PAS_INIT_IMAGE / PAS_AUTH_AND_RESET 可用性 → PAS_IS_SUPPORTED(PAS_GPU=13)；`gputz=0` 跳过；
- 判决行 `tz=available/unavailable` + `gpu zap status/supported`。

**依据**：HackiOS `redwood_adreno.c` 的 scm()/step_trustzone()（同一 smc 约定与调用号）；zap 镜像已从 raphael vendor 提取（a640_zap.mdt/.b00/.b01/.b02/.elf）。

**工程**：0107 对 0106 终态 `git apply --check` 通过。

---

## Task 40 (r39/t29) — GPU stage 4 前置：SMMU 只读探针（2026-10-25）

**输入**：t28 实机——`counter=19200001 Hz`（精确 19.2 MHz）、`tz=available`、PAS init/auth 调用可用、GPU zap supported=0（语义未知，无驱动依赖）；GMU 固件解析出 TCM 14,892B + DDR 22,676B，DDR 段必须经 SMMU stream 5。

**0108（sweet_gpu.c +68 行，只读）**：映射 kgsl-smmu 0x2ca0000/0x10000；读 IDR0/1/2/7、SCR0、SGFSR；由 IDR1 推导 page(4K/64K)/pages/contexts/matches 并与 DTS 大小对账；读 SMR/S2CR（≤16）、CBAR（第二页）、CB0 的 SCTLR/TTBR0/TCR/FSR；`stage4=OK/FAIL`；`gpusmmu=0` 跳过。

**依据**：sm8150 `msm-arm-smmu-sm8150.dtsi`（kgsl-smmu@0x02CA0000 0x10000，streams 0/1/2/5）+ arm-smmu.h 布局 + HackiOS step 8/9 的解析方式（IDR1 bit31=64K，[30:28] 页数指数，[7:0] context 数；CB0 偏移 = pages*page）。

**工程**：0108 对 0107 终态 `git apply --check` 通过。

---

## Task 41 (r40/t30) — SMMU 经 hypervisor：改用 SCM IO（2026-10-25）

**输入**：t29 SMMU 探针——IDR 布局正确（`idr0=0x4c017e06 idr1=0x20000005 matches=6 contexts=5 page=0x1000 pages=8`），但 `SMR=0`、`S2CR` 为垃圾值，直接读 GR1 CBAR(0x1000) 卡死 GPU 线程（系统其余继续）。

**根因**：厂商 `arm-smmu.c` 注释——部分 Qualcomm 平台的 SMMU 全局寄存器由 hypervisor 捕获/仿真，寄存器访问需经 **SCM IO**（`SCM_SVC_IO=0x5`，`SCM_IO_READ=1`/`SCM_IO_WRITE=2`，`scm_io_read/write`），静态 context bank 的路径亦如此；直接读仿真不全的 GR1 会挂。

**0109**：新增 `gpu_scm_io_read()`（SCM_FAST|SCM_64|SCM_SIP|SCM_CALL(5,1)，arginfo=1，arg=物理地址）；stage 4 保留直接 IDR/SMR/S2CR 读取，用 SCM IO 交叉验证 IDR0，并逐个读 CBAR 与 CB0 的 SCTLR/TTBR0/TCR/FSR；判决 `stage4=OK/PARTIAL scm_io=working/refused`；不再直接触碰 GR1。

**工程**：0109 对 0108 终态 `git apply --check` 通过（42 增 9 删）。

---

## Task 42 (r41/t31) — SMMU context bank 页直读（绕开 GR1）（2026-10-25）

**输入**：t29 直接读 GR1 CBAR(0x1000) 卡死线程；t30 SCM IO 读 IDR0 返回 status=-2（拒绝）。厂商 `kgsl_iommu.c`：`ctx->regbase = iommu->regbase + KGSL_IOMMU_CB0_OFFSET(0x8000) + (cb_num << CB_SHIFT)`，CB 寄存器用 `readl/writel` 直接访问。

**0110**：删除 GR1/CBAR 读取；保留一次 SCM IO 读 IDR0 记录状态；按 `0x8000 + n*0x1000` 读 CB0..CB4 的 SCTLR/TTBR0/TCR/FSR；判决 `cb_regs=responding/all-ones`。纯只读。

**工程**：0110 对 0109 终态 `git apply --check` 通过（21 增 26 删）。

---

## Task 43 (r42/t32) — SMMU context 1 盲写 + GMU 固件落位（2026-10-25）

**输入**：t29/t31 证明 SMMU 多数寄存器读出为垃圾（hypervisor 仿真不全），t30 SCM IO 被拒（-2）；厂商 kgsl 直接 MMIO 写 CBAR/SMR/S2CR/CB 寄存器。GMU 固件解析：TCM 14,892 B（ITCM）+ DDR 22,676 B（4 段：0x4000/0x40000/0x43fe0/0x57000）+ 运行期 log(0x60002000)/HFI(0x60006000)。

**0111**（新增 `bsd/dev/a640_gmu_fw.h` 37,680 B 固件字节数组 + `sweet_gpu.c` +258 行）：
- 4K 页表（kvtophys 物理地址、PTE shared/AF/nG/uncached），映射 4 个 DDR 段与 log/HFI；
- CM3 保持复位，TCM 段逐字经 GMU ITCM(0x3000)/DTCM(0x7000) 窗口写入并回读；
- DDR 段写入映射内存并 dc civac；
- 盲写 SMMU context 1：CBAR/CBA2R、SCTLR=0、TCR2/TCR/TTBR0/TTBR1/MAIR0/1/FSR、TLBIASID/TLBSYNC、SCTLR_ON、SMR[5]/S2CR[5]→CB1；
- 判决 `stage5=OK/PARTIAL`；**不释放 CM3**（stage 6 才启动）。

**工程**：0111 对 0110 终态 `git apply --check` 通过；固件头文件以 LF 生成（首次 CRLF 触发了 git whitespace 警告，已 sed 修正）。

**0111 编译修复（2026-10-25 11:02）**：CI run 40155835579 failure——`kvtophys` 未声明（BSD 层未导出 osfmk/vm/pmap.h 的该原型）。0111 增加 `extern uint64_t kvtophys(vm_offset_t);` 后重发（补丁对 0110 终态重新校验通过）。

---

## Task 44 (r43/t33) — 释放 CM3：GMU 固件启动（2026-10-25）

**输入**：t32 `stage5=OK fw=loaded tcm_wrong=0 ddr_blocks=4`（TCM 3723 字、DDR 22,676 B、CB1 盲写成功）。

**0112（sweet_gpu.c +128/-1）**：
- HFI 页从 4 KiB 改为 16 KiB（表 + 3 队列 @4K 间隔需 13 KiB，XNU PAGE_SIZE 恰好 16 KiB）；
- 填 HFI 表（vendor a6xx_hfi.c 布局）：words/first_queue=6/queue_words/queues=3、queue.start=0x60006000+848+4096n、type=prio<<8|n、status=1；
- GBIF 解 halt；按 HackiOS step 15 写 GMU 启动寄存器（CX_FAL/CX_FALNEXT/GENERAL_7/FW_INIT_RESULT/BOOT_CONFIG=2/HFI_QTBL/AHB_FENCE_RANGE_0/CM3_CFG=0x4052/PWR_COL_CP_*/HFI_SFR_ADDR=chipid 0x06040000/RPMH_CTRL=0xf11 等）；
- 释放 CM3_SYSRESET，轮询 3 s；判决 `stage6=OK/FAIL/PARTIAL`，附 fw_init/intr/hfi_ctrl/sptprac/rpmh 与 DTCM 版本。

**风险**：本轮固件开始执行；SMMU 映射若有误会触 fault，最坏整机挂起（重启即恢复）。

**工程**：0112 对 0111 终态 `git apply --check` 通过。

**0112 编译修复（2026-10-25 11:28）**：CI run 40157617046 failure——stage 6 的 `uint32_t result` 与线程参数 `wait_result_t result` 重名。改名 `fw_result` 后重发（补丁对 0111 终态重新校验通过）。

---

## Task 45 (r44/t34) — CM3 启动诊断（2026-10-25）

**输入**：t33 `fw_init=0 intr=0 hfi_ctrl=0 sptprac=0xc0 rpmh=0xf`，3 秒超时。

**0113（+8/-1）**：轮询延长到 20 秒；释放后回读 `CM3_SYSRESET/BOOT_CONFIG/CM3_CFG/AO_HOST_INTR(0x24c14)/AHB_FENCE(0x24c4c)`；新增 after-release 诊断行。目的：区分"19.2 MHz 下启动慢"与"CM3 没执行"。

**工程**：0113 对 0112 终态 `git apply --check` 通过。

---

## Task 46 (r45/t35) — GMU 时钟 200 MHz（2026-10-25）

**输入**：t34 debug `sysreset=0 boot_config=0x2 cm3_cfg=0x4052` 但 `fw_init=0`（20 s）→ CM3 无时钟。

**0114（+31）**：释放 CM3 前做厂商 RCG 序列——GCC 0x52004 bit16（gpll0_div gate）→ GPUCC CMD RCGR ROOT_EN、轮询 ROOT_OFF 清 → CFG=0x602（GPLL0_OUT_MAIN_DIV /1.5=200 MHz）→ UPDATE → 清 ROOT_EN；回读 before/after cmd/cfg；`gpuclk=0` 可跳过。

**依据**：gpucc-sm8150.c 的 gmu_clk_src freq table（200 MHz = GPLL0_DIV 1.5）+ clk-rcg2.c 的 force-enable/configure 语义（CMD_ROOT_EN bit1、ROOT_OFF bit31、CFG src [10:8]、div [4:0]、mode [13:12]）。

**工程**：0114 对 0113 终态 `git apply --check` 通过。

---

## Task 47 (r46/t36) — a640 GMU 寄存器表纠正（2026-10-25）

**根因（决定性）**：此前 GMU 寄存器偏移来自 HackiOS（a660），a640 完全不同。厂商 `a6xx_reg.h`（字索引 ×4）：ITCM_START 0x1B400→0x6d000、DTCM_START 0x1C400→0x71000、SYSRESET 0x1F800→0x7e000、BOOT_CONFIG 0x1F801→0x7e004、FW_INIT_RESULT 0x1F81C→0x7e070、CM3_CFG 0x1F82D→0x7e0b4、HFI_CTRL_STATUS 0x1F980→0x7e600、QTBL_INFO/ADDR 0x7e610/0x7e614、SFR 0x7e608、RPMH_CTRL 0x7e3a0、AO_COUNTER 0x7e220、AHB_FENCE_RANGE_0 0x8ec44。GMU VMA：ITCM 0x0(16K)、ICACHE 0x4000(240K)、DTCM 0x40000(16K)、DCACHE 0x44000(240K)、NONCACHED 0x60000000。厂商启动等待 `FW_INIT_RESULT==0xBABEFACE`，load_mode=TCM_BOOT=2，HFI_SFR_ADDR=压缩后的 chipid（a640→0x06040000）。

**0115（+41/-49）**：全部 GMU 常量换 a640；TCM 窗口/范围修正（DTCM 0x40000..0x43fff）；固件块分类按 gmu_vma（ICACHE 0x4000 与 DCACHE 0x57000 经 SMMU，DTCM 0x40000/0x43fe0 走窗口）；启动序列按厂商 a6xx_gmu_configure（含 WFI_CONFIG=0）；轮询 0xBABEFACE。

**教训**：跨 SoC 移植时 GMU/TCM 映射不能照抄；必须从目标 SoC 的 a6xx_reg.h/gmu_vma 取偏移。此前 t33~t35 的"CM3 不启动"全部由此解释。

**工程**：0115 对 0114 终态 `git apply --check` 通过。

---

## Task 48 (r47/t37) — GMU 时钟提前到 GMU 读取之前（2026-10-25）

**输入**：t36（a640 正确寄存器表）回归——30 秒唤醒后整机挂死、flush 停在 ~30 s。t35 对照（错误偏移）正常打印 stage 0，证明环境/线程调度无问题。

**根因**：stage 2 在 200 MHz RCG 仍 root-off 时读取正确的 GMU 寄存器（0x7e0xx/0x7e2xx），GMU 寄存器访问挂死 NoC；t35 读错误偏移所以无害。

**0116（+26）**：把 GCC 0x52004 bit16 + GPUCC RCG（ROOT_EN/CFG=0x602/UPDATE/清 force）从 stage 6 提前到 stage 2，在使能 cx_gmu 与任何 GMU 读取之前；stage 6 保留幂等重复；新增 `stage2: gmu clock` 日志。

**工程**：0116 对 0115 终态 `git apply --check` 通过。

---

## Task 49 (r48/t38) — 跳过 stage-2 GMU 读取 + stage-6 日志窗口（2026-10-25）

**输入**：t37 `gpugmu=0` 隔离实验——stage 0/1 完整打印、系统不挂（flush #63），锁定挂点在 stage 2 的 GMU 寄存器读取（0x7e0xx），t36/t37 均挂于此；t35 因读错偏移（a660）而幸免。

**0117（+8/-1）**：stage 2 的 GMU 读取默认关闭（`gpuread=1` 才开）；stage 6 写 GMU 启动寄存器前加 11 秒日志窗口与 `GMU config registers written` 标记；其余不变。

**目的**：让线程穿过 stage 2 进入 stage 5/6，并用落盘日志定位 GMU 写/启动是否可执行。

**工程**：0117 对 0116 终态 `git apply --check` 通过。

---

## Task 50 (r49/t39) — t38 挂死实为 data abort panic：GMU CSR 偏移少了一次 rebase（2026-10-25）

**输入**：t38（0117，stage-2 GMU 读闸门 + stage-6 11 s 窗口）重放。ring 在 30 秒唤醒处停在 flush #3（123,041 B，末行 #1862），健康对照 `gpugmu=0` 为 flush #63；屏幕在 30 秒后冻结。

**决定性证据（屏幕）**：`panic(cpu 4 caller ...): Kernel data abort. at pc 0xfffffe00074fc6ec, lr 0xfffffe00074fc6d4`。用 CI artifact 的 `kernel.development.raphael` 符号表 + 从 0103–0117 patch 重建的 `sweet_gpu.o` DWARF 行表解析：pc = `gpu_thread+0x1268` → `sweet_gpu.c:835`，lr → `sweet_gpu.c:831`；835 行是 **stage 5 第一条 GMU 写** `gpu_wr(gmu5, GMU_CM3_SYSRESET, 1)`。

**根因**：0115 的“a640 寄存器表”使用 **GPU 基址（0x2c00000）相对** 的 a6xx_reg.h 偏移（SYSRESET 0x7e000），但 `gmu5` 是 `ml_io_map(0x2c6a000, 0x25000)` 的 GMU 映射：`0x2c6a000 + 0x7e000 = 0x2ce4000` 在映射外 → 翻译错误 → data abort panic。旧 a660 值 0x14000 与 0x7e000 指向同一物理寄存器（0x2c7e000）；t27/t28 的 `stage2: gmu sysreset=0x1 ... counter=21197600 Hz` 证明旧偏移一直读到的是真实 GMU CSR。0115 真正有效的只有固件布局：DTCM VA 0x40000、ICACHE/DCACHE 经 SMMU、`ddr_blocks=2`、`0xBABEFACE` 判定。

**0118（+29/-25）**：新增 `GMU_CSR(x)=x-0x6a000`，全部 GMU CSR/TCM 窗口偏移 rebase 到 GMU 基址；保留固件布局修正；stage 2 GMU 读取前与 stage 5 首写前各加 3 s 落盘窗口。打包 cmdline 追加 `gpuread=1 ufslogsecs=1`。

**教训**：SoC 寄存器头文件的偏移基准必须写清楚（GPU 基址 vs GMU 基址）；跨基址复用常量时先做地址等式验算，再用旧日志里的真实读数交叉验证。panic 的 pc/lr 配合内核符号表 + `.a` 里的 DWARF 行表可以在 Windows 上完成精确到行的定位（`llvm-symbolizer` 来自 Android NDK）。

**工程**：0118 对 0117 终态 `patch -p1 --dry-run` 通过。

---

## Task 51 (r50/t40) — t39 打通 GMU 寄存器；CM3 不执行 → SMMU S2CR 是 BYPASS（2026-10-25）

**t39（0118）实机结果**：ring total 225,565 B / flush #133，系统全程存活（首次 30 秒唤醒不 panic）。stage 2 GMU 读恢复：`sysreset=0x1 boot_config=0 fw_init=0`、`rpmh=0xf sptprac=0xc0`、`counter=19200000 Hz`；stage 4 SMMU OK；stage 5 `tcm=5464 words wrong=0 ddr_blocks=2`、`stage5=OK`；stage 6 配置写全部接受（`GMU config registers written`，释放后 `sysreset=0 boot_config=0x2 cm3_cfg=0x4052`，版本 `core=0x20000105`），但 **`fw_init=0`，CM3 不执行**。→ 0118 的 −0x6a000 rebase 完全正确，问题转入 SMMU/启动条件。

**根因（0119）**：固件里写 `0xBABEFACE` 的代码位于 **ICACHE 区 VMA 0x4000**（gmu_vma），该区是经 GMU SMMU 映射的宿主内存；stage 5 的 `SMMU_S2CR(5)` 写的是 `1u`，按 ARM SMMU-500 语义 = **TYPE=1 (BYPASS), CBNDX=0**，不是注释声称的“stream 5 → context 1”。CM3 因而从物理 0x4000 取指、HFI/log 也绕过翻译 → 永远写不出 FW_INIT。

**0119（+30/-6）**：S2CR(5)=1<<8（TYPE=0 翻译，CBNDX=1）；补回 vendor 的 `GMU_PWR_COL_CP_RESP=0`（0115 误删）；AHB fence 改为 a640 上游值 `BIT31|0xa<<18|0xa0`；启动前清零 GMU 日志页，失败后 dump 前 16 word + log 写指针 + SMMU CB FSR。

**依据**：ARM SMMU-500 S2CR TYPE[1:0]/CBNDX[15:8]；上游 `a6xx_gmu.c` 的 fw_load/start 序列；HackiOS `redwood_adreno.c` step_gmu_start 对照；raphael DTB（gmu_kernel iommus = SID 5 mask 0x400，GMU reg size 0x30000）。

**工程**：0119 对 0118 终态 `patch -p1 --dry-run` 通过。

---

## Task 52 (r51/t41) — S2CR 已生效但 TLB 锁死：QSMMU safe-toggle + RSC 唤醒（2026-10-25）

**t40（0119）实机结果**：ring 860,633 B / flush #9027，系统健康。`smmu cb1@0x9000 ... smr5/s2cr5=0x100 sent`；stage 5=OK；stage 6 配置写入、释放 CM3，但 `gmu log wptr=0`、日志页全 0、**`smmu_fsr=0x400`（TLBLKF）**、`fw_init=0`。

**判读**：S2CR 修复已让 stream 5 走 context 1 翻译（不再是 bypass），CM3 已开始取指；失败点是 GMU SMMU 的页表遍历被 **TLB lock** 挡住。DTB 证实 `/soc/kgsl-smmu@0x02ca0000` = `qcom,qsmmu-v500` + `qcom,skip-init` + `qcom,use-3-lvl-tables`，GMU 的 iommus phandle 0x249 正指向它。上游 `arm-smmu-qcom.c` 对 QSMMU 在 reset 路径调用 `qcom_scm_qsmmu500_wait_safe_toggle(0)`（SVC 0x15 / CMD 0x03 / arg0=CLIENT_ALL=2 / arg1=0）关闭 wait-for-safe；否则 TLB 保持锁住，出现 TLBLKF。另：stage2 `rpmh_state=0xf` 表示 GMU 仍在 RPMh slumber，vendor `a6xx_rpmh_start` 会在启动 CM3 前唤醒 RSC。

**0120（+40/-1）**：stage 5 在 SMMU 编程前调用 SCM safe-toggle；只读探测 TCU（0x2cc2000 八个寄存器）；stage 6 读取 GPU RSC 状态（GMU+0x23000 区）与 `GMU_RSCC_CONTROL_ACK`，若未 awake 则写 `RSCC_CONTROL_REQ=2`、轮询 ACK bit1、再清零；其余不变。

**工程**：0120 对 0119 终态 `patch -p1 --dry-run` 通过。

---

## Task 53 (r52/t42) — S2CR 位域更正：context 在 [7:0]，GMU 流表项是 1（2026-10-26）

**背景**：t40（0119）把 S2CR 从 `1u` 改成 `1u<<8`，理由是“1u 是 bypass”。复核 ARM `arm-smmu.h` 后确认这是错的：`S2CR.TYPE = GENMASK(17,16)`、`S2CR.CBNDX = GENMASK(7,0)`。HackiOS `hakios_adreno.c` 的 `space_on()` 直接写 `S2CR(match)=space->context`（GMU context=1）并以 `(s2cr & 0x300ff)==context` 校验。→ t39 的 `1u` 本来就是“context 1 + 翻译”；t40 的 `1u<<8` 把 GMU 指到了 context 0，CB1 FSR=0x400 不能作为 GMU 故障证据。

**另一个关键差异**：HackiOS 的 `GMU_MATCH = 1`（流表项 1，entry 0 留给 3D 核），`GMU_STREAM = 5`、mask 0x400；我们的代码一直写 entry 5。结合 `qcom,skip-init`（hypervisor 托管流表），entry 索引很可能就是 t39 仍然失败的原因。

**0121（+26/-10）**：S2CR 改回 `1`；entry 1 与 entry 5 都写 SMR/S2CR；TLBSYNC 后轮询 TLBSTATUS；打印 entry1/5 回读；保留 QSMMU safe-toggle、RSC 唤醒、TCU 探测、GMU 日志/FSR 转储。

**工程**：0121 对 0120 终态 `patch -p1 --dry-run` 通过。0120 已编译但未实机，直接由 0121 取代。

---

## Task 54 (r53/t43) — t42 证实 S2CR 正确，但 RSC 唤醒把系统带死；0122 默认关闭（2026-10-26）

**t42（0121）实机**：`qsmmu safe-toggle status=0`（SCM 可用）；`smr1=0x84000005 s2cr1=0x1 smr5=0x84000005 s2cr5=0x1`，回读与写入完全一致，`tlbstatus=0`——**S2CR 位域与流表项修复确认生效**，hypervisor 接受 entry 1/5 的写入，GMU stream 5 走 context 1。但 ring 只有 93,442 B / flush #20，最后一行是 `stage6: GMU registers in 11 s`：系统在 11 秒窗口内死亡，配置写与 CM3 释放都没执行。窗口前唯一新动作是 **RSCC_CONTROL_REQ=2 唤醒（ack=0）**；vendor 是在 `a6xx_gmu_rpmh_init` 配好 PDC 序列后才唤醒 RSC，我们跳过了 PDC。

**0122（+55/-31）**：`gpursc` 默认 0（跳过 RSC 唤醒，需要时 repack 打开）；`gpusafe` 可关；stage 6 配置写分三组，每组后 1.5 s 落盘窗口；其余保留。

**教训**：不要在没有 PDC 序列的情况下盲写 RSCC_CONTROL_REQ；QSMMU/RSCC 的任何唤醒动作都要先有 vendor 的 PDC 配置。

**工程**：0122 对 0121 终态 `patch -p1 --dry-run` 通过。
