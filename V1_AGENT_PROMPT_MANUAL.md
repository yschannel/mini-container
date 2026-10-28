# V1 最小可验收容器 分阶段 Agent 提示词手册

本手册只规定之后如何开发与协作，不代表其中命令已经执行、仓库已经创建或功能已经实现。当前工作仅为编写手册。执行时，A 是使用 Ubuntu 的你，B 是使用 openSUSE 的组员；两人分别使用自己的 Agent，通过 GitHub 交换代码与证据。

沿用 DEVELOPMENT_PLAN.md 的 C++17/CMake 与 V1 目标。本手册将身份固定，并把分工细化：A 负责仓库初始化、公共接口初稿、核心执行链；B 接手构建维护、测试、cgroup 与交叉验证。仅做前台非交互容器，不实现 C/S CLI/daemon、UDS、网络、Seccomp、SSH、TTY、镜像管理。下文的 `minic run` 只是 V1 的本地命令入口，不是客户端/守护进程架构。

## 一 阶段总表与执行顺序

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

### 1. 占位符填写时点

| 占位符 | 何时填写 | 含义 |
|---|---|---|
| `<GITHUB_OWNER>`、`<REPO_NAME>`、`<VISIBILITY>` | A 执行 S00 前 | 仓库所属账号/组织、名称、public 或 private；不得替人猜测 |
| `<A_GITHUB_ID>`、`<B_GITHUB_ID>` | S00 前 | 两人账号；邀请协作者需明确目标 |
| `<LOCAL_PARENT_A>`、`<LOCAL_PARENT_B>` | S00/S01 前 | 各自 Linux 上用于存放项目的现存目录 |
| `<REPO_URL>` | S00 创建成功后 | GitHub 返回的真实 URL；SSH/HTTPS 按实际认证选择 |
| `<C00>` 至 `<C09>` | 每阶段合并后 | main 的完整 SHA，写入台账，交接时复制到下一提示词 |
| `<REPORT_DATE>` | S00 时由组员填写 | 具体汇报日；由此确定提前两天的功能冻结点 |
| `<RUN_ROOTFS>`、`<CGROUP_BASE>` | S04/S05 实测后 | 本机实际路径，不跨机器照抄；由配置/脚本读取 |

尖括号内容是占位符，不能原样交给 shell 执行。提示词内也出现预期构建目录、脚本和接口名称；它们是待实现的契约，不应冒充已有文件。各 Agent 先检查实际文件，若前置文件缺失，报告受阻，不自行补写上游阶段。

第一次在 Ubuntu 执行 S00 前，A 将本手册和 DEVELOPMENT_PLAN.md 复制到准备初始化的项目工作目录，供 Agent 实际读取。B 从仓库获得它们。不要要求尚未创建的仓库提供这些输入文件。

### 2. 时间预算与收缩规则

默认每人每天约 3–4 小时；第一天完成 S00–S03，第二、三天推进 S04–S06，第四天起 S07–S09。具体按汇报日倒排，不保证一天内完成所有问题。

- 必须完成：V0、三个 namespace、专用 Rootfs/proc、CPU/内存限制、错误分类、正常/失败/中断清理、真实 Linux 验收、两人调试记录。
- 可以简化：使用 chroot；静态 BusyBox 优先；只跑可信非交互负载；测试采用轻量 CTest，不强制下载 GTest；stdout/stderr 继承终端；只做约定的有限工作负载。
- 可以延后：pivot_root、init 包装进程、复杂信号转发、动态 Rootfs 自动打包、多架构通用安装器、Perf/FlameGraph、V2/V3 的所有功能。
- 第二天尚未跑通隔离：暂停漂亮日志、更多测试框架和自动安装器，集中修复主链。cgroup 单模块与核心仍按冻结接口推进。
- 汇报前两天停止添加功能，只修阻断问题、复验与做材料。不能通过删掉 CPU/内存限额、跳过 openSUSE 或隐藏失败来宣称完整 V1。
- 进度不足时明确展示“V1 部分完成”，列出通过/未通过/未执行。只达到 Ubuntu 全验收时可称“Ubuntu V1 通过，openSUSE 未通过/受阻”，不可称“双系统 V1 通过”。

## 二 所有阶段共同遵循的约定

以下内容由 S00 写入仓库 `docs/contracts.md`、`docs/workflow.md` 和 `docs/stages.md`，后续提示词都要求重新读取；不依赖 Agent 记忆。若它们与本手册出现不一致，先报告差异，由 A/B 明确批准契约变更后再继续，不能各自解释。

### 1. 简单 GitHub 流程

1. main 保持可构建；每阶段一个短分支，如 `a/s02-v0-runtime`、`b/s05-cgroup`。阶段内小提交，避免一次覆盖全仓库。
2. Agent 只在当前授权阶段工作。编写代码、运行已授权本地测试、提交当前阶段分支并推送 PR；不替另一人点击审查，不擅自合并。若认证、权限或工具不足，给出待执行命令，不冒充已推送。
3. 另一人审查改动范围、接口、实际测试证据与失败路径；发现问题回到原分支修复。无分支保护也按此规则执行。
4. 初次 S00 可由 A 直接建立 main 初始提交；之后由负责人在另一人审查通过后合并 PR。合并可通过 GitHub 网页完成，不强制安装 gh。
5. 合并后登记 main SHA、PR、作者、审查人、测试代码 SHA；另一人再拉取。测试了旧代码后又修改实现，必须重跑受影响测试。纯证据/文档提交也需说明所记录的被测代码 SHA。
6. 并行 S04/S05 从同一 C03 出发。A 的 S04 先合并；B 将最新 main 合入自己的分支，解决自己范围内冲突并重测后再提合并。不得 force-push 共享 main。
7. 共享接口变更：先提交简短变更说明，写调用方影响、原因、迁移和重测项；双方认可后由 A 维护公共头文件，先合并接口与必要编译适配，再恢复并行。B 不悄悄改参数签名，A 不改写 B 的 cgroup 实现。

为避免台账自引用：一个提交不能在自己的文件中预先记录自己的最终 SHA。阶段结束先在交接报告/PR 中记录合并后的真实 Cxx，下一阶段再把它写入台账；不要为“补写本提交 SHA”不断产生新提交。代码测试 SHA、后续证据/文档提交 SHA 分开记录。B 提供 A 阶段所需的构建登记时，可以在 A 阶段分支提交仅涉及 B 主责文件的小提交，由 A 拉取后继续；不必等 S05 完成，也不让 A 绕过目录所有权。交接前确认双方没有同时写同一文件。

### 2. 目录所有权

| 主责 | 文件/目录 | 规则 |
|---|---|---|
| A | `include/minic/{config,runtime,cgroup,result}.hpp`、`src/main_cli.cpp`、`src/core/{runtime,namespaces,rootfs}.cpp`、`src/common/` | A 在 S00 建公共头；B 审查；公共头变更需双方确认 |
| B | `src/core/cgroup.cpp`、`tests/`、`scripts/check-env.sh`、`scripts/prepare-cgroup-env.sh`、`scripts/with-delegation.sh`、`scripts/build-rootfs-workloads.sh` | S05 不提前接入 Runtime |
| B（S00 后接手） | `CMakeLists.txt`、`cmake/`、`CMakePresets.json` | A 需要加源文件时向 B 给出最小构建变更清单，B 提交；不要求先有 Presets |
| A | `scripts/prepare-rootfs.sh`、`docs/architecture.md`、最终 README | Rootfs 脚本必须保护目标路径 |
| B | `scripts/run-stage.sh`、`scripts/run-acceptance.sh`、`scripts/demo.sh`、`docs/compatibility.md` | shell 脚本的输出/返回码遵循统一约定 |
| 各自 | `docs/evidence/A/`、`docs/evidence/B/`、各自调试文档 | 不覆盖对方原始记录 |
| 串行编辑 | `docs/contracts.md`、`docs/workflow.md`、`docs/stages.md` | 阶段负责人更新，另一人暂不编辑同文件 |

