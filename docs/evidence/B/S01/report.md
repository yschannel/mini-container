# S01 阶段报告（B / openSUSE）

状态：技术检查通过；阶段受阻于人工确认、A 的 Ubuntu 同版构建及双方接口确认，未合并。汇报 2026-11-04，功能冻结 2026-11-02。

前置 C00：`40040ab865ece11b1f7211ca37e01ac4c2979d0f`。仓库：`https://github.com/yschannel/mini-container`；本地目录：`/home/zhangboyan/learning/BUPT/linux/mini-container`。本地 HEAD、`origin/main` 与远端 `main` 均为 C00，初始工作区干净；远端查询在沙箱外成功。被测代码为 S01 工作区，提交 SHA 待填写；最终提交后应重跑受影响项。

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
| S01-BUILD | `cmake --build build/debug -j2` | 0，含公共头检查 / 0 | PASS | `build.log` |
| S01-HELP | `./build/debug/minic --help` | 0，骨架说明 / 符合 | PASS | `help.log` |
| S01-VERSION | `./build/debug/minic --version` | 0，骨架版本 / 符合 | PASS | `version.log` |
| S01-UNSUPPORTED | `./build/debug/minic run` | 125，不伪装功能 / 符合 | PASS | `unsupported.log` |
| S01-UBUNTU | A 在 Ubuntu 构建本阶段代码 | 0 / 待 A 执行 | NOT_RUN | 待补 |
| S01-HUMAN | B 本人按手册操作及解释 | 待 B 确认 | NOT_RUN | 待补 |
| S01-INTERFACE-ACK | A、B 双方确认接口 | B 已审查；A 待确认 | NOT_RUN | `interface-review.md` |

人工检查待 B 本人确认：亲自完成克隆、一次构建、运行 `--help`、查看实际 cgroup 挂载与架构及 `git log`；解释两机为什么必须原生构建、控制器可见但未委派的区别、接口所有权如何防冲突。Agent 不代本人签认。

Git：`b/s01-env-build`；提交 SHA、PR、审查/合并状态与 C01 待后续填写。A 审查合并后记录真实 C01，再交接 A 拉取 C01 进入 S02。S05 不提前启动。
