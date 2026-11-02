# A单人汇报提纲（暂按9分钟）

2026-11-04由A（yschannel）独自完成全部讲解、演示和问答，B（Blonisco）不上台，无接话安排。项目仍由两人开发：A负责Runtime、隔离/Rootfs、full集成和Ubuntu验证；B负责cgroup、资源负载/探针、构建测试支持、统一验收和openSUSE验证。A讲述B的工作时明确归属，不说成独立完成或亲手执行了B的操作。

A维护本提纲、README、架构、台账和单人讲稿；B可在台下提供资源/工具材料、核对技术内容及备用演示支持，不同时修改A维护的文件。2026-11-02起冻结新增功能。本文件是待彩排稿，不把建议命令或学习计划写成实测完成。

版本页固定两行：合并main `25cf83acb7bfe080e12c1eb19d4318721f7fb109`；两机被测实现/验收脚本 `97800f0244b807466e7c516021f3c40b882abf45`。运行源码与测试脚本相同，文档提交不冒充重测。

## A的9分钟顺序

| 时间 | A讲解和展示的内容 |
|---|---|
| 0:00–0:45 | 教学目标、两人真实分工、已实现的v0/isolate/full与安全边界 |
| 0:45–2:45 | 完整父子时序：参数→clone先等待→create/attach→G→Rootfs/proc/exec→wait→snapshot→cleanup，解释先限额后执行 |
| 2:45–3:45 | 隔离短例：hostname、PID1、proc和Rootfs，宿主保持不变；明确示例是isolate |
| 3:45–5:15 | A讲B实现的cgroup：manager/jobs委派、CPU微秒额度与实测节流、内存/swap及OOM事件；展示已保存快照，不现场跑完整12项 |
| 5:15–6:15 | 失败与清理：一次exec失败JSON；create/attach不放行，中断后代回收；snapshot先于删除 |
| 6:15–7:15 | 工具与问题闭环：A既有GDB/strace和/dev/null修复；B的V0 ASan记录，说明LSan失败与覆盖限制 |
| 7:15–8:15 | Ubuntu/openSUSE同候选各12项PASS；attach项为测试构建触发真实内核错误，baseline为isolate，不扩大实测范围 |
| 8:15–9:00 | 已知限制、尚待彩排事项和冻结安排；结束陈述由A完成，不转交他人 |

8分钟时缩短代码切屏和历史过程，不省略资源/OOM/清理与限制。10分钟时增加一个运行期namespace对照解释，不增加工具或功能。

## 页面与证据准备

1. 目标/分工/两个SHA。开场可说：“这是我们两人的项目，我负责启动和隔离，组员B负责资源控制和测试；今天由我统一汇报。”
2. [架构与父子图](architecture.md)。只切三组代码：Runtime/child_entry、FullSession、CgroupSession关键方法。
3. 隔离短演示与 [A-1备用证据](evidence/A/isolation/native-verification.md)。A自己操作终端，不等待B接话。
4. 资源页：A按 [单人讲稿](presentation-A.md) 的资源部分解释cpu.max/cpu.stat/memory.events；展示 [Ubuntu固定候选记录](evidence/A/integration/ubuntu-97800f0.md) 的数值，B可事先核对。不要把不同候选的计数混成同一次运行。
5. 失败与清理页：exec ENOENT、负载自身125/127、测试版attach ESRCH、中断成员消失；错误与返回值一起看。
6. 工具页：A亲手GDB/strace，B亲手ASan/错误例按其记录归属；两机表与限制收尾。ASan旧V0覆盖不等于完整V1或泄漏检测通过。

## A独自演示与准备（待本人彩排）

A用两个终端，最多一条短isolate和一条失败命令，资源使用保存的快照。具体命令见presentation-A.md。已有二进制、Rootfs和负载提前核对，不现场安装、不删除资产、不依赖B上台操作。B可台下提供已验证的材料和备用录像位置，A需事先熟悉内容。

现场失败保留错误，切换同候选实测记录并说明是先前结果；不把录像冒充现场，不把SKIP改PASS。源码/demo脚本变动才复验受影响项；纯文档不重跑验收。

待A补充：实际彩排耗时/输出、11-03主/备用机彩排安排、录像与投影片位置、课程封面信息。待与B对齐：技术审校、资源/工具证据位置、备用支持；不再分配台上时间。材料先完成，随后分段结合代码学习；不要求反复答题或重做GDB/strace才继续。