### 3. 首版公共接口语义

S00 创建可编译声明与文档，先固定以下语义及实际 C++ 签名，再开始实现；这里只定义语义，不提供代码实现。避免为占位而返回成功。

- `RunConfig`：阶段模式 `v0/isolate/full`、Rootfs 绝对路径、hostname、argv、可选内存字节数、swap 字节数、CPU quota/period（微秒）、cgroup 委派基目录、证据目录。`v0/isolate` 仅供阶段测试，运行时显式打印“非完整 V1”；默认最终验收使用 `full`。不得在 full 失败后自动降级。
- `RunResult`：`kind = exited/signaled/startup_error/cleanup_error`，独立保存退出值或信号、失败步骤、errno/消息、宿主 PID、run_id、实际启用的隔离/限额、清理结果。保留主失败和清理失败两类信息，不互相覆盖。运行摘要写 `result.json`，日志写 stderr。
- CLI：`minic run --mode v0|isolate|full --rootfs PATH --hostname NAME --cgroup-base PATH [--memory BYTES] [--swap-max BYTES] [--cpu-quota US --cpu-period US] --evidence-dir PATH -- PROGRAM ARGS...`。模式不需要的参数可省略；full 要求本轮指定的限额全部成功。`--cpu-period` 默认 100000 微秒，quota 必须为正；full 模式由验收配置明确给出内存和 CPU，不依赖隐含默认值。
- Runtime 对外 `run(config) -> RunResult`；负责验证、生成唯一 run_id、clone、同步、调用 cgroup、错误通信、等待、快照与回滚。非交互负载继承 stdout/stderr，stdin 连接 `/dev/null`；不实现终端代理。
- cgroup 抽象由 A 声明，B 实现：`create(base, run_id, limits)`、`attach(pid)`、`snapshot()`、`cleanup()` 的真实签名及错误类型写入契约；父进程唯一持有 move-only 会话。析构只作不抛异常兜底，显式 cleanup 必须返回可报告结果。模块不自行创建进程、不解析 CLI、不修改系统根层级。
- 错误与退出码：正常负载退出沿用 0–255；信号退出用 128+signal；运行器启动/配置失败用 125，exec 路径/格式等失败用 126/127（具体分类先写表），清理失败在负载成功时用 125。负载本身也可能返回这些值，因此必须结合 `result.kind` 和步骤判断，不能只看 shell `$?`。信号与清理失败同时存在时保留原终止原因并报告 cleanup 失败，验收仍未通过。
- exec 错误管道写端使用 close-on-exec；结构化错误记录有步骤和 errno。EOF 只是一个信号，必须结合子进程终止状态判定，不能把“早死但未写错误”自动算 exec 成功。检查短读写、EINTR、意外 EOF；控制端消失时子进程不执行工作负载。

### 4. 资源所有权与 V1 边界

- 父进程：clone 栈直到子进程回收；管道相应端；cgroup 会话；唯一 run_id；退出摘要。子进程：自己的管道端、namespace 内挂载与 cwd。所有 fd 有 RAII，exec 前关闭不需要的 fd。
- `clone` 采用独立回调栈，不用 CLONE_VM/CLONE_FILES/CLONE_THREAD；PID/UTS/Mount 模式必须在真实 Linux 运行，先将 mount propagation 设为 private，再修改挂载。父进程的 host namespace 不得被改变。
- 子进程等待放行后才配置 Rootfs 和 exec。full 模式父进程必须先创建、设置并加入 cgroup，再放行；失败终止并 wait，不能先运行再补限额。
- Rootfs 使用专用工作目录；chroot 后 chdir 到 `/`，在新 PID namespace 内挂载 proc。运行前关闭不需要的宿主目录 fd，不提供额外宿主目录绑定。只保证约定演示，不能宣称防逃逸。
- V1 允许负载直接为 namespace PID 1，不做 init 包装。只运行可信、有限、非守护化测试；容器后代处理依赖 PID namespace 生命周期，必须测试后代随 PID 1 终止被清除。PID 1 不保证响应普通 SIGTERM。CLI 收到 SIGINT/SIGTERM 时在正常控制流内终止并回收；可对自己的 namespace init 使用 SIGKILL 作为有界兜底，不能在信号处理器里执行复杂清理。
- 覆盖正常退出、准备失败、exec 失败、限额失败、CLI 中断；不承诺运行器被 SIGKILL、机器掉电后的自动恢复。记录手工诊断/清理方法，只操作确认属于本项目的资源。
- cgroup 使用 systemd 委派子树。管理进程在独立 manager 叶子，空父节点启用 cpu/memory，负载在各自叶子；先确认控制器沿父级可用。不将“有 root 权限”视为所有控制器都可用。
- 在清除 cgroup 前保存 memory.events、memory.current、cpu.stat、cpu.max、memory.max、可用时的 memory.swap.max 与 cgroup.events；OOM 已发生后再检查被删除路径是不成立的。snapshot 失败也要报告。

### 5. 可重复命令与证据契约

S00 生成文档，S01/S03/S04/S05/S07 逐步实现下列入口。未实现的入口不能在记录中写为已执行。

```bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j2
ctest --test-dir build/debug --output-on-failure
./scripts/check-env.sh --output artifacts/env.json
./scripts/run-stage.sh v0 --output artifacts/v0
./scripts/prepare-rootfs.sh --output "$PWD/.local/rootfs"
./scripts/build-rootfs-workloads.sh --rootfs "$PWD/.local/rootfs"
sudo ./scripts/with-delegation.sh --config .local/machine.env -- \
  ./scripts/run-stage.sh isolate --output artifacts/isolate
sudo ./scripts/with-delegation.sh --config .local/machine.env -- \
  ./scripts/run-stage.sh cgroup --output artifacts/cgroup
sudo ./scripts/with-delegation.sh --config .local/machine.env -- \
  ./scripts/run-acceptance.sh --output artifacts/v1
```

脚本位置以仓库根目录为 cwd；`sudo` 仅用于确需特权的 namespace/cgroup 部分。配置 `.local/machine.env` 由本人审核后建立，不提交。with-delegation 的职责：验证/建立一个本次运行独占的 systemd 委派环境、确定实际 cgroup 路径与 manager 分支、将路径通过环境传给测试、传播原命令退出状态、收尾。不是任意读取路径后执行的 shell 注入入口。具体 systemd 命令由 S05 根据实机版本验证并写文档，不能编造所有版本通用的一行命令。

S04 尚未等到 S05 时，隔离阶段直接用 `sudo ./scripts/run-stage.sh isolate ...`；isolate 模式不要求 cgroup。S05 合并后统一用委派入口运行特权验收。

每个测试输出：CASE_ID、命令/参数、时间、系统、被测完整 SHA、预期、实际、退出状态、证据路径，单项状态严格为 `PASS/FAIL/SKIP/NOT_RUN`。FAIL 是实际执行不符合预期；SKIP 是明确检查到前提不足；NOT_RUN 是没运行。只有全部必需项 PASS 才能宣称该范围验收通过；CTest 没发现测试或全跳过不等于通过。runner 返回码区分 0 全通过、1 有 FAIL、2 无 FAIL 但有必需项 SKIP/NOT_RUN。

