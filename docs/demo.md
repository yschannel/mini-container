# B-4 约三分钟现场演示与彩排记录

## 运行前提

在 openSUSE 的**普通终端**、仓库根目录、已同步 main 的构建运行；需 cgroup v2、systemd `Delegate=cpu memory`、sudo 权限、可用 namespace/mount、已装静态 BusyBox。`build/debug/minic` 和 `minic_attach_rejection` 应已构建；`.local/isolation-rootfs` 须有 `.minic-rootfs`、`/bin/sh`、`/bin/minic-workload`。缺 Rootfs 时按现有入口执行 `./scripts/prepare-rootfs.sh --output "$PWD/.local/isolation-rootfs"` 与 `./scripts/build-rootfs-workloads.sh --rootfs "$PWD/.local/isolation-rootfs"`；不临时安装依赖。展示前记录 `git rev-parse HEAD`、`git status --short`，避免把旧二进制说成当前提交构建。

## 操作顺序（现场约三分钟，CPU 两例各约五秒）

1. 0:00–0:30：显示 `git rev-parse HEAD` 和 `ctest --test-dir build/debug --output-on-failure`。说明 CTest 只含非特权 V0 回归，不能代替 full 验收。
2. 0:30–1:10：运行 `./scripts/with-delegation.sh --acceptance`。它自动在一次性委派 scope 中运行 12 项，并在 `artifacts/b3/native-*/verification.json` 与各 case 目录保存结果；预计控制台逐项显示 `smoke PASS` 到 `interrupt-sigterm PASS`，最后 `PASS: native full cases`。不要把已有记录的 PASS 冒充本次输出。
3. 1:10–2:10：打开本次 `verification.json` 的 `cpu-limited`、`memory-oom` 项及其 `result.snapshot.files`。解释 `cpu.max 20000 100000`、`usage_usec` 对约 5 秒墙钟的比值、`nr_throttled>0`；解释 64MiB/swap0、`oom_kill>=1` 与 signal9 的组合。实际数字以现场为准。
4. 2:10–3:00：展示 `attach-failure` 的 `kind=startup_error` / `primary_error.step=cgroup.attach`、`enabled.cgroup=false`、快照和 `cleanup.complete=true`，说明测试构建将 PID 换为 `INT_MAX` 才触发内核拒绝。再展示 `interrupt-sigint`/`interrupt-sigterm` 的运行期成员记录及回收结果。最后看汇总 12 项均 PASS 和本次退出码；若有 FAIL，按实际显示 FAIL 并停止“通过”结论。

为避免查找目录耗时，可在演示前记下**本次** `artifacts/b3/native-YYYYMMDD-HHMMSS` 路径并在编辑器打开 `verification.json`。演示中不另开第二轮完整验收。每例的 `result.json`、stdout.log、stderr.log 可用于临场解释。

## 失败时的备用证据

若 sudo、systemd 委派、Rootfs 或本机权限阻止现场运行，展示终端原始错误和已提交的 [openSUSE 记录](evidence/B/B3/report.md)、[Ubuntu 同候选记录](evidence/A/integration/ubuntu-97800f0.md)；说明二者都是候选 `97800f0244b807466e7c516021f3c40b882abf45` 的历史实机 12 项结果，不是这次现场运行。可展示这些记录中的 CPU/OOM 原始片段与失败回收断言。录像若使用须注明为此前真实运行，不能代替现场命令。若受限环境 `sudo` 报 `no new privileges`，这是沙箱限制，不能推断 openSUSE VM 不支持委派。

## 彩排记录

2026-11-02 19:16（目录时间，Asia/Shanghai），B 在 openSUSE 普通终端从干净的 `f2a08e6f71df86508634f7642da8d316060db37b` 执行 `./scripts/with-delegation.sh --acceptance`，scope 为 `minic-b3-1793618202-9006.scope`。本次原始记录：`artifacts/b3/native-20261102-191644/verification.json`。终端回传在 `cpu-limited FAIL` 后出现 Python `AssertionError`；末尾 `echo "exit=$?"` 输出在用户消息中截断为 `ex`，因此总 shell 退出码与完整演示耗时**未确认**。

汇总已有 9 项：smoke、exit7、exit125、exit127、missing、create-failure、attach-failure、cpu-baseline 共 **8 PASS**；cpu-limited **1 FAIL**；memory-oom、interrupt-sigint、interrupt-sigterm 因 runner 在断言处停止，均 **NOT_RUN**。本次彩排整体 **未通过**，不能引用 2026-11-01 的 12 项 PASS 覆盖它。

`cpu-limited` 的原始 result 是 `kind=exited`、CLI 0、`cleanup.complete=true`，读回 `cpu.max=20000 100000`、`memory.max=67108864`、`memory.swap.max=0`；墙钟 5.131918 秒，`usage_usec=1006722`（约 19.62% 单核墙钟），`nr_periods=51`、`nr_throttled=51`、`throttled_usec=4071771`。快照叶子及 scope 在事后查询时已不存在，宿主 PID 9427 事后亦不存在。这些值说明限额/节流现象出现，**不能**据此覆盖 runner 当时的 FAIL。

当前 `tests/full_acceptance.py` 在保存该 FAIL 前将 CLI/JSON、限额、叶子/宿主 PID 是否消失、宿主 hostname/mountinfo 是否未变合成一个布尔值，没有在汇总中记录失败的是哪一条。由于事后检查不能重建当时状态，失败断言的精确原因**尚未定位**。本阶段按约定不改源码或测试、不重复整套验收；现场演示改用本次 FAIL 的真实日志解释边界，并备好 2026-11-01 的两机历史证据。后续由测试负责人单独定位并复验受影响项，再决定是否重做完整彩排。
