# S02 A 本人 GDB 练习指南

A 已亲手完成本次练习，实际过程见 [GDB 实验记录](gdb-record.md)。下文保留原操作指南，其中日志命令不是本次实际执行记录。Agent 未代本人操作。B 登记 CMake 后优先使用 `build/debug/minic`；此前可使用已带 `-g -O0` 的 `build/s02/minic`，需如实记录构建方式。

在仓库根目录亲手执行：

```bash
mkdir -p artifacts/s02/gdb
# B 完成 CMake 登记后把下行路径改为 build/debug/minic
gdb --args ./build/s02/minic run --mode v0 --evidence-dir "$PWD/artifacts/s02/gdb" -- /bin/echo 'hello world'
```

进入 GDB 后：

```gdb
set pagination off
set logging file artifacts/s02/gdb/session.log
set logging enabled on
set follow-fork-mode child
set detach-on-fork on
break v0_child_exec
run
info inferiors
print argv[0]
print argv[1]
bt
list
```

此时应处于 fork 后的子进程入口。用 `next` 前进到 `execvp` 行（不要用固定行号，`list` 核对），再观察 argv、栈和 error_fd。`next` 可能跨过循环，按实际停点记录。最后：

```gdb
continue
set logging enabled off
quit
```

父进程被 detach 后仍会等待子进程，调试期间不要把停住的子进程当成运行器挂死。若 ptrace 被环境拒绝，记录实际错误，在普通 Ubuntu 终端重试，不关闭安全策略。

请回传：当前 `git rev-parse HEAD`、实际构建命令、`info inferiors`/argv/bt 的观察，以及你对以下三点的解释：

1. fork 创建子进程，exec 替换子进程程序，wait 回收并读取终止状态。
2. 错误写端 CLOEXEC 使成功 exec 自动关闭写端；失败则写 step/errno，随后 `_exit`。
3. EOF 只表示写端都关闭，子进程提前死亡也会产生 EOF；需结合准备记录和 wait 状态，不单靠 EOF 判断成功。

以上是指导与预期，不是本人实测记录。S01 尚未完成的理解确认另按实际反馈补齐。