原始日志写 artifacts/，不提交大量构建产物、Rootfs 或主机敏感信息。精选原始片段和结果清单写 `docs/evidence/<A或B>/<阶段>/`，保留被测 SHA 与环境，检查敏感路径、账号和令牌后再提交。屏幕截图不能代替命令和返回值；记录不得由 Agent 伪造。

统一阶段报告（每条提示词要求输出）：

```text
阶段：Sxx / 执行人 A 或 B / 系统与版本
状态：通过 | 未通过 | 受阻
已合并前置 SHA：…；本次被测 SHA：…；工作区是否干净：…
实际修改文件与公共接口变更：…
逐项验证：CASE_ID / 命令 / 预期 / 实际 / PASS|FAIL|SKIP|NOT_RUN / 证据
人工检查点：已亲自完成 | 待本人完成；记录位置：…
失败或阻碍：原因、范围、修复步骤、由谁处理
Git：分支、提交 SHA、PR、审查/合并状态、main 合并后 SHA（未知就写待填）
交接：谁在何时拉取哪个真实 SHA
下一阶段：可以开始哪些阶段 | 不可开始及原因
```

“通过”同时要求本阶段必需技术验收 PASS、人工检查完成；尚待审查合并时注明“技术通过，未合并，依赖阶段不得开始”。Agent 无法替本人确认“亲自完成”；等待实际反馈期间为受阻。实现缺陷为未通过；权限、环境、上游未交接或人工检查未完成为受阻。修复后在原阶段复验，不生成新的“通过”掩盖旧记录。

## 三 A 可复制的提示词 按阶段取用

复制前填写本阶段占位符。每次新聊天也完整复制对应代码块；其中要求读取仓库契约与台账，能恢复上下文。

### S00 A Ubuntu 环境检查 创建仓库与骨架

```text
你正在协助 A（我），本机应为 Ubuntu。项目是 Linux 课程 Mini-Container，C++17/CMake，目标为 V1 教学型前台非交互容器。本次执行 S00：初始化仓库、可构建骨架和公共契约，不实现容器功能。

参数：GITHUB_OWNER=<GITHUB_OWNER>；REPO_NAME=<REPO_NAME>；VISIBILITY=<VISIBILITY>；A=<A_GITHUB_ID>；B=<B_GITHUB_ID>；本地父目录=<LOCAL_PARENT_A>；汇报日=<REPORT_DATE>。任何占位符未填写时，先列出缺项；不要猜测仓库可见性或账号，不创建错误仓库。认证不输出 token。

先读取我提供的 DEVELOPMENT_PLAN.md、V1_AGENT_PROMPT_MANUAL.md、现存 AGENTS.md；列出现存目录与 git status，确认是否已有仓库和 remote，不能覆盖现有文件、初始化嵌套仓库或重写历史。将两份方案文档保留进仓库。发现不是 Ubuntu/不是 Linux 时只做文档与诊断，特权验收受阻。

先做 Ubuntu 环境检查：os-release、uname -r/-m、虚拟化信息、id、git、编译器、CMake、GDB、strace、file/readelf、BusyBox、cgroup2 挂载和 cpu/memory 控制器、systemd 版本。根据实际版本用 apt 查询候选包与已安装状态，给出准确依赖安装命令；仅安装确有需要的开发工具，不升级发行版、修改启动参数或关闭安全策略。系统安装需本机权限时明确说明；无法执行则记录缺项，不声称准备完成。检查 BusyBox 架构、ELF interpreter/NEEDED，不假定包为静态。

本阶段允许创建项目骨架、README、.gitignore、CMakeLists、include/minic 公共头、docs/contracts.md、docs/workflow.md、docs/stages.md、docs/architecture.md、A 的环境记录；main_cli 只提供 --help/--version，Runtime/cgroup 只有冻结声明。不能写假成功 stub；不能做 daemon、网络、Seccomp。S00 完成后 CMake 维护权交给 B。

严格把手册“共同约定”落入仓库文档：RunConfig/RunResult、Runtime/cgroup 的精确 C++ 签名与所有权、单位、错误和退出码、证据 JSON 字段、资源清理职责、v0/isolate/full 模式边界、源码目录所有权、Git 流程、测试入口和统一报告。cgroup 会话 move-only，父进程显式清理，析构不抛异常。可预建计划中core的空翻译单元并登记构建，空文件不冒充功能实现；main不调用尚未实现符号。不自动下载第三方依赖。公共头能够被一个最小编译检查包含。

验证：cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug；cmake --build build/debug -j2；./build/debug/minic --help 与 --version 均返回 0，帮助说明仅骨架。保存命令、返回值与真实输出。尚未建立测试时 ctest 不能记为测试通过。

确认参数和本地安全后，使用已有 GitHub 认证创建指定仓库、提交并推送初始 main；若无工具可用，给出 GitHub 网页创建空仓库、git remote/push 的精确待办，等我实际执行，不伪报成功。不要向第三方发送消息。B 的协作者邀请由我在 GitHub 按已填写账号处理，记录是否完成；不把仓库地址或本地提交误写成远端成功。

人工检查：让我亲自执行一次构建和 --help、查看 git log/git remote，确认 GitHub 能看到文件与 B 的权限；解释三个模式、RunResult 与 shell 退出码的区别、谁拥有 cgroup。要求我实际确认并记录，不代替我完成。S00 验收：本机真实构建和两个命令符合预期、远端可见、B 有访问方式、接口初稿齐全；缺任一项不进入 S01。

完成后按手册统一阶段报告输出：通过/未通过/受阻、实际测试与 FAIL/SKIP/NOT_RUN、修改清单、环境和人工证据、初始 main 完整 SHA=C00、真实 REPO_URL、认证/权限待办及下一步。将台账中的未知项明确保留；通知我把 REPO_URL、C00 和本手册交给 B 的 Agent，禁止自行执行 S01。
```

### S02 A V0 核心执行 错误通信与等待

