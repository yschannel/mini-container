# mini-container

Linux 课程教学项目，C++17 / CMake。当前为 **S00 骨架**，只实现 `--help`、`--version`；不具备 V0 执行或 V1 容器功能。
A：yschannel / Ubuntu 24.04.4 VMware x86_64；B：Blonisco / openSUSE（待实机验证）。
汇报日 2026-11-04，功能冻结 2026-11-02。

```bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j2
./build/debug/minic --help
./build/debug/minic --version
```

使用系统 Make，不需要 Ninja，不自动下载依赖。尚无测试；无测试不能记为测试通过。
公共头可独立包含，Runtime/cgroup 仅声明，调用尚不能链接。

契约见 [contracts](docs/contracts.md)，责任与 Git/证据约定见 [workflow](docs/workflow.md)，阶段门槛见 [stages](docs/stages.md)，目标时序见 [architecture](docs/architecture.md)。
原始 DEVELOPMENT_PLAN.md 与 V1_AGENT_PROMPT_MANUAL.md 保留；具体身份、日期以本次阶段记录为准，方案中的未知日期描述是历史初稿。
接口初稿须 B 在 S01 审查并双方确认，不能提前实现 S02。

GitHub 仓库：https://github.com/yschannel/mini-container （已核实为 yschannel 的公开空仓库）；初始 main 尚未推送，C00 待核实。
不提交 build、artifacts、Rootfs 或本机私密配置。人工复核步骤见 docs/evidence/A/S00/report.md。
