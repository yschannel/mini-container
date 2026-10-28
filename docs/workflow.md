# 协作约定

A：yschannel（Ubuntu）；B：Blonisco（openSUSE）。汇报 2026-11-04，功能冻结 2026-11-02。



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


## 证据与统一报告


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