```text
你协助 A，在 Ubuntu 开发 Mini-Container C++17。当前仅实现 V0 基础执行，本阶段 S02。前置是 B 的 S01 已审查合并：C01=<C01>。不要依赖旧聊天。

先查看系统、git status/remote/log，读取 AGENTS.md、DEVELOPMENT_PLAN.md、V1_AGENT_PROMPT_MANUAL.md、docs/contracts.md、workflow.md、stages.md、实际公共头/CMake/B 的环境与接口审查。工作区干净后拉取 main，验证 HEAD 是 C01 或已确认包含 C01 的后续主线；台账 S01 与人工检查未通过、接口未被双方确认、构建失败时受阻，不补写 B 的任务。建立 a/s02-v0-runtime。

只修改 src/main_cli.cpp、src/core/runtime.cpp、src/common/、A 的记录和阶段台账；公共头须走双方认可的变更流程。需要构建源文件登记时将最小清单交 B，等其提交进入基线，不自行改 B 主责文件。不修改 cgroup/tests。目标是 --mode v0：解析和校验 argv，创建子进程，exec 指定宿主程序、等待并生成契约 RunResult/result.json。先不创建 namespace、Rootfs 或 cgroup；输出明确“V0 非隔离运行”。用 fork 实现 V0，后续隔离模式才采用 clone 回调。

要求所有 syscall 检查、errno 当场保存、fd RAII、错误管道 CLOEXEC，处理 EINTR/短读写/意外 EOF，子分支失败使用正确退出方式，避免执行父进程清理；不继承不必要 fd。保存 pid、run_id 和失败步骤，按契约区分启动错误与负载非零返回。CLI stdout/stderr 继承，stdin 接 /dev/null。用户参数不得拼成 shell 字符串执行。实现 SIGINT/SIGTERM 的安全通知机制，清理在正常控制流中完成；不添加复杂进程管理功能。

构建运行：cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug；cmake --build build/debug -j2。分别用 minic run --mode v0 --evidence-dir artifacts/s02/<case> -- 执行 /bin/true、/bin/sh -c 'exit 7'、/definitely-not-exist。期望：0/exited、7/exited、127/startup_error 且步骤 exec、errno 为 ENOENT；每例保存真实 shell 状态与 result.json，两者应一致。用带空格参数验证 argv 未被二次 shell 解析；无参数/非法模式应在创建子进程前报错。不同 case 使用独立证据目录。权限拒绝的程序应与不存在程序区分，分类遵循契约。

人工检查：让我亲手在 Debug 构建上启动 gdb，break 到 V0 子进程执行点；fork 路径可 set follow-fork-mode child，检查 info inferiors、argv、exec 前调用栈，保存观察。让我解释 fork/exec/wait 分别做什么、CLOEXEC 的作用、为什么 EOF 不能单独证明成功。本人未实际运行时不要记录为完成。

验收：上述命令结果准确、无明显残留直接子进程、人工检查完成；没有测试框架不能声称 ctest 已验证。失败先修 S02 原分支并重跑，不能让 B 基于失败代码做 S03。输出统一阶段报告：状态、命令/预期/实际及 PASS/FAIL/SKIP/NOT_RUN、证据与人工记录、改动和接口、分支/被测 SHA/提交/PR。提交推送交 B 审查，不代其批准；合并后填 C02，B 拉取 C02 才能进入 S03。
```

### S04 A Namespaces Rootfs 与隔离错误路径

```text
你协助 A，Ubuntu，C++17 Mini-Container。当前 S04，目标 V1 隔离子集，尚未接入 cgroup，必须显式用 --mode isolate。前置 S03 已通过合并 C03=<C03>。本阶段可与 B 的 S05 并行，只共享已冻结接口。

不要依赖聊天记忆。读取 AGENTS.md、两份方案、docs/contracts.md/workflow.md/stages.md/architecture.md、公共头、Runtime、CLI、V0 测试与两机记录。检查真实系统、干净工作区、main 含 C03、本机 V0 回归通过；新分支 a/s04-isolation。接口缺失或前置未通过就受阻。只改 A 主责 core/runtime/namespaces/rootfs、main_cli、common、prepare-rootfs.sh、A 证据和架构；测试需求交 B 添加 run-stage isolate，构建登记同样交 B；不编辑 cgroup.cpp 或 B 文件。

实现 clone 回调独立栈，CLONE_NEWPID|CLONE_NEWUTS|CLONE_NEWNS|SIGCHLD，不共享 VM/文件表。栈保持到回收；父子控制管道和独立错误管道，子进程先等待父进程明确放行。isolate 模式没有 cgroup，也必须显示该限制。子分支先设 mount 传播 private，再设 hostname、配置 Rootfs、chroot 后 chdir /、在新 PID namespace 挂载 proc，最后 exec。检查每一步。关闭宿主目录 fd 和无用管道；错误管道结构/错误码与契约一致。没有控制字节或父端意外消失就退出，不执行负载。

prepare-rootfs.sh 只操作显式传入的专用目录：解析绝对路径，拒绝 /、宿主系统目录和危险软链接目标，不递归删除未知现存目录；列出将写入内容。先验证本机 BusyBox 架构及静态/动态链接，再安装所需 applet。优先静态 BusyBox；若动态，明确实际 ELF loader/NEEDED 及复制方案，不能只复制 busybox 后宣称可运行。创建 /proc 和可识别的 rootfs 标记。禁止为方便自动绑定宿主 /、/proc 或泄露宿主目录。Rootfs 原始资产不进 Git，只提交可重复准备说明/脚本。

验证先构建并跑 V0，再执行 ./scripts/prepare-rootfs.sh --output "$PWD/.local/rootfs"。若 B 的 run-stage isolate 尚未就绪，用 README 记下等价的逐条 minic 命令：sudo ./build/debug/minic run --mode isolate --rootfs "$PWD/.local/rootfs" --hostname minic-s04 --evidence-dir artifacts/s04/<case> -- /bin/sh -c 'hostname; echo $$; /bin/ps; /bin/ls /'。预先记录宿主 hostname 和一个宿主标记路径；容器 hostname 应为指定值，宿主不变；shell PID 为 1（以实际负载执行策略解释），ps 不显示宿主普通进程；ls 显示 Rootfs 标记而宿主标记不可见。不能要求 ps 只有一行。记录宿主 PID 与 /proc/<pid>/ns 链接同宿主的差异，观察需在任务存活期间进行。

负例：不存在的 Rootfs 应在危险操作前拒绝；专用有效 Rootfs 中缺少程序应为 exec 启动错误，不能误判为文件不存在的 Rootfs；重复运行检查宿主 hostname/mountinfo 不变。不同 mount namespace 内看不到宿主挂载，不代表 namespace 内根挂载已经正确，需两类证据。CLONE/权限不足写清 syscall/errno 并受阻，不能退回 v0 后通过。

人工检查：让我亲自运行 GDB，在 child_entry、放行后与 exec 前断点，检查回调参数/栈和控制流程；clone 路径不保证 follow-fork-mode 有效，观察实际 inferiors 必要时用可控调试等待方式。再亲手 strace -ff -o artifacts/s04/trace 运行一条短命令，解释 clone、mount、chroot、execve、wait 的顺序。记录至少一个真实问题闭环；没有遇到 bug 就如实记录一次机制验证，不编造修复。

验收：隔离证据、错误路径、V0 回归、重复运行均通过，人工记录完成；rootfs 不能运行、host 被改变、挂载失败被忽略均为未通过。问题在原分支修复复测。按统一报告输出通过/未通过/受阻、逐项状态、命令/输出/证据、被测 SHA、接口与提交/PR、人工进度。B 审查后 A 合并为 C04；把真实 C04 给 B，B 在 S05 合并前同步它。不得启动 S06，须等 S05 也合并。
```

### S06 A 接通限额 同步 快照与回滚

