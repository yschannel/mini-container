# S02 阶段报告（A / Ubuntu）

状态：V0 实现及直接编译验证完成；标准 CMake 构建待 B 源登记，本人 GDB 待完成，未合并。按 A 明确授权，S01 理解确认保留真实状态但不阻塞开发。不得进入 S03。

前置 C01：`22019e8d860464936d21ab3784c6fcf27f070269`（PR #1 合并提交）。开始时 main HEAD=origin/main，工作区干净。用户确认 S01 两机构建与 A 审查完成；没有代签尚未完成的理解确认。

范围：src/main_cli.cpp、src/core/runtime.cpp、src/common/、A 的 S02 记录和 docs/stages.md。公共头/B 文件未改。V0 使用 fork/execvp、CLOEXEC 错误管道、准备/失败定长记录、EINTR/短读写处理、waitpid 回收、stdin=/dev/null、关闭无关 fd。signalfd 接收阻塞的 SIGINT/SIGTERM，正常控制流对直接子进程 SIGKILL 并 wait；子进程 exec 前恢复信号掩码。不创建 namespace、Rootfs、cgroup，不管理任意后代。

错误输出区分 startup_error/exited/signaled，证据输出失败在 stderr 报告并确保非零。enabled 全部 false，quota 空，period 默认值不表示 CPU 限额启用。主错误与收尾字段独立。早死且未报告准备的情况为 sync 错误；准备后无错误记录的信号死亡保留 signaled，不宣称已确认 exec 成功。

验证命令与构建交接见 build-handoff.md。原始输出 artifacts/s02/；复现脚本 verify.py 逐例记录 CASE_ID、完整命令、时间、系统、代码 SHA、预期、实际、状态与证据。首轮测试发生在 C01 的未提交实现工作区；提交后必须重测，以后续记录为准。

| 检查 | 状态 | 结果 |
|---|---|---|
| 直接 Debug 编译，-Wall -Wextra -Wpedantic | PASS | 无警告 |
| true / exit7 / 不存在程序 | PASS | 0/exited；7/exited；127/startup_error exec ENOENT |
| 权限拒绝、带空格和 shell 字符的 argv | PASS | 126/exec EACCES；参数原样输出 |
| 无参数、非法模式、full | PASS | 创建子进程前 validate，125 |
| 负载自身 125/126/127、信号退出、stdin EOF | PASS | 保留 exited；143/signaled；cat 得到 EOF |
| CLI SIGINT/SIGTERM、重复 10 次 | PASS | 有界终止且直接子进程已回收 |
| 标准 CMake 配置/构建 | NOT_RUN | B 源登记待完成；不可把直接编译冒充此项 |
| CTest / ASan / openSUSE S02 | NOT_RUN | 不宣称已验证，S03 不提前开始 |
| A 本人 GDB | NOT_RUN | gdb-guide.md；等本人反馈 |

限制与交接：当前 CMake 未登记新源，标准入口链接会缺定义，B 需按 build-handoff.md 提交最小登记；A 随后标准构建和复验。B 代码审查待完成。无自动合并，C02 未知。V0 仅面向可信、有限、不守护化负载，后代管理不在此阶段范围。报告不声称完整 V1 或阶段最终通过。
