# Mini-Container 双人开发与汇报方案

本方案面向两名组员、Ubuntu 与 openSUSE 两套 Linux 环境，以及下周的课程汇报。以 2026 年 10 月 28 日为起点，下周为 11 月 2 日至 8 日；具体汇报日尚未确定，因此采用相对日程，最晚在汇报前两天停止添加功能。

## 1. 项目定位与范围

实现一个基于 Linux 内核隔离与资源控制机制的教学型容器运行时。核心目标是解释并验证：一个普通进程如何获得独立的主机名、进程视图、根文件系统与资源额度；程序如何启动它、获知失败并回收资源。

沿用原设计的 C++17 与 CMake，使用 Linux C 接口进行系统编程。C++ 主要用于 RAII、数据结构与模块组织。课程允许高级语言，因而暂不同时维护纯 C 版本。报告中明确所用语言。

下周汇报建议目标：V1 稳定通过，争取 V2 的前台 CLI 与 daemon。V3 网络和 Seccomp 为后续升级内容。若下周只是阶段汇报，以实际实现和调试记录为主体，剩余功能标为计划；若是最终验收，应优先完成 V1 与证据整理，再决定是否扩展。

不在本阶段实现：OCI/Docker 兼容、镜像仓库、镜像分层、交互式终端代理、SSH、rootless、生产级安全隔离、daemon 崩溃后的容器接管。仅运行小组提供的可信测试程序；明确这是教学原型。

## 2. 分版本成果与验收

### V0 可重复构建与进程执行基础

功能：CMake 构建、参数解析、错误日志、RAII 文件描述符封装；启动指定程序、等待退出并返回退出码。

验收：两台 Linux 从同一提交构建成功；执行成功、程序不存在、非零退出三种情况均有准确结果；保存环境信息、构建日志和一次 GDB 调试记录。

定位：进度保底版本，尚不能作为容器核心功能完成的证明。

### V1 最小可验收容器

功能：前台命令 `minic run --rootfs PATH --hostname NAME [--memory BYTES] [--cpu-quota QUOTA] -- PROGRAM ARGS...`；PID、UTS、Mount namespace；独立 BusyBox Rootfs；容器内挂载 proc；cgroups v2 CPU/内存限制；正常和失败路径的清理。

实现边界：第一版执行非交互命令，stdin 不代理，不承诺直接进入交互 shell；stdout/stderr 继承终端。使用专用 Rootfs，不提供宿主机目录的任意挂载。chroot 可作为首版根目录切换机制，但不将其描述为完整安全边界；pivot_root 留作升级。网络、User、IPC namespace 不计入本版已完成功能。

验收清单：

1. 容器设置不同 hostname，宿主 hostname 不变；并列展示宿主 PID 与容器 PID。
2. 容器内执行 `ps`，仅见该 PID namespace 的进程；不要求永远只有一个进程，因为测试脚本、ps 本身也会产生进程。
3. 容器内 `ls /` 显示测试 Rootfs，宿主预设标记文件不出现在其中；不以此宣称完整防逃逸。
4. 运行小组编写的内存测试程序：逐页写入分配内存，超过明确设定的额度；保留 memory.events 中 OOM/oom_kill 的前后变化和程序退出状态。记录 memory.swap.max 的设置，避免 swap 使演示结果不稳定。
5. 运行固定 CPU 测试，对比不限额与限额时的 cpu.stat 使用量增量/节流信息；不要只凭某一时刻 top 数字判断成功。
6. 指定无效 Rootfs、不存在的程序，以及让资源配置失败；均输出出错步骤、系统错误与非零退出码，不运行未满足限制的负载。
7. 连续运行三次，包括一次失败；检查没有残留的容器进程、项目挂载及本次创建的 cgroup。失败清理也必须可观察。

V1 通过条件：主演示机全部通过，另一台系统至少完成构建、隔离演示与差异记录。第二台尚未通过的项目如实标记，不写成双系统全部支持。

### V2 多容器管理与通信

功能：`minicd` 前台运行，CLI 通过 Unix Domain Socket 发送请求；支持 `run / ps / stop / logs`；保存内存中的容器状态；工作负载 stdout/stderr 写入每容器日志文件。

实现边界：不做 daemon 自守护化、复杂并发、日志跟随、TTY、重启恢复。daemon 可采用单线程事件循环或短请求处理模式，但运行容器不能阻塞其他管理请求。