```text
你协助 A，Ubuntu，Mini-Container V1。当前 S06，前置 S04/S05 均通过合并，主线 C05=<C05>，其中已包含 C04。请从仓库恢复上下文，不假定记得之前聊天。

读取 AGENTS.md、方案/手册、docs/contracts/workflow/stages/architecture、实际公共头、runtime/namespaces/rootfs/cgroup、B 的 cgroup 探针与委派说明、全部阶段结果。核对 main SHA 与台账、干净工作区和构建；确认本机委派能力与 B 的模块接口一致，建立 a/s06-full-runtime。若 cgroup 探针本机未跑，先真实运行；权限/控制器缺失为受阻，不能实现后自动关闭限额。

只修改 A 主责 Runtime/CLI/common、架构和 A 证据；不改 B 的 cgroup 实现/测试/构建，共享签名按变更流程处理。B 同步补充契约测试和构建变更后再继续集成。目标 --mode full：验证完整配置→创建子进程阻塞→创建并设置专用 cgroup→attach 子 PID→显式放行→子进程配置隔离并 exec→wait→在删除前 snapshot→cleanup→result.json。父进程为资源唯一所有者。设置/attach 任一步失败都不能放行，须终止并 wait。析构仅兜底，显式清理错误必须向上传递。

检查正常/exec 失败/Rootfs 准备失败/cgroup 失败/父管道关闭/SIGINT/SIGTERM：关闭对应 fd、回收子进程和 clone 栈、保存快照、仅删除本次 cgroup。OOM 的 memory.events 在 cleanup 前保存；主错误和清理错误同时记录。处理 PID 1 信号特殊性；在正常事件处理流内实施对自己子进程的有界终止，必要时 SIGKILL；不在信号处理器内调用日志/分配/复杂清理。V1 不做 init/任意守护进程管理，也不声明对自身 SIGKILL 后的自动恢复。

构建并跑 ctest/V0/isolate，使用本机 .local/machine.env 与 sudo ./scripts/with-delegation.sh --config .local/machine.env -- ./scripts/run-stage.sh cgroup --output artifacts/s06/probe。让 B 提供 full 的最小验收入口，或先用完全记录的等价 minic 命令：64 MiB memory.max、swap-max=0（实机支持并验证）、cpu quota=20000/period=100000 us；执行短成功程序和受控内存/CPU 负载。不得用 ASan 二进制做紧内存限额演示。

观察：运行期 cgroup.procs 包含实际宿主 child PID；负载读取/proc/self/cgroup 的归属与记录一致；result.json 列出真实启用的限额和 snapshot。加入 cgroup 失败时使用会创建外部标记的可信负载，标记绝不出现，证明未先运行。测试目录/标记必须为本次独占。注入用受控假实现或仅测试构建的 fault seam，不向正常 CLI 加危险任意故障命令；B 在 S07 接手系统性故障用例。

人工检查：我亲自断在父进程 attach 前、放行前和 child exec 前，检查 child 状态、cgroup.procs/限制文件与运行期 PID；用一次 strace 对照“attach→放行→exec”的证据，不能仅以日志书写顺序证明同步。让我解释如果早放行会发生什么、为何先 snapshot 后 cleanup、父进程中断时谁做 wait。观察结果写 A 调试文档。

验收：两模块探针和完整短流程通过，限制参数确实生效，错误时负载不运行，快照/清理证据存在，回归与人工检查完成。任一必需项 FAIL/SKIP/NOT_RUN 不算通过；缺依赖则受阻，修复仍在 S06。按统一模板报告状态、逐项命令/预期/实际/证据、被测 SHA、修改和接口、人工记录、提交/PR与限制。B 审查后合并记 C06，B 拉取 C06 开始 S07；不直接发布 V1。
```

### S08 A Ubuntu 完整复验 加固与亲自调试

```text
你协助 A，Ubuntu，Mini-Container V1，阶段 S08。前置 S07 的脚本和所有阻断修复均已合并 C07=<C07>。不得依靠聊天记忆；先读 AGENTS.md、方案/手册、contracts/workflow/stages、架构、实际实现、S07 故障报告和所有测试脚本。

核对系统、工作区、main SHA、前置状态与本机 Rootfs/委派配置。新建 a/s08-ubuntu-hardening，仅改 A 所有的核心缺陷、架构与 A 证据；测试/构建/cgroup 问题交 B 修并合并后同步。接口变更须双方审批。不要为迎合测试改预期，不加 daemon/网络/Seccomp 或额外安全功能。

运行 Debug 构建、ctest、独立 ASan 构建中非特权测试、环境检查、V0 和 isolate 回归，再运行 sudo ./scripts/with-delegation.sh --config .local/machine.env -- ./scripts/run-acceptance.sh --output artifacts/s08/v1。先读脚本确认没有清空防火墙、修改系统根 cgroup 或删除非项目路径。全程保存 shell 返回码、summary、result.json、限制快照与运行期 PID/ns/挂载证据。

验收包括成功/非零/exec失败、三个 namespace、Rootfs/proc、内存超限 OOM 事件、CPU 限额与节流、限额/attach 失败时无负载标记、管道断开、SIGINT/SIGTERM、后代终止、三轮重复运行与资源清理。动态 PID/路径按 run_id 比对，不能写固定 PID。不得把 OOM 的任意 SIGKILL当作 OOM 证据，必须关联该 cgroup 的 oom_kill 增量。所有未跑/跳过/失败分开报告。

我必须亲手做三项：1 用 GDB 在 child_entry/放行/exec 前断点检查一次实际隔离运行；2 用 strace -ff 追踪一次正常与一次失败启动，解释调用、errno、wait 与清理；3 按 B 文档亲自运行 CPU/内存限额用例，读懂 cpu.stat 和 memory.events。给我可复制的本机命令和待观察位置，允许长命令拆成短步，不能由 Agent 的执行替代人工记录。ASan 仅由非特权构建证明内存访问检查范围，不覆盖整个内核资源清理。

人工应解释：PID 1 为什么特殊；/proc 为何在容器内重挂；shell退出值为什么不能单独判断 startup_error；错误管道读到 EOF 如何结合 wait 结果解释；资源快照为什么在删除前。记录本人反馈，若不会解释就补短学习与再次观察，不编造通过。

若核心有修改，先提交并复跑受影响集及完整验收；候选代码必须是干净、可识别的提交，不把脏工作区测试当成提交的结果。按统一报告输出状态、所有 case 实际结果与证据、人工完成项、缺陷修复、被测 SHA、PR/审查/合并状态。B 审查后合并记 C08（候选）。B 拉取准确 C08 做 S09；A 同时从 C08 复验，若后续改代码则候选作废，两机重测新 SHA。
```

### S10 A 功能冻结 发布记录与汇报材料

```text
你协助 A，Ubuntu，Mini-Container V1，阶段 S10，只做收口。前置 C09=<C09> 已合并，里面记录 A/B 对同一候选代码的真实测试、人工交换复现和审查。读取 AGENTS.md、方案/手册、contracts/workflow/stages、实现、两机证据、compatibility 和未解决问题；核对 main 干净且含 C09，不凭旧聊天认定通过。

允许修改 README、docs/architecture.md、阶段/汇报文档和 A 证据；演示脚本由 B 主责提供。原则上不改源码或接口、不加工具。如果发现阻断缺陷，回到对应所有者修复，再对新候选两机重测 S08/S09；不要靠文档宣布已解决。汇报前两天冻结功能，未完成项明确保留。

整理架构图与父子时序图；写清 v0/isolate/full 与已验证功能、真实限制、退出码/JSON、Rootfs 与委派准备、构建/测试/演示命令。引用实际被测 SHA 与证据，区分两机通过、受阻和未测。制作约8–10分钟的 Markdown 汇报提纲：目标1分钟、架构2分钟、演示3分钟、两人调试2分钟、差异/计划1分钟。A讲核心启动/隔离，B讲资源/工具，但双方准备完整问答。

与 B 一起审阅 scripts/demo.sh：从已构建资产执行少量确定性用例，默认不临时联网安装；每例显示预期、实际、run_id、退出状态和关键快照，错误立即停止并指向日志。准备环境失效时可用的真实录屏，不能把录屏当现场实时成功。记录完整构建、CTest、demo 的真实结果，不重复运行无意义测试以凑工具数量。

人工检查：我亲自从 README 完成一次演示；B 在自己的系统用同一候选复现，并交叉问答整个启动链。双方各选一个真实调试问题或机制验证说明证据如何支持结论。检查材料无 token、个人敏感信息，截图对应实际命令；所有“已实现”都有证据。未满足全验收就只能命名阶段成果，不能打完整 v1-passed 标签。

提交文档 PR 交 B 审查。合并后记录最终 main SHA，并由两人确认结论；若只是文档变化说明被测代码未变，若 demo/脚本变更则两机复跑相关入口。全部必需项通过后建议稳定标签 v1.0-course（先检查不存在，不覆盖现有标签）；在我当前指令授权范围内执行或明确给出我亲手打/推标签的步骤，不伪报远端状态。不要合并未经 B 审查的 PR。

最后按统一报告：通过/未通过/受阻、最终代码与材料 SHA、两机测试表、人工记录、PR和标签真实状态、证据路径、未完成事项、汇报前检查表结果与准确演示命令。说明本次只完成 V1，无后续实现自动开始。
```

