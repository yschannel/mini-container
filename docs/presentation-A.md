# A单人6分钟汇报与代码理解附录

角色：A / yschannel，2026-11-04独自完成6分钟汇报，Ubuntu主演示，B不上台。此稿覆盖目标、完整启动、隔离、cgroup、CPU/OOM、清理、工具、双系统验收与限制。先完成材料，再按下面的小段结合代码学习；不靠反复答题阻塞材料工作。彩排尚未执行。

开场讲法：“这是我和组员B共同开发的教学型容器项目。我负责Runtime、隔离/Rootfs、full集成和Ubuntu验证；B负责cgroup资源模块、负载/探针、构建测试支持、统一验收和openSUSE验证。今天由我统一讲解。”B可台下提供材料和技术审校，但不安排接话。讲B的实现与操作时明确说“组员实现/验证”，不改写为A独立完成。

材料main基线：`25cf83acb7bfe080e12c1eb19d4318721f7fb109`；同候选双机12项被测SHA：`97800f0244b807466e7c516021f3c40b882abf45`。以下行号按该main代码定位，函数名为主，旧GDB/strace记录的历史行号不强行替换。

## 现场六分钟口播（正文，约5分40秒内容＋20秒缓冲）

下列文字是口播草稿，不是已计时通过的彩排。代码长讲解移至后面的附录，不在台上逐条展开。资源/工具摘要吸收B在PR #7（已审查f2a08e6）提供的材料，保留其贡献归属；不直接使用原三分钟openSUSE完整验收演示安排。

### 0:00–0:25：目标与分工

“我们用Linux机制实现了一个教学型容器运行时。我负责程序启动、隔离、Rootfs和集成，组员B负责cgroup资源模块、负载与测试，以及openSUSE验证。今天我在Ubuntu上统一演示。项目支持基础执行、隔离和full限额三种模式，只面向可信的前台负载。”

屏幕：目标/分工页。页脚区分main合并SHA25cf83a和两机被测SHA97800f0，不口头逐字念完整SHA。

### 0:25–1:40：完整启动顺序

“CLI先把参数整理成RunConfig，验证Rootfs和限额。然后clone创建PID、UTS和Mount namespace，子进程先等待，不立即执行程序。父进程创建本轮cgroup，写入并读回限制，再attach子进程的宿主PID，全部成功才发送G放行。这消除了先运行、后补限额的窗口；失败就终止并回收，不降级成不限额运行。

子进程收到G后，先把挂载传播设为private，再设置hostname、chroot并chdir到根目录，准备私有/dev/null和新proc，最后exec。宿主PID与容器里的PID1是同一个任务在不同视图里的编号。父进程wait回收后，先保存cgroup快照，再cleanup并写结果。”

屏幕：只用一张 [时序图](architecture.md)，突出两处顺序：attach在放行前、snapshot在删除前。源码切屏留给问答。

### 1:40–2:25：Ubuntu隔离短演示

运行本文件后面的短isolate命令；不现场跑完整12项。

“这里展示的是isolate模式。容器hostname变成minic-report，shell看到自己的PID是1，proc显示新namespace里的进程，根目录有专用Rootfs标记。再看宿主hostname，它没有改变。这一例只展示隔离，full资源效果接下来用同候选的实测快照说明。”

如果操作超过预留时间，保留错误并切既有记录，不追加一轮完整验收。

### 2:25–3:40：cgroup、CPU与OOM

“资源控制部分由B实现。systemd为本次运行委派cpu和memory，管理进程在独立manager叶子，负载进入自己的run_id叶子。

CPU配置20000/100000的单位是微秒，约为单核20%的时间额度。Ubuntu实测五秒多墙钟使用约1.02秒CPU，发生51次节流，比例约20.12%。所以我们既读回配置，也检查cpu.stat，不只看top截图。

内存限制64MiB、swap为0，负载实际写入96MiB。本轮独占叶子的memory.events记录oom_kill为1，同时负载被signal9终止。137单独不能证明OOM，必须结合本轮事件。快照里的memory.current是结束时当前量，不是峰值。”

屏幕：打开 [Ubuntu97800f0记录](evidence/A/integration/ubuntu-97800f0.md) 的CPU/OOM摘录。明确这是先前实机快照。baseline是isolate且未采CPU使用计数，不声称已有完整基线计数差值。

### 3:40–4:20：失败与清理

“负载自己退出127，kind仍是exited；exec找不到程序才是startup_error和exec步骤。create或attach失败时，子进程不会被放行。attach负例通过测试专用构建诱发真实内核ESRCH，不是生产程序自然故障。

