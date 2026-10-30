# S01 阶段报告（B / openSUSE）

状态：两机骨架构建通过；阶段待 B 本人理解确认与 A 接口确认，未合并。汇报 2026-11-04，功能冻结 2026-11-02。

前置 C00：`40040ab865ece11b1f7211ca37e01ac4c2979d0f`。仓库：`https://github.com/yschannel/mini-container`；本地目录：`/home/zhangboyan/learning/BUPT/linux/mini-container`。本地 HEAD、`origin/main` 与远端 `main` 均为 C00，初始工作区干净；远端查询在沙箱外成功。实现与本机重测代码 SHA：`5284db753dc74d3c091746407479de60dad3dc15`。这份记录的后续提交只修改证据文字，不改被测代码。

系统：openSUSE Tumbleweed 20261028 / x86_64 / kernel 7.2.11-1-default。当前 Agent 运行环境虚拟化报告 `container-other`；cgroup2 挂载为只读，root `cgroup.controllers` 可见 cpu/memory，root `cgroup.subtree_control` 仅有 pids。不能据此声称已有可写 systemd 委派或完整 V1 能力。`/usr/bin/busybox` 动态链接，`/usr/bin/busybox-static` 静态链接、无动态节。工具已安装；`zypper --installed-only` 查询见 `packages.log`，无需安装依赖。

## 修改与接口审查

仅修改 B 主责范围：`CMakeLists.txt`、`tests/public_headers.cpp`、`scripts/check-env.sh`、`docs/compatibility.md`、`docs/stages.md` 和 B 的 S01 证据。未修改公共头、CLI、Runtime 或 cgroup 实现；未创建 cgroup 或修改系统挂载。接口审查见 `interface-review.md`，B 认为现有签名足够；A 确认待完成。

## 逐项验证

| CASE_ID | 命令 | 预期 / 实际 | 状态 | 证据 |
|---|---|---|---|---|
| S01-BASE | `git status; git rev-parse HEAD; git rev-parse origin/main` | C00、干净 / 符合 | PASS | 本报告；终端输出 |
| S01-REMOTE | `git ls-remote origin refs/heads/main` | 远端 C00 / 符合（沙箱外） | PASS | 本报告 |
| S01-ENV | `./scripts/check-env.sh --output artifacts/env.txt` | 只读诊断 / 已生成 | PASS | `environment.log` |
| S01-PACKAGES | `zypper --non-interactive search --installed-only ...` | 查询已装工具 / 成功 | PASS | `packages.log` |
| S01-CONFIG | `cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug` | 0 / 0 | PASS | `configure.log` |
| S01-BUILD | `cmake --build build/debug -j2` | 0，含公共头检查 / 0；在实现提交后重测通过 | PASS | `build.log`、本报告 |
| S01-HELP | `./build/debug/minic --help` | 0，骨架说明 / 符合 | PASS | `help.log` |
| S01-VERSION | `./build/debug/minic --version` | 0，骨架版本 / 符合 | PASS | `version.log` |
| S01-UNSUPPORTED | `./build/debug/minic run` | 125，不伪装功能 / 符合 | PASS | `unsupported.log` |
| S01-UBUNTU | A 在 Ubuntu 24.04.4 x86_64 对 `73bf10741497796e233e3cc3f729ee612f446439` 执行配置、构建、`--help`、`--version` | 四项符合骨架预期；`minic` 与 `minic_public_headers` 均构建通过 | PASS | A 回传的本人执行结果；本报告 |
| S01-HUMAN | B 本人按手册操作及解释 | 克隆、构建、帮助、架构、cgroup 挂载及 git log 已确认；三项理解待确认 | NOT_RUN | B 回传的本人执行结果；本报告 |
| S01-INTERFACE-ACK | A、B 双方确认接口 | B 已审查；A 待确认 | NOT_RUN | `interface-review.md` |

人工检查进展：B 回传本人终端输出，配置和构建成功、`--help` 显示骨架，`uname -m` 为 x86_64；本人终端 `findmnt -t cgroup2` 显示 cgroup2 挂载为 `rw`，与 Agent 沙箱内的 `ro` 不同，应以各自执行环境分别记录，`rw` 本身仍不证明已有项目可用委派。B 后续回传 `git log -1 --oneline`，显示 `73bf107 (HEAD -> b/s01-env-build)`，该项已确认。双机原生构建、控制器与委派区别、接口所有权三项理解尚待 B 明确确认。Agent 不代本人签认。

Git：`b/s01-env-build`；实现提交 `5284db753dc74d3c091746407479de60dad3dc15`；A 在 Ubuntu 被测分支 SHA `73bf10741497796e233e3cc3f729ee612f446439`。分支已推送；PR、审查/合并状态与 C01 待后续填写。A 审查合并后记录真实 C01，再交接 A 拉取 C01 进入 S02。S05 不提前启动。