验收：同时运行两个非交互长任务；CLI 返回容器 ID；ps 区分运行与退出状态；停止其中一个，另一个继续运行；logs 能读取指定容器输出；错误请求不会使 daemon 退出；SIGTERM daemon 时清理其管理的容器，并说明 SIGKILL/崩溃恢复不在支持范围。

V2 前必须补足 PID 1 策略：推荐小型 init 包装进程负责转发信号、waitpid 回收后代；退出码按文档约定报告。stop 按约定先发送终止信号，超时后强制结束，再完成 wait 与清理。未经该验证，不宣称支持任意多进程工作负载。

### V3 扩展项 每项独立验收

V3a 网络：先完成独立 Network namespace、loopback、veth 与固定地址，验收宿主与容器互通；再决定是否加入网桥与 NAT。外网访问还依赖转发、路由、防火墙与 DNS，不作为首轮承诺。不清空系统防火墙；只创建与删除项目拥有的规则和设备。

V3b Seccomp：过滤一个确定会被测试程序直接调用的 syscall，例如 getppid；同一程序在过滤前后展示返回值/errno 的变化，确保执行程序所需 syscall 仍允许。不要仅以 reboot 命令失败证明 Seccomp 生效。

V3c 完善项：pivot_root、更多异常恢复、性能测量、额外 namespace。根据剩余时间只选一项。

## 3. 实现架构与接口

V1：CLI → Runtime → namespace / Rootfs / cgroup → 工作负载。

V2：CLI → UDS 协议 → daemon 管理器 → 同一 Runtime → 工作负载。CLI 不直接执行核心挂载与 cgroup 操作。

建议目录：

```text
CMakeLists.txt
src/main_cli.cpp
src/main_daemon.cpp          # V2 再加入
include/minic/config.hpp    # RunConfig / RunResult / ContainerState
include/minic/runtime.hpp
src/core/runtime.cpp        # 启动编排、同步、等待与回滚
src/core/namespaces.cpp
src/core/rootfs.cpp
src/core/cgroup.cpp
src/common/fd.hpp
src/common/error.hpp
src/ipc/                    # V2 协议与 UDS
src/manager/                # V2 状态、停止、日志
tests/unit/
tests/integration/
tests/workloads/            # 内存、CPU、信号等小型测试程序
scripts/check-env.sh
scripts/demo.sh
docs/architecture.md
docs/debugging.md
docs/compatibility.md
docs/agent-log.md
```

最先约定 RunConfig：rootfs、hostname、argv、可选内存字节数、CPU quota/period。明确 CPU 的 quota/period 是配额时间参数，并在 CLI 帮助中写清单位。RunResult 至少区分启动错误、正常退出、信号终止；状态记录宿主 PID、退出信息与拥有的资源。各参数和状态必须有单一负责人维护。

V1 核心时序：

1. 检查参数、Rootfs、权限和所需内核接口，准备 RAII 资源与父子通信管道。
2. 创建带 PID/UTS/Mount namespace 的子进程。首版采用 clone 回调和独立栈，避免边写边混合多套启动方式；栈保持到子进程退出。
3. 子进程先阻塞在同步点；父进程配置专用 cgroup 并将子 PID 加入；失败则终止子进程并回滚，不释放工作负载执行。
4. 父进程放行；子进程将挂载传播设为 private，配置 hostname、切换 Rootfs 与工作目录、挂载 proc，最后 exec 工作负载。
5. 使用独立错误管道报告准备/exec 错误；错误管道写端带 close-on-exec，成功 exec 与提前失败必须可区分。
6. 父进程等待、保存结果、完成清理。清理只针对本次拥有的资源，顺序与创建依赖相反。

这条时序是双方都必须能画出来、解释出来的架构主线。PID 1 初始化包装与额外子进程在 V2 接入时同步更新时序图。

## 4. 两人分工与 Agent 使用

以下将 Ubuntu 组员称为 A，openSUSE 组员称为 B；可按熟悉程度交换，不能将一方固定为只写文档。

| 工作 | A 主责 | B 主责 | 共同事项 |
|---|---|---|---|
| V0/V1 | Runtime、namespace、Rootfs、启动同步 | CMake、参数与错误封装、cgroup、测试负载 | 首日冻结接口；逐日联调 |
| V2 | Runtime 接入管理器、PID 1 与信号回收 | UDS、协议、CLI、状态和日志 | 一起审查 stop 与退出路径 |
| 工具证据 | GDB、strace、启动流程说明 | ASan/Valgrind、CTest、资源测量 | 交换复现对方的一次调试 |
| 兼容验证 | Ubuntu 主演示 | openSUSE 交叉验证 | 同一提交、同一用例，记录差异 |
| 汇报 | 隔离与生命周期 | 资源限制与工程工具 | 双方讲得清完整架构 |

