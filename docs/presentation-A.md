# A的启动与隔离讲解稿

角色：A / yschannel。配合共同9分钟提纲，A的核心启动讲解约2分钟，隔离演示约1分钟，再用约30秒说明一次真实调试。此稿已根据代码整理，现场彩排尚未执行；B资源/工具细节由其独立材料维护。

材料main基线：`25cf83acb7bfe080e12c1eb19d4318721f7fb109`；同候选双机12项被测SHA：`97800f0244b807466e7c516021f3c40b882abf45`。以下行号按该main代码定位，函数名为主，旧GDB/strace记录的历史行号不强行替换。

## 第一段：从参数变成一次运行请求

打开 [main_cli.cpp](../src/main_cli.cpp#L18) 的main，重点看RunConfig、选项循环、`--`之后argv，以及第55行调用Runtime::run。模式默认full，现场仍显式写模式；内存是字节，CPU是微秒，数值用from_chars严格解析。`--evidence-dir`转为绝对路径；Rootfs、委派路径在Runtime进一步校验。

讲法：“CLI只把用户请求整理成配置，真正的运行生命周期交给Runtime。PROGRAM后的每个参数单独保存，不拼成一段shell去执行。如果我显式指定/bin/sh -c，那是我的负载，不是运行器偷偷解析命令。”

接着打开 [Runtime::run](../src/core/runtime.cpp#L124)，看full必需正内存/CPU、有效period、Rootfs标记与真实dev/proc目录。**为什么先验证？** 在创建负载前拒绝不完整配置，不能full失败后自动变成isolate；RunConfig是“请求”，RunResult是“实际发生”。

## 第二段：clone产生PID，但先不让负载执行

看 [clone与独立栈](../src/core/runtime.cpp#L201) 和 [isolate_child_entry](../src/core/runtime.cpp#L74)。clone标志为PID/UTS/Mount三个namespace及SIGCHLD，独立1MiB回调栈，参数/栈在父回收前保持。

讲法：“父进程需要一个宿主PID，才能把它加入cgroup。但我们又不希望程序先跑起来。所以子回调先关闭无关fd，再read控制通道，只接受父进程的G。没有G、遇到EOF或读错，就退出，不执行工作负载。”

对照 [A-1运行期PID/ns记录](evidence/A/isolation/native-verification.md)：宿主PID42879与容器PID1指同一任务，从不同PID namespace观察得到不同编号；namespace链接不同也与真实/proc视图互相印证。该PID仅是历史样本，演示不能写死。

## 第三段：父进程设置限制、attach，成功才放行

看 [FullSession::prepare](../src/common/full_session.hpp#L10)：create→保存唯一会话→attach→enabled。B的 [CgroupSession::create](../src/core/cgroup.cpp#L83) 验证委派、独占创建run_id叶子，写入并读回限额；[attach](../src/core/cgroup.cpp#L145) 写入真实host_pid并检查成员。Runtime的 [allow_release/send G](../src/core/runtime.cpp#L214) 只在成功、未取消时放行。

讲法：“namespace负责看见什么，cgroup负责能用多少。只设置一个文件名不算生效，我们还读回配置并确认成员。先attach后G，消除负载不限额运行的窗口。任何一步失败都关闭控制端，终止并wait，而不是悄悄放开限制。”

分工边界：委派scope/manager/jobs由外部脚本准备，Runtime使用基目录；不让CgroupSession擅自创建系统根层级。更详细控制器和资源解释交B。

## 第四段：子进程在自己的空间里准备Rootfs与exec

看 [子回调第94行起](../src/core/runtime.cpp#L94)：先private传播，再hostname，chroot后chdir /，临时tmpfs /dev中创建null设备，新PID namespace中挂proc，恢复信号掩码并execvp。

讲法：“private必须在新挂载前做，避免传播到宿主。chroot只换文件系统根，chdir还要清掉旧cwd；/proc必须在新PID namespace里挂。最后exec只替换该任务的程序，宿主PID不因此改变。我们没有init包装，负载可以直接是namespace PID1。”

真实问题闭环：[两轮失败记录](evidence/A/isolation/first-native-run.md)。最小Rootfs缺/dev/null，shell后台任务即使继承stdin仍会打开/dev/null；首个重定向修正不足，实际补子空间临时字符设备后验收通过。它解释了为什么“主程序能启动”不等于“后台子任务路径都准备好了”。

错误通道是另一条CLOEXEC管道，返回step/errno和准备记录。exec成功通常关闭写端，但早死也会产生EOF；父必须结合协议及wait状态，不能见EOF就报成功。已有 [本人GDB](evidence/A/S02/gdb-record.md) 和 [本人strace](evidence/A/isolation/strace-record.md) 复用，不重做。

## 第五段：wait、snapshot、cleanup与结果

看 [Runtime信号与等待循环](../src/core/runtime.cpp#L248)、[FullSession::finish](../src/common/full_session.hpp#L21) 和 [CLI结果输出](../src/main_cli.cpp#L56)。父处理管道和signalfd，收到中断在正常控制流对自己的namespace init执行有界终止，再回收。PID1不保证响应普通SIGTERM，因此不能假定转发一次就结束。

讲法：“wait证明负载结束且完成回收，但资源证据还没保存。我们先snapshot再cleanup，OOM后才读取一个已删除路径是不成立的。清理失败单独报告，不能盖掉原本的exec错误或负载退出值。”

快照保留memory.events/cpu.stat/限制原文；cleanup只删除本轮叶子，失败保留所有权供重试。析构兜底不能替代显式错误报告。文件资产和进程/挂载不同：Rootfs资产保留，namespace结束释放本轮挂载。

## 推荐代码与证据切屏

| 切屏 | 只展示的内容 | 证明/解释 |
|---|---|---|
| main_cli.cpp:18、55 | 配置与Runtime调用 | CLI和运行生命周期分离 |
| runtime.cpp:74、201、214 | 等待、clone、放行 | 有PID但负载未执行；成功限额/attach后才G |
| runtime.cpp:94 | private→hostname→chroot/chdir→dev/proc→exec | 宿主不变、子视图改变 |
| full_session.hpp:10、21 | prepare/finish | attach前置，snapshot先于cleanup |
| ubuntu-97800f0.md | attach ESRCH、CPU/OOM、中断成员 | 真实同候选12项；测试构建诱发内核拒绝需明确 |
| strace-record.md | 两个PID文件的关键调用 | 本人观察，非新跑的full trace |

## A的短演示建议（尚待彩排）

不要现场重跑完整12项或临时安装。先确认已有正确Rootfs和Debug二进制，展示当前SHA；下面只用已有CLI，不新增demo脚本：

```bash
git rev-parse HEAD
hostname
sudo ./build/debug/minic run --mode isolate \
  --rootfs "$PWD/.local/isolation-rootfs" --hostname minic-report \
  --evidence-dir "$PWD/artifacts/report-isolate" -- \
  /bin/sh -c 'hostname; echo "PID=$$"; ps; ls -a /'
hostname
```

预期：容器输出minic-report、PID=1、Rootfs标记，宿主hostname前后相同。明确这是isolate演示，full资源数据另由B展示，不把此例当full通过。

可选失败短例：

```bash
sudo ./build/debug/minic run --mode isolate \
  --rootfs "$PWD/.local/isolation-rootfs" --hostname minic-report \
  --evidence-dir "$PWD/artifacts/report-missing" -- /not-a-program
printf 'CLI status=%s\n' "$?"
cat artifacts/report-missing/result.json
```

预期127、startup_error/exec/errno2；本命令为演示建议，尚未记录该演示目录的实际结果。现场若失败，保留错误，改展示已入库 [固定候选Ubuntu证据](evidence/A/integration/ubuntu-97800f0.md)，说明是历史实测；不伪装现场成功。B的full demo/委派脚本以其维护的说明为准。

## 本人尚需补充

- 彩排选定一条隔离例和一条失败例，实际输出/耗时/日期；11-03安排主、备用机演示及真实备用录像。
- 与B确认时间和切屏顺序、资源讲解文件路径；封面、课程/小组信息和最终演示机配置。
- 能顺着五段讲清排序即可，不重复GDB/strace或反复答题。已有操作记录不改写成新阶段工具实测。

没有生产级安全承诺，未支持功能不作演示；历史V0 ASan不含LSan，不能说V1“内存无泄漏”。本次材料未新增源码或测试，展示前不要把文档提交SHA写成实际验收SHA。
