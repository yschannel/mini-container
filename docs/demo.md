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

待本人在普通终端按上面顺序实际彩排后填写：时间、系统、HEAD/dirty、命令和 shell 退出码、本次 `verification.json` 路径、12 项结果、演示耗时、失败与调整。没有本人回传前，本节状态为 **NOT_RUN**；已有 2026-11-01 双机验收是先前验证，不能当作 B-4 彩排。