## 四 B 可复制的提示词 按阶段取用

### S01 B openSUSE 克隆 环境 构建与接口审查

```text
你协助 B，本机应为 openSUSE。项目为两人 Linux 课程 Mini-Container，C++17/CMake，当前 S01，V0前骨架。A 使用 Ubuntu，负责核心；我负责构建测试、cgroup和交叉验证。不依赖任何旧聊天。

参数：仓库=<REPO_URL>，本地父目录=<LOCAL_PARENT_B>，前置已推送 main SHA=<C00>。占位符未填/没有访问权限则受阻；不要创建另一个同名仓库，不覆盖现存目录。先确认实际 Linux、工作目录与已有 git 状态，再克隆或核对正确 remote；在干净仓库拉取 main 并核对 C00。读取 AGENTS.md、DEVELOPMENT_PLAN.md、V1_AGENT_PROMPT_MANUAL.md、docs/contracts.md/workflow.md/stages.md、公共头、README/CMake。文件缺失找 A 交接，不自行补上游核心。

做 openSUSE 实机环境检查：os-release（包括 Leap/Tumbleweed 的实际版本）、uname架构/内核、虚拟化、id、systemd、git、C++编译器、CMake、GDB、strace、file/readelf、BusyBox、cgroup2挂载、当前归属和cpu/memory可用情况。不假定与Ubuntu相同。先用 zypper 查询实际软件包候选，再准备必需依赖；名字找不到就给出查询结果，不照抄 apt 包名，不升级系统或关闭安全策略。权限不足保留准确待办。BusyBox用file/readelf检查架构和interpreter/NEEDED，明确静态/动态/未装。

建立 b/s01-env-build，只改CMake/构建支持、scripts/check-env.sh、B环境证据、compatibility、stages。保持A公共头和main_cli语义，不实现cgroup/Runtime。审查公共契约：精确签名是否足以create/attach/snapshot/cleanup、move-only所有权、单位、错误/退出码/JSON、目录职责、清理与并行规则；问题写接口审查意见交A修公共头，双方明确确认后才能进入S02。

check-env.sh只读诊断，输出版本、架构、能力和具体阻碍；构建能力与特权V1能力分别标记，不能因没有控制器就把环境判为完整通过。此时不创建cgroup层级、不改mount/hostname。构建：cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug；cmake --build build/debug -j2；./build/debug/minic --help和--version。应返回0且说明骨架；保存日志与状态。新增公共头编译检查；未实现功能不以stub返回成功。无用GTest下载不加入。

人工检查：我亲自完成clone、一次构建、运行--help、查看实际cgroup挂载/架构和git log；让我解释同一源码为何要两机原生构建、何谓控制器存在但未委派、接口所有权如何防冲突。由我确认记录。A在Ubuntu也运行相同构建和检查，提供对应记录后再合并构建变更。

验收：openSUSE骨架可构建、环境证据完整、Ubuntu构建未退化、接口被双方确认；完整V1能力不足可以列为后续阻碍，但不得隐藏，严重到无法构建则本阶段受阻。修复原分支，不提前开始S02/S05。按统一报告输出状态、逐项PASS/FAIL/SKIP/NOT_RUN与证据、人工反馈、修改/接口意见、分支/被测SHA/提交/PR。推送给A审查合并，登记C01；A拉取C01进入S02。不要自行合并自己的PR。
```

### S03 B V0 测试 自动构建与内存检查

```text
你协助B，openSUSE，C++17 Mini-Container V0，当前S03。前置A的S02已审查合并C02=<C02>。从仓库重新读取AGENTS.md、两份方案、contracts/workflow/stages、实际Runtime/公共头/CLI/CMake和A的V0调试记录，不依赖聊天记忆。

核对系统、干净工作区、main含C02、前置通过。建立b/s03-v0-tests。只修改tests、CMake/cmake、scripts/run-stage.sh、B证据、compatibility/stages；Runtime/CLI错误交A修复并合并后同步，不在测试里绕开；公共接口走双人流程。此阶段不实现namespace/cgroup，也不承诺内核资源正确性。

实现CTest与scripts/run-stage.sh v0 --output DIR；单元测试覆盖参数边界/RunResult分类，集成测试实际运行true、exit 7、不存在程序、权限拒绝、带空格argv、负载本身exit125/127，证明JSON能区别payload退出和运行器失败。用自己的安全临时文件测试权限拒绝，不能改系统文件。新增ASan构建选项和非特权测试，不把未执行的子进程代码算已覆盖。未检测到测试或跳过必需项应显式失败/不完整。

运行Debug配置/构建、ctest --test-dir build/debug --output-on-failure、./scripts/run-stage.sh v0 --output artifacts/s03/v0。另以文档约定的实际选项构建ASan，并对参数/错误/RAII的非特权测试运行；保存详细输出、状态和覆盖范围。若用Valgrind则Debug非ASan构建单独执行，error-exitcode设为非0；不强制两者都装。不要以“没有报告”代替确认工具确实启用。

验收：每例有CASE_ID/完整命令/预期/实际/状态/代码SHA；0/7/127与result.kind正确，错误程序不被误记exited；ASan无已覆盖范围的非法访问；连续多次V0运行无未回收直接子进程。A拉取测试分支或合并前的精确被测提交在Ubuntu运行相同测试，提供真实结果；如果A必须先同步接口修复，等其合并再更新分支重测。

人工检查：我亲自执行一次ASan或Valgrind测试，解释报告中的检查范围与“无泄漏报告不等于无cgroup/挂载残留”；亲自查看一次JSON与shell退出状态。A亲手重复一例失败命令。记录实际问题、定位证据与复测，无问题可记录机制验证。人工未完成是受阻。

失败先定位实现/测试/环境所有者，回原阶段修复、重跑相关及V0回归；不得开始并行S04/S05。输出统一阶段报告：通过/未通过/受阻、逐项状态和证据、两机结果、人工检查、被测SHA/工作区、修改清单、PR/审查状态。A审查合并记C03；双方拉取C03后分别开a/s04-isolation与b/s05-cgroup。
```

### S05 B cgroup 测试负载与委派环境

