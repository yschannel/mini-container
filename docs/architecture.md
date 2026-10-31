# V1 架构与责任

采用 V1_ACCELERATED_WORKFLOW.md 的并行流程。A 负责 Runtime/隔离/Rootfs，B 独立负责 cgroup/资源测试；公共头签名沿用 contracts.md。

CLI → Runtime → v0 或 isolate → 可信非交互负载。full 尚未接通，失败不降级。

- v0：fork/exec 宿主程序，保留原有错误分类和回收。
- isolate：clone 独立 1 MiB 回调栈，PID/UTS/Mount 三个 namespace，无共享 VM/文件表。父进程保持栈、参数、控制端和错误读端到子进程回收。
- 子进程先关无关 fd，等控制通道明确 G 字节；父端消失、EOF 或错误则不执行负载。控制通道是 socketpair 的单向使用，MSG_NOSIGNAL 避免改变全局 SIGPIPE 处置。
- 放行后设置 /dev/null stdin，挂载传播 private、hostname、专用 Rootfs/chroot/chdir /、新 proc，再 exec。stdout/stderr 继承终端。
- CLOEXEC 错误管道报告准备/exec 步骤及 errno。EOF 结合准备记录和 wait 状态分类，不独立当成功。enabled 记录实际启用的 namespace/Rootfs，cgroup 和可选限额为空。
- 父进程通过 signalfd 在正常控制流处理中断，必要时 SIGKILL 自己的 namespace init，再 wait。namespace PID 1 的后代生命周期和 namespace 内挂载由内核终止/释放，Rootfs 文件资产保留。

只运行可信、有限、非守护化负载，允许负载直接作 namespace PID 1，不做 init/TTY/网络/Seccomp/pivot_root，不声明 chroot 防逃逸或运行器 SIGKILL 后自动恢复。

Rootfs 准备脚本只写项目 .local 中专用新目录，验证静态 BusyBox 架构/链接，不删除既有资产。隔离执行需实际 namespace/mount 权限；Agent EPERM 与普通终端 sudo 结果分别记录。

当前验证与限制见 docs/evidence/A/isolation/report.md。资源模块未接通，后续 full 才加入先创建/设置/attach cgroup 再放行的编排，不提前引用 B 的未完成实现。