结束后先wait、再snapshot、最后cleanup。快照保留OOM和CPU统计，删除只针对本轮叶子。中断测试先看见init和后代，再终止CLI，确认进程和叶子消失。主错误与清理错误独立保存。”

屏幕：已有JSON/结果表，不再现场跑第二条失败命令。可选失败命令仅放附录供问答或彩排选择。

### 4:20–5:15：工具与双系统验证

“我用GDB观察过子进程参数、调用栈和exec替换，也用strace观察过隔离调用顺序。我们实际发现过Rootfs缺/dev/null，导致后台任务未启动；仅改重定向还不够，补私有设备后复验通过。

B还维护CTest与ASan验证。CTest的1项是非特权V0回归，特权12项另走委派入口。历史V0 ASan关闭LeakSanitizer后通过，不能说泄漏检测通过或覆盖了内核资源回收。

Ubuntu由我执行，openSUSE由B执行，在相同干净候选97800f0上各12项原生验收通过。后续合并和文档SHA与被测SHA分开记录。”

屏幕：工具证据缩略图和两机结果表，不重新启动GDB/strace。

### 5:15–5:40：限制与结束

“这是教学原型，不是生产安全容器。没有daemon、网络、Seccomp、TTY或init包装，也不保证运行器被SIGKILL或掉电后的恢复。我们保留失败记录，对已验证范围负责。11月2日起冻结功能，接下来只做材料、彩排和必要修复。以上是我们的实现与验证。”

5:40–6:00只作切屏和操作缓冲，不再增加正文。B不上台接话；需要资源问答时A按下方附录回答，不能把B的亲手操作说成自己做的。

## 代码理解附录（材料完成后分段学习，不逐字上台读）

先按参数→clone/等待→create/attach→G→Rootfs/proc/exec→wait→snapshot→cleanup理解，再逐段补CPU和OOM；一次只讲一个小段，不反复答题、不重做已完成的工具操作。下列十段、代码位置和可选命令保留用于准备与问答。

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

分工边界：委派scope/manager/jobs由外部脚本准备，Runtime使用基目录；不让CgroupSession擅自创建系统根层级。这部分由B实现和维护，台上由A继续按下面的资源段统一解释。

## 第四段：子进程在自己的空间里准备Rootfs与exec