```text
你协助B，openSUSE，Mini-Container V1资源模块，当前S05。基线C03=<C03>已通过；与A的S04并行，A先合并为C04，我在合并前同步它。不依赖旧聊天，先读AGENTS.md、方案/手册、contracts/workflow/stages、冻结cgroup头/RunConfig/Result、构建与V0测试、两机环境诊断。

检查干净工作区、基线、真实cgroup2与cpu/memory和systemd能力，建立b/s05-cgroup。仅改src/core/cgroup.cpp、tests/cgroup探针和workloads、B主责构建/委派/负载脚本、B证据和compatibility。A核心/CLI/Rootfs脚本/公共头不改；接口不足先申请由A提交变更。可按A的明确清单登记S04源文件、添加run-stage isolate用例，不改隔离实现。

实现契约create/attach/snapshot/cleanup，检查memory.max、cpu.max、memory.swap.max（本轮需要时）、cgroup.procs写入与读回，不吞errno；清理幂等但残留/忙错误必须报告，析构不抛异常。不在模块内部启动进程，不调用systemd替代自身限额写入。不支持的请求返回明确能力错误，禁止静默不限额。

按本机实际systemd版本实现prepare-cgroup-env/with-delegation：只建立本项目本次独占的委派域，验证Delegate和控制器沿祖先可用；将manager移入独立叶子，空父启用控制器，负载放独立叶子；传出实际CGROUP_BASE。保留原命令退出码并清理本次域。先在文档展示解析路径/布局和即将执行的动作，不碰系统根控制器、不移动无关服务。权限不足、控制器被占用、只读/非v2等报告具体检查和errno；不要自动改内核参数、重启、关闭安全策略。A在Ubuntu验证其实际可用性；不能拿openSUSE命令当然适用Ubuntu。

实现可信内存负载（逐页实际写内存，有最大上限与可打印进度）、单进程单线程固定时长CPU忙循环、输出自身host可关联信息的等待负载、错误/后代测试辅助。测试负载不读取任意宿主文件。build-rootfs-workloads.sh检查实际架构与链接方式；优先静态构建，缺静态工具链则明确列出动态loader/库打包待办，不把宿主动态二进制直接当Rootfs可用。实现cgroup探针：独立受控子进程先阻塞，配置/attach后放行，运行期采样，结束保存快照再cleanup，不提前改Runtime。

运行Debug构建/CTest/V0回归。真实Linux运行sudo ./scripts/with-delegation.sh --config .local/machine.env -- ./scripts/run-stage.sh cgroup --output artifacts/s05/cgroup。探针检查：child PID在叶子的cgroup.procs；memory.max和cpu.max读回精确相符；内存试验例如64MiB上限、swap.max=0、逐页写到128MiB（先确认机器有安全余量），该叶子memory.events的oom_kill增量>=1，不能仅看SIGKILL；若swap控制不可用记录受阻/改用明确且经两人批准的可复现方案，不伪装同条件通过。

CPU比较同一委派环境中两次独立会话，均采cpu.stat：不限额单线程忙循环约5秒与quota20000/period100000微秒约5秒。记录Δusage_usec/实际墙钟微秒、Δnr_throttled、读回的cpu.max；限额试验允许0.1核的测量余量，上界参考0.30且Δnr_throttled>0，baseline在安静机器建议>=0.7核；baseline不足则环境不具可比性，重测或受阻，不强行判通过。不要求精确20%。保存运行期和结束前快照，不在cgroup已删后读取。

负例：构造本次专用无控制器/不可写/错误路径配置与attach失败，不修改系统目录权限；负载标记不出现，探针返回明确阶段错误，最终仅清理自己资源。不能用root对普通只读文件权限测试当然可拒绝；选择真实可证明的失败条件或测试替身。

人工检查：我亲自建立一次委派环境、读cgroup.procs/memory.max/cpu.max/memory.events/cpu.stat，解释manager为何分叶、CPU额度的单位、OOM计数和信号为何都需要。A在Ubuntu亲自运行探针并记录环境差异。工具无法运行时报告实际限制，不让我背无验证的输出。

验收：模块探针/负例/两机环境记录/人工检查通过，隔离探针如还依赖S04则明确等待。A合并C04后，将main合入本分支、核对接口与run-stage isolate并跑回归；冲突只处理自己范围，核心冲突交A。未通过不交接S06。按统一报告给状态、逐项PASS/FAIL/SKIP/NOT_RUN、命令/预期/实际/证据、快照、被测SHA、接口、人工与两机结果、提交/PR。A审查合并记C05；A拉取C05才能集成S06。
```

### S07 B 完整验收脚本 openSUSE 故障与清理

```text
你协助B，openSUSE，Mini-Container V1，阶段S07；前置S06已通过合并C06=<C06>。先读AGENTS.md、方案/手册、contracts/workflow/stages、实际Runtime/cgroup/Rootfs和CLI、所有脚本及A集成记录。确认main含C06、工作区干净、本机Rootfs/委派准备完成；新建b/s07-acceptance。

只改tests、构建支持、run-stage/run-acceptance/demo、B证据和compatibility；核心失败交A修、cgroup失败由我修；公共头双方讨论后A修改。不能减弱断言让测试变绿，不加网络/Seccomp/daemon。实现手册统一命令入口和PASS/FAIL/SKIP/NOT_RUN汇总、返回码0/1/2，使用每轮唯一run_id，日志记录被测SHA/是否dirty。设置超时和有界负载，timeout本身也要能清理并报告，不能把外部timeout退出误当容器正确回收。

用真实Linux完整运行sudo ./scripts/with-delegation.sh --config .local/machine.env -- ./scripts/run-acceptance.sh --output artifacts/s07/v1，另运行Debug/CTest/V0/isolate回归。验收用例必须覆盖：
1 成功、负载exit7、负载exit125/127与exec失败的JSON区别；Rootfs缺失/无权限或loader缺失有正确步骤与非0结果。
2 hostname内外对照；长任务存活时比较host PID与namespace链接；容器/proc无宿主普通进程，包含实际测试进程而非固定PID清单；Rootfs标记可见、宿主标记不可见，宿主mountinfo不受影响。
3 full模式限额文件读回；内存负载逐页写，OOM前后事件在cleanup前采样并Δoom_kill>=1；CPU以S05的同机baseline、时长、读回和节流规则验证。
4 cgroup设置/attach失败时可信负载外部标记绝不出现；父控制端意外关闭、exec错误与挂载失败有结构化错误。不可安全触发的路径可用测试构建替身验证控制逻辑，但标注模拟，不替代真实挂载/限额集成。
5 正常、负载非0、exec失败、限额失败、中断五类退出；向CLI发SIGINT和SIGTERM后在规定超时内结束，namespace init及其测试后代消失、父端wait已完成、cgroup为空且被删除；不要求PID1响应普通TERM。
6 连续三轮（含失败）后按run_id查进程/PID与ns标识、自己挂载、cgroup；不能只grep一个易重用PID。不删除Rootfs原始目录，把“namespace销毁的挂载清理”与“Rootfs资产保留”区分。

证据需要：运行期PID/ns/cgroup/mount记录、父进程wait结果、JSON、cpu/memory快照、清理后的不存在/空状态、确切命令和退出码。进程立即退出的场景用受控握手负载获取运行期证据，不靠随意sleep抢时序。测试seam只能在测试构建，不能为生产CLI留下任意故障注入入口。保存失败原始记录，修复后另建复测记录。

人工检查：我亲手跑一轮完整验收并挑一次故障路径用strace观察write/close/wait；再查看非特权ASan结果和一次真实cpu/memory快照，说明用户态内存检查与内核资源限制不同。让我画出完整时序，解释先限额后放行、快照后清理和SIGINT回收。记录真实观察，不用Agent执行替代人工。

有FAIL则状态未通过，先按所有者修原阶段模块并合并后更新测试分支、在最新SHA重跑；环境不足/SKIP/NOT_RUN或人工待做则受阻，不宣称双系统通过。阶段报告按统一格式列每个case、命令/预期/实际、状态和证据、问题/负责人与修复节点、人工反馈、改动、被测SHA/分支/PR。A审查合并记C07；A拉取准确C07做S08。未通过不得先发布候选。
```

### S09 B 同一候选复验 双方交换复现

