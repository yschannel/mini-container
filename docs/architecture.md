# 架构与责任（计划，尚未实现）

S00 仅构建 minic 的帮助与版本；核心方法只有声明。C++17，无第三方下载依赖。
目标链路：CLI → Runtime → namespace / Rootfs / cgroup → 可信非交互工作负载。
Runtime 由 A 实现，cgroup 由 B 实现，公共头由 A 维护、B 审查。

1. 父进程校验配置，创建唯一 run_id、RAII 管道与独立 clone 栈。
2. v0 后续用 fork；isolate/full 用 clone 回调，不用 CLONE_VM/FILES/THREAD。子进程阻塞于控制管道。
3. full 父进程 create/设置/读回 cgroup，attach 宿主 child PID；任一步失败均不放行，终止并 wait。
4. 明确控制字节放行后，子进程设置 private mount propagation、hostname、chroot、chdir /、挂载新 PID namespace 的 proc，exec。
5. 独立错误管道写端 CLOEXEC，记录步骤和 errno；处理 EINTR、短读写与意外 EOF。EOF 结合 wait 状态判定；早死而无错误记录不能当 exec 成功。控制管道 EOF 时禁止执行负载。
6. 父进程 wait，保存结果，在删除前 snapshot，再显式 cleanup，写 result.json。失败逆序回滚且保留所有错误。

父进程拥有 clone 栈直到 wait 完成、自己的管道端、cgroup 会话、run_id 与结果；子进程拥有自己的 fd、cwd 和 namespace 内挂载。
exec 前关闭无关 fd，禁止残留宿主目录 fd；仅删除本次创建且确认归属的资源。
正常退出、准备/exec/限额失败、控制端断开、中断均需回收。信号处理器只通知正常控制流，后者终止并 wait。
V1 直接让有限负载作为 PID 1，不承诺任意守护进程；必要时对自己的 namespace init 用 SIGKILL 有界兜底，并验证后代清除。
运行器自身 SIGKILL、掉电后恢复不在范围。chroot 是教学演示，不能宣称防逃逸。

委派布局：空的委派父节点启用控制器，manager 独立叶子，run_id 对应独占负载叶子；后续 S05 在两机验证，不修改系统根层级。
S00 不创建进程隔离、Rootfs、挂载或 cgroup；daemon/UDS/网络/Seccomp 均不实施。
