新增明确标记为非完整 V1 的 isolate 模式：独立 clone 栈及 PID/UTS/Mount namespace，子进程等父端 G 字节放行后设置 private 传播、hostname、chroot、私有 /dev/null 和 proc，再 exec。沿用现有错误/结果接口，父进程 wait 回收，支持准备/exec 失败和中断后的 namespace 后代清理。附专用静态 BusyBox Rootfs 脚本。

采用 V1_ACCELERATED_WORKFLOW.md；基线 C02=3d467120786dce63b841820c60dd7e9c4e99eb4e。未改公共签名或 B 的 cgroup 实现，full 仍拒绝。

验证：标准 Debug 构建与受影响 V0 CTest 通过；Ubuntu/VMware 在实现 66664782a71de911c9e5cd92e561e4c46e8ad1cb、干净工作区执行 8 项隔离检查通过，包含 namespace/PID/hostname/Rootfs/proc、三轮含失败及两种中断后代回收。其中 mount EPERM 项是 strace 故障注入，明确区分。A 已亲手 strace 观察完整放行/隔离/exec/wait 顺序。详见 docs/evidence/A/isolation/report.md、native-verification.md、strace-record.md。

首两轮发现最小 Rootfs 缺 /dev/null，后台任务未启动；保留原失败证据，修复为子 namespace 临时 tmpfs 中的 null 字符设备后复验通过。只支持可信有限非守护化负载，不宣称 full/cgroup 或双系统 V1 通过。请 B 审查，不自动合并。
