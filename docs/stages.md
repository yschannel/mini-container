# 阶段台账



| 阶段 | 执行人 | 必须依赖的已合并基线 | 产出与合并节点 | 能否并行 |
|---|---|---|---|---|
| S00 初始化与接口初稿 | A / Ubuntu | 无 | GitHub 仓库、文档、可构建骨架，记 C00 | 首次串行 |
| S01 克隆与环境验收 | B / openSUSE | C00 | 环境、构建支持、接口审查，合并记 C01 | A 可学习接口，不能先写核心 |
| S02 V0 执行与错误报告 | A / Ubuntu | C01，双方确认接口 | V0 Runtime，合并记 C02 | B 可学习 cgroup 文档，不改共享文件 |
| S03 V0 测试与双系统基线 | B / openSUSE | C02 | 自动测试、ASan、两机 V0 记录，合并记 C03 | A 须先按 B 的用例补 Ubuntu 证据 |
| S04 隔离与 Rootfs | A / Ubuntu | C03 | namespace、Rootfs、同步、清理，合并记 C04 | 与 S05 并行，互不编辑对方模块 |
| S05 cgroup 与测试负载 | B / openSUSE | C03；合并前同步 C04 | cgroup、负载、委派环境脚本，合并记 C05 | 与 S04 并行；约定 S04 先合并 |
| S06 完整启动链集成 | A / Ubuntu | C05，含 C04 | 先限额后执行、快照、回滚，合并记 C06 | 必须等两个模块均已合并 |
| S07 openSUSE 集成与故障验收 | B / openSUSE | C06 | 验收脚本、交叉验证与问题单，合并记 C07 | A 修核心问题，B 修测试问题；修复后重测 |
| S08 Ubuntu 加固与亲自调试 | A / Ubuntu | C07，全部阻断问题已修复 | Ubuntu 全验收、GDB/strace、候选提交 C08 | B 可整理已有证据，不能抢先宣布最终通过 |
| S09 同提交双机复验与交换学习 | B 主执行，A 配合 | C08 | B 全验收、双方交换复现、候选结论，合并记 C09 | 两机可同时测试同一 C08 |
| S10 冻结与汇报材料 | A 主执行，B 审查 | C09，两人确认验收结论 | 文档、演示入口、记录与稳定标签 | 两人并行彩排；合并由 A 收口 |

主线：`S00 → S01 → S02 → S03 → (S04 ∥ S05) → S06 → S07 → S08 → S09 → S10`。

表中 Cxx 是“阶段完成后 main 的真实提交 SHA”，不是分支名，也不是需要创建的 Git 标签。必须填入阶段台账。依赖通过并合并后，接手者才 `git fetch origin`、`git switch main`、`git pull --ff-only`，核对 SHA 后新建阶段分支。不得依据聊天中的“已经好了”跳过同步。已有分支先处理未提交修改，不自动 reset/clean/stash。


## 当前状态

S00：初始提交已由 A 推送。旧报告 `docs/evidence/A/S00/report.md` 是推送前记录，保留原始观察；不让 A 为补填自身 SHA 再提交。C00=`40040ab865ece11b1f7211ca37e01ac4c2979d0f`；本地 `origin/main`、HEAD 与远端 `main` 均已核实一致。

S01：A 已审查并在 GitHub 合并 PR #1；两机构建通过（用户确认）。真实 C01=`22019e8d860464936d21ab3784c6fcf27f070269`，S02 开始时 main HEAD 与 origin/main 一致、工作区干净。此前理解确认仍保留待完成状态；按 A 本次明确授权，结合 S02 调试补齐，不阻塞开发，不代本人签认。

S02：A 在 `a/s02-v0-runtime` 实现 V0；公共头未改动。仅 fork/exec/错误管道/wait/结果输出，不实现 namespace、Rootfs、cgroup。B 的 CMake 源登记、A 本人 GDB 与 B 审查待完成；C02 待合并后填写。S03–S10 NOT_RUN。

仓库 yschannel/mini-container；A yschannel / Ubuntu，B Blonisco / openSUSE。汇报 2026-11-04，功能冻结 2026-11-02。


## 新版并行流程接续（2026-10-31）

以 Git 历史及 V1_ACCELERATED_WORKFLOW.md 为准：S02 已合并，真实 C02=`3d467120786dce63b841820c60dd7e9c4e99eb4e`，A 已确认 Ubuntu 标准构建/CTest 和本人 GDB 完成。上方旧待办文字保留作历史，不再阻塞新阶段。

A-1：a/v1-isolation，从 C02 开发隔离/Rootfs/同步/清理；标准构建与受影响 V0 回归通过，Agent clone EPERM 单列 SKIP；实现 66664782a71de911c9e5cd92e561e4c46e8ad1cb 在 A 普通终端原生 8 项检查通过（其中准备失败为模拟注入），新机制本人 strace 已完成；待最后修复/证据推送与 B 审查。B 独立推进资源模块；不要求固定模块合并顺序，不开始 full 集成。详细交接见 evidence/A/isolation/report.md。
