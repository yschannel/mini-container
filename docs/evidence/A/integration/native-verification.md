# A-2 Ubuntu full 实机记录（本次已执行范围）

执行人：A（yschannel），本人在普通 Ubuntu 24.04 / VMware 终端执行，不是 Agent 代跑或测试替身。日期：2026-11-01（Asia/Shanghai，依据证据目录时间）；本人回传脚本总退出码为 0。

被测实现及脚本 SHA：`d045d5939ffa315c00bbe716f39cc8cf0c16969a`。verification.json 的 7 项均记录 dirty=false，与整理前本地 HEAD 一致；各例独立 result.json 与汇总逐项相同。该 SHA 基于已合并 A-1 main `6088ccece75b450255c99ab269f059bdb2efe30e`，取入 B 资源分支 `d3ba16d`；是集成候选，不是 main 完整 V1 验收结论。记录的 SHA 来自脚本 Git 元数据，本次未新增二进制构建标记验证。

## 命令与证据来源

使用此提交内的 native_full.py，通过专用 systemd 委派 scope 执行：

```bash
sudo systemd-run --system --scope --unit="minic-a2-$(date +%s).scope" \
  --property='Delegate=cpu memory' --same-dir \
  python3 "$PWD/docs/evidence/A/integration/native_full.py"
```

本次实际 scope 为 `minic-a2-1793523340.scope`，委派 jobs 基目录为 `/sys/fs/cgroup/system.slice/minic-a2-1793523340.scope/jobs`。管理进程在独立 manager 叶子，测试仅操作此专用 scope 的 jobs，未写系统根 cgroup。

原始证据：`artifacts/integration/native-20261101-165545/verification.json`，含各例完整命令、SHA、dirty、墙钟、预期/实际和结果；各子目录保存 result.json、stdout.log、stderr.log。本记录和 selected-snapshots.json 为精选摘录，不提交整个 artifacts。复现参数以固定提交中的 native_full.py 为准，运行不需要重做 GDB/strace。

## 逐项结果

以下 full 用例除 create-failure 外使用 memory=67108864 字节、swap=0、CPU quota/period=20000/100000 微秒；Rootfs 为项目 .local/isolation-rootfs。cpu-baseline 明确为 isolate，未启用 cgroup。

| CASE_ID | 负载 / 条件 | CLI 状态 | kind / 错误 | 墙钟秒 | 状态 |
|---|---|---|---|---|---|
| smoke | shell 检查 hostname=minic-full、PID=1、null 字符设备、Rootfs 标记并运行 ps | 0 | exited | 0.099183 | PASS |
| exit7 | `/bin/sh -c 'exit 7'` | 7 | exited | 0.071927 | PASS |
| missing | `/not-a-program` | 127 | startup_error / exec / ENOENT=2 | 0.093233 | PASS |
| create-failure | 使用非 cgroup v2 的 Rootfs 目录作 cgroup-base | 125 | startup_error / cgroup.create / errno=0 | 0.062962 | PASS |
| cpu-baseline | isolate 下 `/bin/minic-workload cpu 5` | 0 | exited | 5.054158 | PASS |
| cpu-limited | full 下 `/bin/minic-workload cpu 5` | 0 | exited | 5.073864 | PASS |
| memory-oom | full 下 `/bin/minic-workload memory 96`，写入96MiB | 137 | signaled / signal=9 | 1.081050 | PASS |

smoke 的 shell 使用 set -e，所有前置检查成功；ps 显示容器内 PID 1。full 成功启动用例的 enabled 包含 PID/UTS/Mount/Rootfs/cgroup=true。cpu-baseline 的 cgroup=false、snapshot=null，没有把 isolate 冒写成 full。

create-failure 在 clone 后拒绝非 cgroup v2 基目录，错误消息为 `base is not on cgroup v2`；Rootfs/cgroup enabled=false，snapshot=null，cleanup.complete=true。脚本在命令返回后断言 `/tmp/must-not-run-46043` 对应的 Rootfs 标记不存在，证明该创建失败路径未放行负载。此例不是 attach-failure，也不覆盖 cgroup 部分写入失败回滚。

## CPU 与 OOM 原始计数

cpu-limited 的删除前快照：

```text
cpu.max: 20000 100000
memory.max: 67108864
memory.swap.max: 0
cpu.stat:
  usage_usec 1013068
  user_usec 1009209
  system_usec 3859
  nr_periods 50
  nr_throttled 50
  throttled_usec 3964054
```

CPU 用量/墙钟约为 `1.013068 / 5.073864 = 19.97%`（相对单核），并观察到实际节流。cpu-baseline 仅记录同一有界 CPU 负载的墙钟和退出结果，没有采集其 CPU 使用计数，因此不声称测得基线 CPU 用量或完整计数差值对照。

memory-oom 使用本次独占新运行叶子，删除前快照：

```text
memory.max: 67108864
memory.swap.max: 0
memory.events:
  low 0
  high 0
  max 50
  oom 1
  oom_kill 1
  oom_group_kill 0
  sock_throttled 0
cgroup.events:
  populated 0
  frozen 0
```

同时有实际限额读回、本轮叶子的 oom_kill=1 和 signal=9；不只凭 CLI 137 宣称 OOM。未单独保存该叶子启动前的计数快照，不伪称已采集前后两份快照。所选原始完整快照见 selected-snapshots.json。

## 回收检查与范围边界

7 项均 cleanup.complete=true，无 cleanup.errors/snapshot_error/evidence_errors。脚本在每例结果落盘后检查宿主 PID 不存在、已删除的 snapshot.path 不存在、宿主 hostname 和 /proc/self/mountinfo 与开始时一致；末尾检查 jobs 无运行子叶并 rmdir jobs。本人报告脚本退出 0，说明包括 CPU/OOM 计数检查和末尾回收检查在内的断言均完成。没有独立回传 scope 退出后 systemd scope 路径的查询，不扩大为额外 scope 事后观察。

**仍为 NOT_RUN：本集成候选的原生 full attach-failure、full interruption/后代回收、最终 Ubuntu/openSUSE 同候选统一验收。** 既有替身测试、B 独立资源探针和 A-1 isolate 中断结果不能替代这些项。本次仅确认表列 Ubuntu 实机范围通过，不宣称整个 A-2、main 或双系统完整 V1 验收通过。
