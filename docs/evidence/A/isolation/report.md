# A-1 隔离模块交接

状态：Ubuntu isolate 子集技术验收及 A 本人 strace 完成；待推送最后修复/证据、B 代码审查。不自动合并、不开始 full 集成。

开发基线/main C02：`3d467120786dce63b841820c60dd7e9c4e99eb4e`。开始时 main=origin/main，仅新版 V1_ACCELERATED_WORKFLOW.md 未跟踪并带入分支，无源码修改被覆盖；未发现 AGENTS.md。采用新版并行流程，S02 已完成，不重复旧门禁。

被测实现：`66664782a71de911c9e5cd92e561e4c46e8ad1cb`；8 项原生记录均 dirty=false。分支 a/v1-isolation。后续证据提交不改变实现。

## 改动与接口

RunConfig/RunResult 公共签名不变。CLI 开放 isolate 参数，full 继续拒绝；Runtime clone 独立 1 MiB 栈及 PID/UTS/Mount namespace，无共享 VM/文件表。父保持栈与参数直至 wait 回收。控制通道为 CLOEXEC socketpair 的单向使用，父明确发送 G、子等待；EOF/错误/非 G 不执行负载，MSG_NOSIGNAL 防止父写端 SIGPIPE。子关闭无关宿主 fd，设置 private 传播、hostname、chroot/chdir /、临时 tmpfs /dev/null 和 proc，再 exec。

错误管道保存 step/errno，结合准备记录与 wait 分类；JSON 写实际启用功能。signalfd 在正常控制流处理中断，以 SIGKILL 有界终止自己的 namespace PID 1，再 wait，由 PID namespace 生命周期终止后代和释放内部挂载。只支持可信有限非守护化负载；无 init，不承诺 PID 1 响应普通 SIGTERM或运行器自身 SIGKILL 后恢复。

Rootfs 脚本限项目 .local 专用新目录，拒绝既有目录、软链接和 ..，不删除未知内容；验证 BusyBox ELF 架构、静态标记与 INTERP/NEEDED。已确认本机 BusyBox x86_64 静态，Rootfs 资产不入 Git。临时 /dev 挂载在子 namespace 内，不绑定宿主 /dev，不遗留设备节点到资产目录。

仅修改 A 的 CLI/Runtime/common、Rootfs 脚本、架构/台账/证据及新增流程文档；没有修改 B cgroup/tests 或公共签名。新实现位于现有构建源中，无需 CMake 改动。

## 验证

| 检查 | 状态 | 证据与范围 |
|---|---|---|
| 标准 Debug CMake 构建 | PASS | cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug；cmake --build build/debug -j2；修复后重建无警告 |
| 受影响 V0 CTest | PASS | ctest --test-dir build/debug --output-on-failure，v0_regression 1/1 |
| 静态 BusyBox Rootfs 准备 | PASS | scripts/prepare-rootfs.sh；.local/isolation-rootfs |
| 缺 Rootfs、原生 live、三轮含 exec 失败 | PASS | native-verification.md；hostname/PID/ns/proc/Rootfs、宿主状态不变与直接子进程回收 |
| 准备失败处理 | PASS（模拟注入） | strace 注入 mount EPERM，step=mount.private，回收成功；不替代能力实测 |
| SIGINT/SIGTERM 与后代回收 | PASS | 先确认后台后代存活，中断后 137/signaled 且本轮进程消失 |
| A 本人 strace | PASS | strace-record.md，真实 clone→放行→private/hostname/chroot/dev/proc→exec→wait |
| Agent namespace 探测 | SKIP | Agent clone EPERM，区别于 sudo 原生环境 |
| openSUSE isolate、cgroup/full | NOT_RUN | 不扩写为双系统完整 V1 通过 |

两轮失败与实际修复见 first-native-run.md：BusyBox 后台任务需要 /dev/null；仅重定向不足，补私有设备后原生 8 项通过。旧 artifacts 保留，没有覆盖或隐藏失败。原生结果 artifacts/isolation/native-20261031-162650；本人 trace 在 artifacts/isolation/manual；精选证据已提交文档，不提交大量原始资产。

## 交接

B 审查控制/错误通道、namespace/Rootfs 顺序、栈/fd 生命周期与清理。PR 描述见 pr-body.md；需确保远端包含实际修复 6666478 及最终证据，不能基于旧 b5fab92 宣布通过。未请求合并，不接入 full。11-02 起冻结新增功能。
