# A-1 隔离模块交接（进行中）

开发基线/main C02：`3d467120786dce63b841820c60dd7e9c4e99eb4e`。开始时 main=origin/main，仅新版 V1_ACCELERATED_WORKFLOW.md 未跟踪，随分支保留；没有未提交源码被覆盖。分支 a/v1-isolation。未发现 AGENTS.md。采用新版顺序，S02 已合并并完成标准构建/CTest/本人 GDB，不重开旧门禁。

## 实现

沿用 RunConfig/RunResult 公共签名。Runtime 新增 isolate，clone 使用独立 1 MiB 栈及 CLONE_NEWPID/NEWUTS/NEWNS/SIGCHLD，无共享 VM/文件表。父进程保持栈和参数到 wait 完成。控制通道采用 CLOEXEC Unix stream socketpair（管道语义，父写子读），发送明确 G 字节；MSG_NOSIGNAL 防止父端写入触发 SIGPIPE。子进程关闭其他控制端和宿主 fd，遇到 EOF/错误/非 G 不执行负载。

放行后：stdin 连接宿主 /dev/null → mount propagation private → hostname → chroot/chdir / → 新 PID namespace 的 proc → exec。错误管道返回步骤与保存的 errno；命名空间/Rootfs 启用情况由成功创建与子进程记录填写 JSON。准备失败、exec 失败、中断均 wait 回收；namespace 最后进程消失后其中挂载由内核释放，Rootfs 文件资产保留。

信号仍采用 signalfd 通知正常控制流；对自己的 namespace PID 1 使用 SIGKILL 作有界终止，其后代由 PID namespace 生命周期清理。只支持可信有限非守护负载；不新增 init，不保证 namespace PID 1 响应普通 SIGTERM。full 不开放，不接 cgroup，不修改 B 的实现/测试或公共签名。源码仍登记在现有 runtime/result_io 目标中，无需 CMake 修改。

Rootfs 脚本只接受仓库 .local 下专用新目标，拒绝既有目录/软链接/..，不删除未知内容；验证 ELF 架构、file 静态标记、readelf INTERP/NEEDED。当前 x86_64 BusyBox 静态验证通过，资产生成于 .local/isolation-rootfs，不入 Git。

## 实际检查（实现工作区，待提交后原生复验）

| CASE | 状态 | 实际结果 |
|---|---|---|
| Debug CMake 配置/构建 | PASS | 标准 build/debug 入口，编译无警告 |
| 受影响的 V0 CTest | PASS | v0_regression 1/1；不重复 S02 GDB |
| Rootfs 生成 | PASS | /usr/bin/busybox，x86_64 静态 ELF、无动态节 |
| 缺 Rootfs | PASS | 125/startup_error，step=rootfs，errno=2，未创建子进程 |
| Agent namespace probe | SKIP | 125/startup_error，step=clone，errno=1；不推导 VMware 原生能力 |
| 原生隔离/三轮/失败/中断回收 | NOT_RUN | 等 A 在普通终端 sudo 执行 verify.py |
| 新机制本人 strace | NOT_RUN | 等 A 实际操作及回传，不代签 |

Agent 原始结果：artifacts/isolation/agent-probe/result.json、artifacts/isolation/missing-rootfs/result.json。原生脚本 verify.py 保存每例 command/SHA/dirty/time/status/result、stdout/stderr 和 namespace 对照到 artifacts/isolation/native。strace 注入 mount EPERM 是模拟故障，单独标注；不替代真实能力通过。

## 普通终端接续

先核对 `git rev-parse HEAD` 与当前分支，再执行：

```bash
sudo python3 docs/evidence/A/isolation/verify.py
```

该脚本只创建本项目 artifacts 的证据/宿主测试标记，运行本项目 minic，不操作系统配置、不建立 cgroup、不清空其他任务。需要先存在已准备的 .local/isolation-rootfs。若 Rootfs 尚未生成，先运行 `./scripts/prepare-rootfs.sh --output "$PWD/.local/isolation-rootfs"`；已有目标不重复准备。

本人新机制观察只做一次（不重复 GDB）：

```bash
mkdir -p artifacts/isolation/manual
sudo strace -ff -o "$PWD/artifacts/isolation/manual/trace" \
  -e trace=clone,read,write,sendto,recvfrom,mount,sethostname,chroot,chdir,execve,wait4,kill \
  ./build/debug/minic run --mode isolate --rootfs "$PWD/.local/isolation-rootfs" \
  --hostname minic-trace --evidence-dir "$PWD/artifacts/isolation/manual" -- /bin/hostname
```

观察 clone 三个 namespace 标志、子进程控制读/父放行、private mount/hostname/chroot/chdir/proc、execve、父 wait4 的顺序。trace 多文件按 PID 区分，暂不声称已完成。回传脚本末行、verification.json 和 strace 关键片段后记录实测结论。

## 未完成

原生验收、本人 strace、B 审查、PR 和合并均未完成。不能宣称隔离技术验收通过；不开始 full 集成。提交后报告实际被测 SHA，纯文档不重复全套测试。11-02 起冻结新增功能。

原生复验进展：两轮均在 SIGINT 用例准备阶段发现 Rootfs 缺少 /dev/null，尚未测到中断；详见 first-native-run.md。Runtime 已补子 namespace 临时 /dev/null，待 A 原生复验，不将已有结果扩写为全通过。