Agent 任务按模块拆分，每次任务明确输入、接口、允许修改的文件、验收方法和不做事项。一个模块一次只由一个人/Agent 修改，共享接口改动先沟通。每个任务应在短时间内可审查，避免让 Agent 一次生成整个项目。

推荐任务模板：

> 当前版本 V1。请仅实现指定模块，遵循已冻结的 RunConfig/Runtime 接口，检查每个系统调用，使用 RAII，不改变公共接口，不新增未约定依赖。列出创建/回收的资源、关键 errno 和验证步骤。完成后说明哪些步骤未在真实 Linux 环境验证。

Agent 适合承担：样板代码、CMake、参数解析、协议编解码、错误路径检查、小型测试程序、测试脚本和文档初稿。

人必须亲自完成：实际 Linux 运行；理解 clone 与父子同步；检查 mount/cgroup 路径；GDB 跟踪；读懂一次错误与修复；确认 cleanup；审核 Agent 生成的系统配置修改。代码能编译不代表机制已验证。

每人至少留下三类证据：一段自己能逐行解释的核心代码；一次亲手设置断点或观察调用的记录；一个“现象—假设—证据—修复—复测”的问题闭环。无须故意引入 bug，也不要编造调试经历。

## 5. Ubuntu 与 openSUSE 环境策略

首日运行只读检查并保存输出：`cat /etc/os-release`、`uname -r`、`uname -m`、编译器/CMake/GDB 版本、`findmnt -t cgroup2`、`cat /proc/self/cgroup`、`cat /sys/fs/cgroup/cgroup.controllers`。确认实际为 cgroup v2，并有 cpu/memory 控制器；不能仅凭发行版名称假定可用。

选择先通过预检的一台作为主演示机，默认优先 Ubuntu；另一台承担交叉验证。记录发行版具体版本、内核、CPU 架构、是否虚拟机与实际权限。若为 WSL/嵌套容器，额外确认运行条件，必要时使用已有完整 Linux 虚拟机。

两端分别原生构建同一 Git 提交，不复制可执行文件。固定依赖范围：编译器、CMake、构建工具、GDB、strace 与 BusyBox；ASan 用编译选项启用。GTest、JSON 库与 libseccomp 按版本引入，不让首版依赖临时联网下载。

先验证 BusyBox 是否静态链接。动态 BusyBox 必须配套加载器与库；不假定两台系统的软件包产物一致。两机架构相同时可共享已确认的静态 Rootfs 资产，否则分别准备。测试负载同样检查链接方式，避免“文件存在却无法 exec”的问题。

cgroup 采用明确的 systemd 委派子树，记录 Delegate 设置和控制器启用方式；不要随意修改系统管理的根层级。管理器留在管理分支，工作负载进入叶子 cgroup，遵守 no-internal-process 规则。委派环境准备可以由 systemd 完成，但 memory.max、cpu.max 和 cgroup.procs 的操作由自己的代码实现。

首版在测试环境使用管理员权限，不将无权限问题静默跳过。遇到发行版安全策略拒绝时，保存错误和诊断信息，优先针对项目配置，不用关闭整套安全机制作为默认解决办法。

跨系统差异记录表：环境版本、构建结果、隔离结果、CPU/内存结果、失败原因、修复方式、未解决限制。确实不支持的环境应给出明确预检错误。

## 6. 工具使用与验收证据

| 工具 | 实际任务 | 最小证据 |
|---|---|---|
| Git | 小提交、模块分支、交叉审查、稳定版本标签 | 可对应功能/修复的提交记录 |
| CMake + CTest | Debug/ASan 构建与可重复测试 | 两机同一提交的构建及测试摘要 |
| GDB | 在子进程入口、同步点、exec 前断点观察 | 断点、变量/调用栈、解释 |
| strace | 跟踪 clone、mount、execve、wait 等实际调用 | 关键调用片段及其与架构的对应 |
| ASan 或 Valgrind | 检查协议/配置/资源封装，V2 检查 daemon 重复请求 | 报告、覆盖范围、修复或无错误结果 |
| cgroup 文件 + 计时工具 | 验证额度和效果 | memory.events、cpu.stat 前后对比 |
| Perf 可选 | 测量启动或运行热点 | 环境、样本、方法与有限结论 |

