# A单人6分钟汇报提纲

2026-11-04由A（yschannel）独自汇报，Ubuntu为主演示环境，B（Blonisco）不上台。项目是两人共同开发：A负责Runtime、隔离/Rootfs、full集成与Ubuntu验证；B负责cgroup、资源负载/探针、构建测试支持、统一验收与openSUSE验证。A讲述B的工作和工具记录时明确归属，不说成独立完成。

本提纲及统一讲稿由A维护。B在PR #7独立维护presentation-B.md和demo.md，提供技术审校与备用支持；不同时修改A文件。资源/工具摘要参考已审查的PR #7提交f2a08e6；原稿的openSUSE三分钟完整验收演示不直接套到本场。B的后续修订待独立审查，不在本分支自动合并。

材料基线main：`25cf83acb7bfe080e12c1eb19d4318721f7fb109`；两机实际被测候选：`97800f0244b807466e7c516021f3c40b882abf45`。两者运行源码/验收脚本相同，纯文档提交不冒充重测。11-02起冻结新增功能。

## 六分钟分配

| 时间 | A讲什么 / 屏幕 |
|---|---|
| 0:00–0:25 | 目标、真实开发分工；一页功能/版本卡 |
| 0:25–1:40 | 完整启动顺序与排序原因；一张父子时序图 |
| 1:40–2:25 | Ubuntu一条短isolate演示：hostname、PID1、proc/Rootfs；明确不是full例 |
| 2:25–3:40 | cgroup与CPU/OOM；展示已保存的Ubuntu同候选快照，说明B的模块贡献 |
| 3:40–4:20 | 失败不放行、错误分类、snapshot→cleanup；展示已有JSON摘录 |
| 4:20–5:15 | A的GDB/strace、B的ASan边界，两机同候选各12项PASS |
| 5:15–5:40 | 已知限制、冻结收口，A结束陈述 |
| 5:40–6:00 | 留作命令输入/切屏缓冲，不再加新内容 |

正文见 [presentation-A.md](presentation-A.md) 的“现场六分钟口播”。其后代码附录只用于准备和问答，台上不逐段读完。不安排B接话，不把六分钟变成压缩念完十段代码。

## 页面与操作安排

1. 目标/分工/两个SHA：A开场明确两人贡献，完整SHA放页脚，不逐字读。
2. [架构与时序](architecture.md)：讲参数→clone先等待→create/attach→G→Rootfs/proc/exec→wait→snapshot→cleanup。只在问答时切源码。
3. 隔离短例：提前准备二进制、Rootfs和sudo凭证，A独自操作Ubuntu终端，预期minic-report、PID=1及Rootfs标记；前后宿主hostname一致。命令见统一讲稿。
4. 资源卡：只展示 [Ubuntu固定候选记录](evidence/A/integration/ubuntu-97800f0.md) 的cpu.max、cpu.stat、memory.events和cleanup。约20.12%来自本轮usage/墙钟；OOM须有oom_kill而非只看137。CPU baseline为isolate，没有基线CPU计数。
5. 失败卡：显示已有exec ENOENT与负载exit127的kind区别；create/测试构建attach失败不放行；中断成员消失与叶子清理。六分钟默认不再运行第二个现场例。
6. 工具与两机卡：A自己的GDB/strace，B的历史V0 ASan（LSan未通过），Ubuntu/openSUSE同候选各12项通过。数字和操作不跨候选拼接。

## Ubuntu演示与备用（彩排尚未完成）

本场不重跑完整12项、不现场跑CTest或安装依赖。`with-delegation.sh --acceptance`是两机已验证的统一验收入口，但它运行12项，作为复验入口保留，不占本场演示时间；从普通用户启动，由脚本内部sudo。不要加一个不存在的通用`-- COMMAND`用法。

保存的Ubuntu原始证据目录为`artifacts/b3/native-20261101-173747`，已提交精选记录如上。提前打开所需JSON/记录页，不在台上查找最新目录；现场输出和历史快照明确区分。sudo/权限或命令异常时保留错误，切备用记录并说“这是先前实测结果”，不无限重试或把录像冒充现场。

待A本人补：六分钟计时彩排与短isolate实际输出/退出码、最终投影片/封面、备用录像与主/备用机安排。B提供资源审校和材料支持，不占台上时间。本稿仍为待彩排建议，不把原生验收等同于彩排。先生成材料，再按代码附录一次学习一小段，不重复GDB/strace，不用答题门禁阻塞工作。
