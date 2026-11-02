# 共同汇报提纲（暂按9分钟）

A维护共同提纲、README、架构和台账；B独立维护资源/工具讲解与演示说明，不同时修改这些文件。演讲日期2026-11-04；2026-11-02起仅收口，不新增功能。本文件是排练稿，未记录一次尚未发生的彩排为完成。

版本页固定两行：合并main `25cf83acb7bfe080e12c1eb19d4318721f7fb109`；两机被测实现/验收脚本 `97800f0244b807466e7c516021f3c40b882abf45`。当前运行源码与测试脚本一致，证据提交不冒充重测。

## 时间与分工

| 时间 | 主讲 | 展示与要点 |
|---|---|---|
| 0:00–0:40 | A | 教学问题：如何把一个宿主进程变成具有隔离视图与资源约束的可信负载；不做Docker替代品 |
| 0:40–1:20 | A/B | 功能表：v0/isolate/full；两机同候选各12项PASS，attach用例为测试构建诱发内核拒绝 |
| 1:20–3:20 | A | 一张父子时序图：clone先等待→create/attach→G→Rootfs/proc/exec→wait→snapshot→cleanup；指出先限额后执行窗口 |
| 3:20–4:20 | B | 资源讲解：CPU微秒额度、内存字节/swap策略、独立manager与委派；细节由B自己的材料承载 |
| 4:20–6:20 | A/B | 最多两个现场短例：隔离hostname/PID/Rootfs；一次失败及JSON。full资源展示由B选既有入口/已有快照，避免现场跑完整12项 |
| 6:20–7:50 | A/B | 证据：A既有GDB/strace与/dev/null真实修复；B资源计数、工具覆盖与系统差异。每人只讲亲手做过的事实 |
| 7:50–8:40 | B/A | Ubuntu/openSUSE结果、故障注入边界、LSan未通过、可信有限负载与非生产安全边界 |
| 8:40–9:00 | A | 当前已完成范围、剩余彩排/材料、11-02冻结；留约1分钟应对切屏或提问 |

8分钟时压缩资源数字与历史过程，不删除失败/清理证据。10分钟时加一个运行期namespace对照解释，不临时增加功能或工具。

## 建议页面与证据

1. 标题/两人角色/两个SHA；完成范围以README为准。
2. 架构图与full时序图：[architecture.md](architecture.md)。A只选三处代码：Runtime::run、isolate_child_entry、FullSession::prepare/finish。
3. 隔离现场输出，备用为 [A-1 namespace对照](evidence/A/isolation/native-verification.md) 和 [本人strace](evidence/A/isolation/strace-record.md)。不要把isolate输出说成full。
4. 资源页由B撰写：CPU quota/period、cpu.stat、memory.events、快照先于删除。Ubuntu原始精选值位于 [固定候选记录](evidence/A/integration/ubuntu-97800f0.md)，openSUSE数值位于 [B记录](evidence/B/B3/report.md) 的复验小节；两机数值不要求完全相等。
5. 失败/清理：create失败不放行、attach测试版本真实ESRCH、中断后成员消失，exit125/127与startup_error区分。
6. 两人调试/修复与限制。A的S02 GDB是旧阶段真实观察，不能说成full GDB；B的历史V0 ASan禁用了LSan，不能说完整V1泄漏检测通过。

## 彩排与演示建议（均待本人执行）

A准备两个终端：一个运行短isolate，另一个对照宿主hostname或展示保存的result.json。命令见 [presentation-A.md](presentation-A.md)。Rootfs/负载和二进制提前准备，不现场安装依赖、不删除既有目录。B选择已实现的委派入口或自己的经验证demo，给出文件位置后共同提纲只引用，不重写B脚本。

现场失败时保留命令与错误，不无限重试、不把SKIP改PASS；切换到已入库的同候选实测记录并明确说“这是先前实测证据”。录像仅为真实备用，不能冒充直播。若演示脚本/源码变动，复验受影响项；本次纯文档不重复整套。

待A/B共同补齐：最终讲者时间分配、B材料路径与选定演示命令、11-03主/备用机彩排时间与真实结果、备用录像位置、是否需要投影片/PDF和学校要求的封面信息。工具/学习尚缺本人记录的部分由本人补充，不代签，不阻塞其他材料。
