# S02 A 本人 GDB 实验过程

执行人：A；整理日期：2026-10-31（Asia/Shanghai）。环境：Ubuntu。
证据来源：A 在聊天中逐组回传的实际终端输出，由 Agent 整理；不是 Agent 代跑。未启用 GDB 会话日志，不声称存在 session.log。

## 二进制与范围

本次使用 `build/s02/minic`，即 S02 直接以 `-g -O0` 构建、带 debug_info 的二进制，未使用较早的 `build/debug/minic`。构建命令及实现提交见 [复测摘要](verification.md) 与 [构建交接](build-handoff.md)。对应已记录的实现 SHA 为 `46af9df64deaf85283ee11680d9639c0c69e69c7`；本次调试没有额外回传当场的 git SHA 或重新构建输出，不冒充已做新的二进制溯源验证。

目标：在 fork 后、exec 前停住，查看 argv、inferiors 和调用栈，观察程序替换及父进程回收。只验证 V0，不涉及 namespace、Rootfs 或 cgroup。

## 1. 启动并跟随子进程

仓库根目录启动：

```bash
gdb --args ./build/s02/minic run --mode v0 \
  --evidence-dir "$PWD/artifacts/s02/gdb" -- /bin/echo 'hello world'
```

GDB 中实际执行：

```gdb
set pagination off
set follow-fork-mode child
set detach-on-fork on
break v0_child_exec
run
```

遇到 debuginfod 下载提示，A 输入 `n`，继续使用程序已有符号。
实际输出节选（省略线程库信息与地址）：

```text
V0 非隔离运行；非完整 V1
[Attaching after Thread ... (LWP 40619) fork to child process 40645]
[New inferior 2 (process 40645)]
[Detaching after fork from parent process 40619]
[Inferior 1 (process 40619) detached]
Thread 2.1 "minic" hit Breakpoint 1, minic::(anonymous namespace)::v0_child_exec
(error_fd=6, argv=..., old_mask=..., max_fd=1024) at src/core/runtime.cpp:34
34    if (sigprocmask(SIG_SETMASK, &old_mask, nullptr) < 0) {
```

观察：父 PID 40619 脱离 GDB 后继续运行；子 PID 40645 被暂停在入口。error_fd=6 是错误管道写端。

## 2. 检查参数与调用栈

```gdb
info inferiors
print argv[0]
print argv[1]
print argv[2]
bt
```

实际输出节选（路径按仓库相对路径展示）：

```text
  Num  Description       Connection    Executable
  1    <null>                          .../build/s02/minic
* 2    process 40645     1 (native)     .../build/s02/minic
$1 = ... "/bin/echo"
$2 = ... "hello world"
$3 = 0x0
#0 v0_child_exec (...) at src/core/runtime.cpp:34
#1 minic::Runtime::run (...) at src/core/runtime.cpp:103
#2 main (argc=9, argv=...) at src/main_cli.cpp:37
```

观察：当前 inferior 为子进程；`hello world` 是一个完整参数，argv 以空指针结尾。调用栈仍属于 minic，尚未替换程序。

## 3. 精确停在 execvp 前

```gdb
tbreak src/core/runtime.cpp:52
continue
print argv[0]
print argv[1]
bt
```

实际输出节选：

```text
Temporary breakpoint 2 at ...: src/core/runtime.cpp:52. (2 locations)
Thread 2.1 "minic" hit Temporary breakpoint 2.2, ... v0_child_exec (...)
52    execvp(argv[0], argv);
$4 = ... "/bin/echo"
$5 = ... "hello world"
#0 v0_child_exec (...) at src/core/runtime.cpp:52
#1 minic::Runtime::run (...) at src/core/runtime.cpp:103
#2 main (argc=9, argv=...) at src/main_cli.cpp:37
```

此时 execvp 还未执行。根据源码顺序，子进程已经过准备记录的写入位置；本次没有直接读取管道字节作为额外证据。

## 4. 继续执行与程序替换

```gdb
continue
```

实际输出：

```text
process 40645 is executing new program: /usr/bin/echo
warning: could not find '.gnu_debugaltlink' file for /usr/bin/echo
Error in re-setting breakpoint 1: Function "v0_child_exec" not defined.
hello world
[Inferior 2 (process 40645) exited normally]
```

观察：exec 后 PID 仍为 40645，程序变为 echo。旧函数断点无法重设，是程序已被替换；echo 的额外调试信息缺失未影响本次机制观察。输出正确，正常退出，没有遇到需要修复的缺陷。

## 5. 检查结果与进程结束

退出 GDB 后在 shell 执行：

```gdb
quit
```

```bash
cat artifacts/s02/gdb/result.json
ps -p 40619,40645 -o pid,ppid,stat,cmd
```

A 回传的完整 JSON：

```json
{"schema_version":1,"run_id":"40619-13022120678707","mode":"v0","kind":"exited","host_pid":40645,"exit_code":0,"signal":null,"primary_error":null,"snapshot_error":null,"enabled":{"pid_namespace":false,"uts_namespace":false,"mount_namespace":false,"rootfs":false,"cgroup":false,"limits":{"memory_bytes":null,"swap_max_bytes":null,"cpu_quota_us":null,"cpu_period_us":100000}},"cleanup":{"attempted":true,"complete":true,"errors":[]},"snapshot":null,"evidence_errors":[]}
```

ps 输出只有表头：

```text
    PID    PPID STAT CMD
```

结论：JSON 为 exited/0，清理完成且无错误；检查时两个 PID 均不存在。未单独执行外层 shell 的 `$?`，不能将 JSON 退出值写成实际观察过 shell 状态。

## 6. 本人理解确认与机制解释

A 实际回答：

> fork创造子进程分支，exec更换子进程正在执行的程序，wait等待子进程结束后回收信息

对于 EOF，A 实际回答：

> 因为也有可能是exec失败而导致的程序结束，此时也返回EOF

结合指导补充的准确语义：fork 创建子进程；exec 替换当前进程程序但不改变 PID；wait 获取终止状态并完成回收，避免僵尸进程。父进程持有错误管道读端，子进程持有 CLOEXEC 写端；成功 exec 自动关闭写端，失败应先写 step/errno 再 `_exit`。EOF 只表示所有写端关闭，提前死亡也会产生 EOF，因此必须结合协议记录和 wait 状态判断。

本次亲手操作及 fork/exec/wait、CLOEXEC、EOF 讨论已完成。CLOEXEC 关闭行为依据源码与语义解释，本次未额外用 strace 检测；不扩大为独立 syscall 跟踪通过。其他尚未完成的 S01 理解项保持原状态。

## 7. 结论与后续

S02-HUMAN-GDB：PASS，A 亲手完成预期断点、参数、调用栈和程序替换观察，结果检查符合预期。本次为机制验证，无虚构故障修复。未重复 25 项自动验证。

本记录不表示 S02 已最终验收或合并；B 的 CMake 源登记、标准构建复验与代码审查仍按实际交接推进，不开始 S03。
