# B-3 集成审查与验收交接

基线：`origin/main` 的 `0a5ddaa`（含 #4 合并提交 `92ac869` 和 #5 合并提交 `0a5ddaa`）。#4 已合并，因此本轮只审查 full 编排与 B-3 验收，不重复资源模块代码审查。仓库没有 AGENTS.md。

## 集成审查

- `Runtime::run` 在 clone 后让子进程等待控制字节；`FullSession::prepare` 先 create/写限额/读回，再 attach 宿主 PID，成功才发送 `G`。create、attach 或放行前中断失败时关闭控制端并杀死/wait 子进程。测试版 `minic_attach_rejection` 只把 attach PID 改为 `INT_MAX`，由真实内核拒绝；生产 `minic` 无故障入口。
- wait 后调用 `FullSession::finish`，顺序为 snapshot 再 cleanup。attach 失败仍有会话，会采样再删除叶子；create 的部分写入失败由 `CgroupSession::create` 自行回滚。主错误、snapshot_error、cleanup.errors 独立保存。
- 发现尚需原生验证的风险：PID namespace init 退出后，后代可能在 cgroup 中短暂滞留，使一次 `rmdir` 返回 `EBUSY`；新的 SIGINT/SIGTERM 测试会捕获这种情况。若复现，给 A 最小故障记录后修复核心，不把 cleanup_error 算 PASS。

## 本轮执行

| 项目 | 状态 | 证据 |
| --- | --- | --- |
| Debug 构建、`ctest` V0 | PASS | `cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug && cmake --build build/debug -j2 && ctest --test-dir build/debug --output-on-failure`：1/1 通过 |
| Python 与 shell 语法 | PASS | `python3 -m py_compile tests/full_acceptance.py`；`bash -n scripts/with-delegation.sh` |
| openSUSE 原生 full、attach 拒绝、中断/后代、CPU/OOM | NOT_RUN | Codex 沙箱 `sudo` 报 `no new privileges`，`/sys/fs/cgroup` 只读；不是 openSUSE 虚拟机能力结论 |
| Ubuntu 同候选完整验收 | NOT_RUN | A 现有 smoke/错误/CPU/OOM 结果来自旧实现 `d045d59`，可复用背景证据，不能记为本候选验收 |
| B 人工资源/隔离复现 | NOT_RUN | 未代签人工执行 |

## 普通终端复验

两机 checkout 同一候选 SHA 后，从仓库根目录运行：

```bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j2
ctest --test-dir build/debug --output-on-failure
./scripts/prepare-rootfs.sh --output "$PWD/.local/isolation-rootfs"
./scripts/build-rootfs-workloads.sh --rootfs "$PWD/.local/isolation-rootfs"
./scripts/with-delegation.sh --acceptance
```

Rootfs 若已准备且含 `/bin/minic-workload`，跳过两条准备命令。BusyBox 必须是静态链接；若本机只有动态 BusyBox，先安装对应静态包。原始结果保存在 `artifacts/b3/native-*/verification.json`，各 case 目录含 `result.json`、stdout/stderr。测试含 smoke、exit7/125/127、exec 错误、create 和真实内核 attach 拒绝不放行、CPU 限额与节流、OOM、SIGINT/SIGTERM 后代回收；每例检查快照和叶子删除。scope 仅触及本轮 `minic-b3-*` 委派目录。

最终两机完整验收尚未执行；当前不能宣布 V1 通过。任何源码或验收脚本修复都应更新候选 SHA 并重测受影响项。

## openSUSE 首轮原生尝试（失败记录保留）

B 在普通终端对 `ef7c296` 完成 Debug 构建与 CTest 1/1 PASS，随后启动了 `minic-b3-1793525152-92714.scope`。首个 smoke 的原始结果在 `artifacts/b3/native-20261101-172555/smoke/`：`primary_error.step=rootfs`、`errno=2`、`snapshot=null`、`host_pid=null`，原因是当时 `.local/isolation-rootfs` 不存在。旧 runner 在检查 snapshot 时抛出 `TypeError`，没有形成汇总。此轮 full 为 **FAIL（准备条件缺失且 runner 报告缺陷）**，其余 case NOT_RUN；不将 scope 已启动误记为 smoke 通过。

已在本机创建专用静态 BusyBox Rootfs 并安装静态 `minic-workload`。runner 已增加启动前 Rootfs/构建预检，并在 snapshot 为空时记录 FAIL 而非抛出类型错误。需对新候选在普通终端重新执行完整原生验收。

## openSUSE 复验：实现候选 `97800f0244b807466e7c516021f3c40b882abf45`

B 在普通终端执行 `git switch --detach 97800f0` 后运行 `./scripts/with-delegation.sh --acceptance`。scope 为 `minic-b3-1793525422-96297.scope`；原始记录在 `artifacts/b3/native-20261101-173022/verification.json`，12 项的 `sha=97800f0`、`dirty=false`、状态均 PASS：smoke、exit7、exit125、exit127、missing、create-failure、attach-failure、cpu-baseline、cpu-limited、memory-oom、interrupt-sigint、interrupt-sigterm。先前 Debug 构建和 CTest 1/1 PASS 已由 B 同机执行。

关键原始值：attach-failure 为 `startup_error/cgroup.attach`、退出 125、`enabled.cgroup=false`，Rootfs 的负载标记未出现，失败会话仍有 snapshot 且叶子已删除；CPU 限额读回 `20000 100000`，`usage_usec=1010618`、`nr_throttled=50`、`throttled_usec=3988902`；内存用例 `oom_kill=1`、signal 9。SIGINT 运行期成员 `96469/96470`，SIGTERM 为 `96472/96473`，两例均在 CLI 结束后消失，snapshot 叶子不存在，cleanup.complete=true。scope 退出后路径亦不存在。每例另存 `result.json`、stdout/stderr。

这确认了 **openSUSE 在该实现候选上的已覆盖原生用例通过**。Ubuntu 尚未对相同 SHA 跑统一入口，最终双机验收仍为 NOT_RUN；B 人工资源/隔离学习记录也尚未补齐。此处文档更新不改变被测实现。