```text
你协助B，openSUSE，Mini-Container V1，阶段S09，A配合。前置S08已合并候选C08=<C08>。先读AGENTS.md、方案/手册、contracts/workflow/stages、代码/脚本、A与B全部证据和兼容性问题，不依赖之前聊天。

检查工作区干净、main准确对应C08；若比C08更新，区分文档变化与代码变化，双方明确新的候选SHA后再跑，不能两机测不同实现。仅改B证据、compatibility、调试记录与阶段台账；不直接修核心/测试。若发现实现缺陷交所有者修复、合并为新候选，再回S08/S09两机重测；不能用本机未提交补丁通过。

从README准备本机Rootfs与委派环境，用同一候选分别原生Debug构建、ctest、V0和隔离回归、完整run-acceptance；复验一次非特权ASan用例。记录架构、内核/发行版、工具和依赖版本、完整SHA、所有CASE状态与证据。所有必需项PASS才算openSUSE通过；Ubuntu通过不能替代此处。A同时在Ubuntu按相同候选跑同一验收集并给真实证据，不要求数值/PID相同，只要求语义标准一致。

交换人工复现是必需门槛：B亲自用GDB在Ubuntu核心设计对应的本机child_entry/放行/exec断点观察，并亲自strace一次隔离启动，独立讲清每条管道、host PID/namespace PID、mount/proc与等待回收；A亲自按B说明执行资源限额并解释cpu.stat/memory.events与预期。两人各自将命令、观察、解释、疑问和结论写入各自证据目录。B可指导但不能替A写“已亲自完成”，A也不能替B确认。

人工共同检查：从程序启动到结束画一遍完整时序；互问错误管道意外EOF、cgroup attach失败、PID1信号、中断和cleanup错误如何报告；选一个真实问题说明证据如何排除其他原因。不清楚的点先补观察，再作完成确认。

验收：两机对同一实现候选的全部必需case为PASS，交换学习与审查完成，无未处置阻断项；环境受限则准确报告受阻，保留Ubuntu单机完成结论，不给双系统通过。输出统一阶段报告和两机并列表：状态、候选SHA、每项实际结果、FAIL/SKIP/NOT_RUN、命令/快照/证据、人工记录、问题、文档提交/PR。A审查合并记录C09；只有通过并合并后A拉取C09做S10。不要替A创建发布标签。
```

## 五 V1 总验收清单

以下项目既是 S07 脚本需求，也是 S08/S09 的最终判断依据。脚本参数若必须改变，先更新契约和两机说明；不能让手册命令失效却仍宣称可复制。

| ID | 运行入口或测试动作 | 通过标准 | 失败/受阻判断及证据 |
|---|---|---|---|
| V1-01 | 两机Debug构建、CTest、V0 runner | 同一实现SHA、构建成功，必需测试均执行且PASS | 构建失败为FAIL；0测试/跳过必需项不通过；保存构建与CTest日志 |
| V1-02 | full短命令+hostname/PID/ns检查 | host hostname不变，指定容器hostname生效；运行期namespace标识不同，host与容器PID关系可解释 | clone/mount权限不足为受阻；退回v0不算通过；保存运行期ns链接与JSON |
| V1-03 | full中运行BusyBox ps和根目录检查 | proc显示该namespace进程；Rootfs标记可见，宿主普通进程/标记不可见 | 不要求进程数量固定；保存命令、host/container对照和mountinfo |
| V1-04 | 内存负载超64MiB示例限额 | 参数读回正确、逐页写入、该cgroup Δoom_kill>=1，终止和事件关联 | 只有SIGKILL不够；swap未明确/负载无法执行则不完整；保存删除前事件与结果 |
| V1-05 | 5秒单线程CPU baseline/20%额度对照 | 同机可比baseline；quota/period读回正确，Δnr_throttled>0，使用率满足已约定容差 | baseline竞争过大先重测/受阻，不要求精确20%；保存墙钟和cpu.stat差值 |
| V1-06 | 真/假程序、exit7/125/127、缺Rootfs/loader | 负载退出与启动错误的JSON分类准确，errno/步骤/状态一致 | 仅依据shell退出值不充分；每例独立证据目录 |
| V1-07 | cgroup设置/attach失败+负载标记 | 明确错误、非0状态、负载未启动、child已wait、资源回滚 | 静默不限额或先运行后限额立即FAIL；模拟与实机测试分栏 |
| V1-08 | 控制端断开、挂载/exec失败 | 子进程不越过失败步骤，主/清理错误可见、无残留 | 不安全故障用测试seam，不能声称真实内核路径均已覆盖 |
| V1-09 | CLI SIGINT与SIGTERM、含后代负载 | 有界退出，PID namespace内负载终止并回收，自己的cgroup删除 | 不能要求PID1自行处理TERM；保存信号发给哪个host PID及回收记录 |
| V1-10 | 三轮含失败的重复运行 | host状态未改、无本轮进程/namespace/cgroup/挂载残留，Rootfs资产可保留 | grep固定PID或无记录地清掉残留不能证明；保存前后差异 |
| V1-11 | 非特权ASan或独立Valgrind测试 | 工具确实启用，已覆盖用例无非法访问/符合约定的泄漏结果 | 没运行记NOT_RUN；不能推导内核隔离正确；保存报告和范围 |
| V1-12 | A/B亲自调试及交换复现 | 各自实际命令、观察、解释和本人确认均存在 | Agent代跑不算人工完成；缺一方人工确认为受阻 |

验收表必须分别列 Ubuntu 与 openSUSE 的单项状态，并保留“实机/模拟”一列。完整“双系统 V1 通过”要求两机必需实机项均 PASS、模拟项明确标注且不替代关键机制实测、人工检查完成。若只主机通过，报告准确限定范围。

## 六 汇报前检查清单

- [ ] 填写准确汇报日，提前两天功能冻结；没有临时加 daemon/网络/Seccomp。
- [ ] main、稳定标签、演示二进制与材料引用的代码可对应；不是脏工作区特有结果。
- [ ] README 从构建、Rootfs到委派与验收可执行，命令已在对应系统验证。
- [ ] 现场不用临时联网下载依赖；必要权限、Rootfs与测试资产就绪。
- [ ] 演示前环境预检；出现权限/控制器错误立即报告，不静默降级。
- [ ] 精选“隔离、限额、一次失败清理”演示在时间内完成，失败时显示证据位置。
- [ ] 两人各有真实GDB/strace/内存或资源验证记录，能解释工具发现了什么。
- [ ] 双方都能画完整时序，解释同步、PID1、proc、errno、wait与清理。
- [ ] 每个已完成声明有实际CASE、被测SHA、环境与结果；未执行、跳过、失败分别列出。
- [ ] 原始失败及修复复测记录保留；没有编造工具输出或借截图冒充本人操作。
- [ ] 双系统差异如实记录，未通过项没有从材料中隐藏。
- [ ] 备份真实演示录像与日志，并说明录像只是现场故障备用。
- [ ] GitHub无令牌、机器配置私密值、Rootfs大文件和大量构建产物。

## 七 参考与适用边界

本手册基于仓库 DEVELOPMENT_PLAN.md 细化；为保障一周内学习与验收，固定 A/B 职责并收紧到 V1。本文件本身不执行仓库创建、软件安装、系统配置或代码实现。系统版本未知时，后续 Agent 应以实机和官方资料核对，不根据发行版名称推测。

- [Linux namespaces](https://man7.org/linux/man-pages/man7/namespaces.7.html)
- [PID namespace 与 PID 1](https://man7.org/linux/man-pages/man7/pid_namespaces.7.html)
- [Mount namespace 与传播](https://man7.org/linux/man-pages/man7/mount_namespaces.7.html)
- [cgroup v2 控制器与事件](https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html)
- [systemd cgroup 委派](https://github.com/systemd/systemd/blob/main/docs/CGROUP_DELEGATION.md)
- [GDB fork 调试](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Forks.html)