看 [子回调第94行起](../src/core/runtime.cpp#L94)：先private传播，再hostname，chroot后chdir /，临时tmpfs /dev中创建null设备，新PID namespace中挂proc，恢复信号掩码并execvp。

讲法：“private必须在新挂载前做，避免传播到宿主。chroot只换文件系统根，chdir还要清掉旧cwd；/proc必须在新PID namespace里挂。最后exec只替换该任务的程序，宿主PID不因此改变。我们没有init包装，负载可以直接是namespace PID1。”

真实问题闭环：[两轮失败记录](evidence/A/isolation/first-native-run.md)。最小Rootfs缺/dev/null，shell后台任务即使继承stdin仍会打开/dev/null；首个重定向修正不足，实际补子空间临时字符设备后验收通过。它解释了为什么“主程序能启动”不等于“后台子任务路径都准备好了”。

错误通道是另一条CLOEXEC管道，返回step/errno和准备记录。exec成功通常关闭写端，但早死也会产生EOF；父必须结合协议及wait状态，不能见EOF就报成功。已有 [本人GDB](evidence/A/S02/gdb-record.md) 和 [本人strace](evidence/A/isolation/strace-record.md) 复用，不重做。

## 第五段：wait、snapshot、cleanup与结果

看 [Runtime信号与等待循环](../src/core/runtime.cpp#L248)、[FullSession::finish](../src/common/full_session.hpp#L21) 和 [CLI结果输出](../src/main_cli.cpp#L56)。父处理管道和signalfd，收到中断在正常控制流对自己的namespace init执行有界终止，再回收。PID1不保证响应普通SIGTERM，因此不能假定转发一次就结束。

讲法：“wait证明负载结束且完成回收，但资源证据还没保存。我们先snapshot再cleanup，OOM后才读取一个已删除路径是不成立的。清理失败单独报告，不能盖掉原本的exec错误或负载退出值。”

快照保留memory.events/cpu.stat/限制原文；cleanup只删除本轮叶子，失败保留所有权供重试。析构兜底不能替代显式错误报告。文件资产和进程/挂载不同：Rootfs资产保留，namespace结束释放本轮挂载。

## 第六段：A需要掌握的cgroup与委派

代码：[CgroupSession::create](../src/core/cgroup.cpp#L83)、[attach](../src/core/cgroup.cpp#L145)，以及scripts/with-delegation.sh和tests/full_acceptance.py。公开接口是create/attach/snapshot/cleanup，会话只在父进程持有，不在模块内部启动负载。

讲法：“namespace改变负载看到的系统视图；cgroup把宿主上的任务归入一个资源组，限制和统计该组的资源使用。两者配合，但不是同一种机制。这里的cgroup模块和探针由B实现，我负责Runtime在正确时间调用它们。”

理解三个目录：systemd为本次运行建立独占scope；管理进程在manager叶子；空jobs父节点启用cpu/memory，每轮run_id是其下独占叶子。这样不把管理器也限成负载，不在有内部进程的父节点上违规启用domain控制器。控制器存在、可写、已沿祖先委派是不同前提；sudo并不自动满足全部条件。

create验证基目录为cgroup v2、控制器可用且对子节点已启用，独占mkdir，写限制并读回；attach把**宿主PID**写入cgroup.procs并核对成员。容器内PID1不是要写到宿主cgroup.procs的数字。配置/attach后才G，后代继承组归属，因此后台任务也被约束。

## 第七段：读懂CPU配置与统计

代码：cgroup.cpp中create的cpu.max写入/读回、snapshot的cpu.stat读取；测试full_acceptance.py的cpu-limited。参数用微秒：`cpu.max = 20000 100000`表示每100ms周期最多约20ms CPU时间，相当于单核约20%的额度，不是整台多核机器CPU总量的20%，也不承诺每个瞬间都精确20%。

同候选Ubuntu实测（[原始精选记录](evidence/A/integration/ubuntu-97800f0.md)）：

| 指标 | 实际值 / 含义 |
|---|---|
| 墙钟 | 5.076948秒，负载在现实中运行的时间 |
| usage_usec | 1021486，即该组约1.021486秒CPU时间，含组内任务累计 |
| nr_periods / nr_throttled | 51 / 51，观察到周期与被节流周期计数 |
| throttled_usec | 3991774，内核记录的节流时间；不当作负载完成工作量 |
| CPU用量/墙钟 | 约20.12%单核时间，结合限制读回与节流证明本次限额效果 |

讲法：“我不只给出top截图。先核对cpu.max，再看cpu.stat和墙钟。本次有读回、有CPU累计用量、有节流计数，证据能相互支持。”

baseline是isolate下相同的5秒有界负载，只有墙钟和退出结果，没有基线cgroup CPU计数；不能声称这次采了基线/限额两份完整usage差值。不同机器、负载竞争会影响数值；使用证据中的同一轮数据，不拼接其他候选。

## 第八段：OOM事件与内存限制

代码：create写memory.max/memory.swap.max，snapshot保存memory.events；full_acceptance.py的memory-oom及B的有界逐页写负载。限制作用于该cgroup的内存记账，不是“malloc参数大于64MiB就立刻返回错误”；分配与实际触页不同，负载会实际写页。

Ubuntu固定候选使用memory.max=67108864（64MiB）、memory.swap.max=0，负载请求并写96MiB，结果signal9/CLI137，独占叶子快照含oom=1、oom_kill=1。swap0是禁该组swap，不是把内存上限设为0；memory.current是快照时当前用量，负载结束后的低值不能冒充峰值或“未触发OOM”。

讲法：“137只能说明按约定报告了SIGKILL，也可能来自手工终止。我要结合本轮独占cgroup的memory.events，看到oom_kill，才能把这次结果归因到内存OOM。oom表示进入OOM处理的事件，oom_kill才表示发生了OOM杀进程，不能互换。”

该轮叶子是新建独占的；保存了结束前/删除前事件，但没有额外保存起始快照，不说成已测两份前后差值。OOM发生后先采样再删除，否则失去证据。

## 第九段：清理到底证明了什么

代码：[FullSession::finish](../src/common/full_session.hpp#L21)、[CgroupSession::cleanup](../src/core/cgroup.cpp#L177)、Runtime等待循环，以及full_acceptance.py中两种中断检查。

顺序是“负载终止→wait回收→snapshot→cleanup”。wait回收进程和退出状态；snapshot保留内核计数；cleanup尝试rmdir本轮独占叶子，不递归删除未知路径。cleanup失败保留所有权，结果不能写complete=true；析构只是兜底，不代替显式报告。Rootfs文件资产留在本机，与本轮namespace挂载及cgroup叶子是不同资源。

Ubuntu的SIGINT运行期cgroup成员为47882/47883，SIGTERM为47885/47886；测试先看到init和后代，再中断宿主CLI。结果signal9、populated=0，成员/proc路径和叶子消失、cleanup.complete=true。这里由namespace生命周期终止后代；没有额外init包装，也不保证负载作为PID1响应普通SIGTERM。

讲法：“我展示正常、exec失败、create/attach失败和CLI中断。主错误与清理错误分开保存，所以不会用一次cleanup失败盖掉原本的exec错误；也不会因为负载exit0就忽略收尾失败。”

attach-failure用测试专用minic_attach_rejection把PID改为INT_MAX，实际内核返回ESRCH；子进程未放行、负载标记未出现。必须说清它是测试构建诱发真实错误，不是生产程序自然失败，更不能把测试入口拿来做正常演示。

## 第十段：工具、两机验收与限制，由A统一收尾

A亲手GDB观察fork子进程argv/调用栈/exec程序替换，亲手strace观察隔离系统调用顺序；相关历史记录直接展示，不说成今天重跑或full GDB。/dev/null问题讲真实两轮修正经过，不虚构一次成功定位。

B在openSUSE维护构建/测试和资源探针，亲手运行对应验证。B历史V0 ASan在禁用LeakSanitizer后通过；LSan初始化失败，既不证明泄漏检测通过，也不证明完整V1无内存或内核资源问题。A介绍这段时说“B的记录显示”，不说“我在openSUSE调试”。

同候选97800f0两机各12项PASS、Debug和CTest 1/1通过；main合并SHA25cf83a与实际被测SHA分别标明。Ubuntu证据由A执行，openSUSE证据由B执行。历史失败和NOT_RUN保留在原记录，采用各报告最新复验小节解释当前状态。

收尾讲法：“完成的是可信、有限、前台非交互负载的教学原型。没有生产级防逃逸承诺，也没有daemon、网络、Seccomp、TTY、User namespace、init包装或pivot_root。我们用同一候选两机验证了列出的路径；不把这些结果扩展成任意负载、运行器SIGKILL或断电恢复保证。”

## 推荐代码与证据切屏

| 切屏 | 只展示的内容 | 证明/解释 |
|---|---|---|
| main_cli.cpp:18、55 | 配置与Runtime调用 | CLI和运行生命周期分离 |
| runtime.cpp:74、201、214 | 等待、clone、放行 | 有PID但负载未执行；成功限额/attach后才G |
| runtime.cpp:94 | private→hostname→chroot/chdir→dev/proc→exec | 宿主不变、子视图改变 |
| full_session.hpp:10、21 | prepare/finish | attach前置，snapshot先于cleanup |
| cgroup.cpp:83、145、162、177 | 创建/attach/快照/清理 | A解释B实现的资源职责和排序 |
| ubuntu-97800f0.md | attach ESRCH、CPU/OOM、中断成员 | 真实同候选12项；测试构建诱发内核拒绝需明确 |
| strace-record.md | 两个PID文件的关键调用 | 本人观察，非新跑的full trace |

## A的短演示命令（正文只选isolate，尚待彩排）

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

预期：容器输出minic-report、PID=1、Rootfs标记，宿主hostname前后相同。明确这是isolate演示，full资源数据随后由A展示已保存的同候选快照，不把此例当full通过。

附录可选失败短例（六分钟正文不执行；仅供问答或以后选择彩排）：

```bash
sudo ./build/debug/minic run --mode isolate \
  --rootfs "$PWD/.local/isolation-rootfs" --hostname minic-report \
  --evidence-dir "$PWD/artifacts/report-missing" -- /not-a-program
printf 'CLI status=%s\n' "$?"
cat artifacts/report-missing/result.json
```

预期127、startup_error/exec/errno2；本命令为演示建议，尚未记录该演示目录的实际结果。现场若失败，保留错误，改展示已入库 [固定候选Ubuntu证据](evidence/A/integration/ubuntu-97800f0.md)，说明是历史实测；不伪装现场成功。资源demo如使用B维护的入口，应由A预先熟悉并独自操作；也可直接展示本稿的原始快照，不现场跑完整12项。

## 本人尚需补充

- 六分钟正文彩排一条短隔离例，失败用已有JSON；保存实际输出/耗时/日期，11-03安排主、备用机演示及真实备用录像。
- A独自彩排全部6分钟与切屏；与B核对资源数字、工具归属和台下备用支持，不分配B台上时间。补齐封面、课程/小组信息和最终演示机配置。
- 材料生成后，先顺着前五段理解启动链，再逐段学习cgroup、CPU统计、OOM和清理；一次讲一小段，不重复GDB/strace或反复答题。已有操作记录不改写成新阶段工具实测。

没有生产级安全承诺，未支持功能不作演示；历史V0 ASan不含LSan，不能说V1“内存无泄漏”。本次材料未新增源码或测试，展示前不要把文档提交SHA写成实际验收SHA。