GDB 的 follow-fork-mode 用于 fork/vfork 路径；clone 回调路径优先直接在 child_entry 断点，按实际 GDB 行为检查 inferiors，不保证一个命令自动跟住所有子进程。

ASan 不验证 namespace 隔离和 cgroup 正确性；不把 ASan 内存开销混入 50 MB 限额演示。Valgrind 与 ASan 分开运行。Perf 受内核权限影响，时间不足不强行制作 FlameGraph。

测试分层：参数/协议/状态转换做单元测试；隔离、资源限制、信号和清理做真实 Linux 集成测试。特权测试显式运行，无法运行时标记跳过原因，不能算通过。

## 7. 日程与停止扩展条件

默认两人每天各投入约 3–4 小时；这是一项排期假设，不是对全部功能的完成保证。若实际投入更少，直接保留 V1，压缩 V2/V3。

| 时间 | A | B | 当天必须产出 |
|---|---|---|---|
| D0 今天 | 环境预检、画启动时序 | 环境预检、建构建骨架 | 同一提交两机可构建、接口约定 |
| D1 | PID/UTS namespace 与执行 | cgroup 探针、CPU/内存测试程序 | V0 通过，部分隔离可观察 |
| D2 | Mount、Rootfs、proc、错误同步 | cgroup 模块与 Runtime 联调 | 第一条完整 V1 演示 |
| D3 | 失败回滚、等待与清理 | 双系统验证、集成脚本、资源证据 | V1 稳定标签、问题清单 |
| D4 有余量时 | PID 1/信号与 Runtime 接口 | UDS、最小 daemon/CLI | 决定是否保留 V2；否则补测试 |
| 汇报前两天起 | 修复、整理架构、复现同伴调试 | 修复、整理工具证据、复现同伴调试 | 冻结功能、完成材料 |
| 汇报前一天 | 主机完整彩排 | 备用机复现、准备录屏 | 无临时安装依赖的演示 |

D2 结束仍无法稳定运行隔离程序：暂停 daemon、网络、Seccomp，集中修复 V1。

D3 结束 V1 尚不稳定：不开始 V2。若只剩一两天，保留已经验证的功能，明确未完成项。

V1 通过后只选择一个升级方向。即使 V2 进展顺利，汇报前两天也停止新增功能。演示使用已验证的标签，不在现场运行未经复测的最新修改。

## 8. 汇报组织与最低交付物

按约 8–10 分钟准备，可依课程规定缩放：项目目标与完成范围 1 分钟；架构和启动时序 2 分钟；隔离、资源和清理演示 3 分钟；两人的工具调试证据 2 分钟；双系统差异与后续计划 1 分钟。

双方必须能回答：namespace 和 cgroup 分别解决什么；容器 PID 与宿主 PID 为什么不同；为什么要在 namespace 内挂载 proc；为什么负载必须等 cgroup 就绪后才能执行；exec 失败如何通知父进程；PID 1 有什么特殊性；失败后有哪些资源要回收；哪一次工具观察真正改变了代码。

最低交付物：

- 可从源码构建的 V1 稳定提交/标签，以及准确标明已实现内容的 README。
- 环境与依赖清单、Rootfs 准备说明、一条可重复演示入口。
- 架构图与父子进程启动时序图。
- Ubuntu/openSUSE 兼容性记录，区分通过、失败与未测。
- 两人的真实调试记录，以及资源限制和失败清理证据。
- Agent 使用记录：任务、生成内容、人工审查发现、实际验证结果。
- 汇报材料及一次完整演示录像，录像作为现场故障备用。

每次完成一版，都同步更新“已完成/已验证/已知限制”；报告与代码保持一致。

## 9. 技术依据

- Linux namespaces 手册：https://man7.org/linux/man-pages/man7/namespaces.7.html
- Mount namespace 与传播：https://man7.org/linux/man-pages/man7/mount_namespaces.7.html
- PID namespace 与 PID 1：https://man7.org/linux/man-pages/man7/pid_namespaces.7.html
- cgroup v2 控制器、事件和进程布局：https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html
- systemd cgroup 委派：https://github.com/systemd/systemd/blob/main/docs/CGROUP_DELEGATION.md
- GDB 子进程调试：https://sourceware.org/gdb/current/onlinedocs/gdb.html/Forks.html
