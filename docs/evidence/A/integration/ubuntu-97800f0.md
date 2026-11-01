# Ubuntu 固定候选原生验收记录

执行人：A（yschannel）；Ubuntu 24.04 / VMware，普通终端本人执行。日期：2026-11-01（Asia/Shanghai）。Agent 只核对和整理产物，没有代跑 sudo 或重新运行验收。

被测实现及验收脚本 SHA：`97800f0244b807466e7c516021f3c40b882abf45`。原始汇总12项均记录相同 SHA、`dirty=false`、`status=PASS`；每例独立 result.json 与汇总一致。Debug 构建、CTest 1/1 PASS 和验收总退出码0由 A 本人回传确认，不冒称这些信息另存于 verification.json。

本记录是后续纯文档提交，不改变被测源码/测试。PR #6 的后续提交 `911ccadfe9e477e8201f77f9c1ffe33c35426cfd` 仅增加 B 的记录；本次结果仍明确归属于上述固定候选。

## 证据来源与复现入口

原始目录：`artifacts/b3/native-20261101-173747/`，verification.json 含各例完整 command、SHA、dirty、预期/实际、状态和结构化结果；各子目录另有 result.json、stdout.log、stderr.log。只提交本精简记录，原始 artifacts 保留本地。

verification.json 的 SHA-256：

```text
627e4ff44f3a90db2e13961cd2e78be98b3319e9ab455ab7909073ab7b880ee8
```

同提交的统一验收入口（仅供定位/复现，本次整理未再次执行）：

```bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j2
ctest --test-dir build/debug --output-on-failure
./scripts/with-delegation.sh --acceptance
```

实际产物中的委派 jobs 为 `/sys/fs/cgroup/system.slice/minic-b3-1793525859-47843.scope/jobs`。除 isolate CPU baseline 外，正常 full 用例请求 memory=67108864 字节、swap=0、CPU quota/period=20000/100000 微秒；使用项目专用 .local/isolation-rootfs。

## 12项核对结果

| CASE_ID | CLI状态 | kind / 关键结果 | 状态 |
|---|---|---|---|
| smoke | 0 | exited / exit=0 | PASS |
| exit7 | 7 | exited / exit=7 | PASS |
| exit125 | 125 | exited / exit=125 | PASS |
| exit127 | 127 | exited / exit=127 | PASS |
| missing | 127 | startup_error / exec / errno=2 | PASS |
| create-failure | 125 | startup_error / cgroup.create / errno=0 | PASS |
| attach-failure | 125 | startup_error / cgroup.attach / errno=3（测试构建触发真实内核拒绝） | PASS |
| cpu-baseline | 0 | exited / exit=0（isolate，无 cgroup） | PASS |
| cpu-limited | 0 | exited / exit=0 | PASS |
| memory-oom | 137 | signaled / signal=9 | PASS |
| interrupt-sigint | 137 | signaled / signal=9 | PASS |
| interrupt-sigterm | 137 | signaled / signal=9 | PASS |

负载自身返回125/127保留 exited；missing 为 startup_error/exec/ENOENT，未仅凭 shell 数值混淆两者。smoke 负载在 set -e 下检查 hostname=minic-full、PID=1、/dev/null 字符设备与 Rootfs 标记，并执行 ps。

## 关键资源与失败路径证据

CPU baseline 在 isolate 模式运行有界5秒负载，墙钟5.039396秒；该例没有 cgroup 快照，不能声称采集了基线 CPU 使用量。cpu-limited 在 full 模式墙钟5.076948秒，删除前快照：

```text
cpu.max: 20000 100000
cpu.stat:
  usage_usec 1021486
  user_usec 1019520
  system_usec 1966
  nr_periods 51
  nr_throttled 51
  throttled_usec 3991774
```

CPU用量/墙钟约为单核20.12%，实际观察到节流，不只凭配置值判断限额生效。

memory-oom 使用64MiB/swap0限制运行96MiB有界写入负载。独占新叶子删除前快照：

```text
memory.max: 67108864
memory.swap.max: 0
memory.events:
  max 49
  oom 1
  oom_kill 1
  oom_group_kill 0
cgroup.events:
  populated 0
  frozen 0
```

对应 CLI137、signal9；以本轮叶子的 OOM事件结合信号判断，不只凭137宣称OOM。本次未保存该新叶子启动前的独立计数快照，不伪称有两份前后快照。

create-failure 使用非 cgroup v2 的 Rootfs 目录作基目录，返回 cgroup.create/errno0，消息为 base is not on cgroup v2；Rootfs/cgroup未启用、snapshot=null，负载标记 must-not-run-47850 未出现（由同提交runner断言与总退出0支持）。

attach-failure 使用 `minic_attach_rejection` 测试专用构建，宏 MINIC_TEST_ATTACH_REJECTION 将传给内核的PID替换为INT_MAX；Runtime自己的子进程仍在等待放行。实际为内核写 cgroup.procs 返回 ESRCH=3，而非假成功或本地参数拒绝。负载标记 attach-must-not-run-47850 未出现；enabled.cgroup=false，快照中 memory.current=0、cpu.stat usage_usec=0、cgroup.events populated=0，失败会话仍采快照并清理。此项是测试构建诱发真实内核错误，不能描述为生产二进制自然发生的attach故障。

## 中断、后代与回收

| 中断发送给宿主CLI | 运行期cgroup成员（宿主PID） | 实际结束 | 清理 |
|---|---|---|---|
| SIGINT | 47882、47883 | CLI137 / signaled / signal9 | PASS |
| SIGTERM | 47885、47886 | CLI137 / signaled / signal9 | PASS |

runner先观察同一叶子含namespace init和后代，再给CLI发信号。两项结果均保留删除前snapshot（populated=0），cleanup.complete=true；runner断言叶子与采样成员的/proc路径均消失。signal9反映Runtime对namespace PID1的有界终止，不冒写成负载直接响应SIGINT/SIGTERM。

12项均 cleanup.complete=true、cleanup.errors=[]、snapshot_error=null、evidence_errors=[]。普通用例检查结束后的宿主PID及快照路径不存在，并检查宿主hostname/mountinfo与开始时相同；中断用例检查叶子及记录成员消失。最后断言jobs无子叶并rmdir jobs。总退出0由A本人确认，支持这些runner断言执行完毕；未额外回传systemd scope退出后路径查询，不扩大为独立事后观察。

## 结论边界与协作

本记录确认Ubuntu在固定候选97800f0上的表列12项通过，补齐此前该集成候选未测的attach拒绝与两种中断回收。既有7项记录保留为历史，不覆盖或改写；未重复GDB/strace。

B 已在自己的 docs/evidence/B/B3/report.md 记录相同实现候选的openSUSE12项通过；本记录只为A的Ubuntu执行负责，不代B签认其他人工学习，也不自动将整个V1或PR审查/合并状态标为完成。

只新增本文件。B继续维护自己的报告及PR #6分支，由B将本文档提交cherry-pick到其最新分支后推送；A不直接写B分支、不自动合并PR。
