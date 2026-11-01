# A-2 full Runtime 接入（进行中）

## 基线与所有权

开始时工作区干净，位于 a/v1-isolation。fetch 确认 PR #3 已合并，main 快进至 `6088ccece75b450255c99ab269f059bdb2efe30e`，从此建立 a/v1-integration。发现 B 已推送资源分支，快进取入 `d3ba16d`（B 实现 71dc77b、内核 attach 失败验证 a32188c 及证据 d3ba16d）。该组合是集成基线，不是 main 已验收 full。

没有修改 B cgroup.cpp、资源探针/工作负载、委派脚本或公共签名。CMake 只为 minic 链接已存在的 minic_cgroup 库。新版流程和实际 cgroup 契约已核对，不重复 S02 GDB 或 A-1 strace。

## 已完成

- full 验证 Rootfs、hostname、绝对 cgroup_base、显式正内存和 CPU 限额及内核 period 范围；CLI 以严格无符号整数解析字节/微秒，v0/isolate 拒绝资源选项。
- clone 子进程阻塞，父持有会话；create 完成设置/读回，再 attach 宿主 PID，成功后才发送 G。配置、attach 或已收到的中断失败，不放行，关闭控制端并终止/wait。
- 子进程复用隔离和 exec；父等待后 snapshot，再 cleanup。create 部分失败由 B 按契约回滚；attach 失败仍保存可用会话快照并清理。父的异常兜底先终止/wait 再释放会话和 clone 栈。
- FullSession 内部模板复用同一生产会话调用顺序，测试替身仅用于控制/错误保留测试；生产 minic 链接 B 真实 CgroupSession，未引入假成功桩。
- snapshot_error、cleanup.errors 与 primary_error 分开；负载成功而收尾失败为 cleanup_error/125，非零负载/信号保留原原因。JSON 序列化实际 limits、cgroup 标记和删除前原始快照内容，日志 stderr 报收尾错误。
- 关闭错误管道 EOF 后不继续轮询其 HUP，避免无等待地循环 waitpid。
- 复用 B build-rootfs-workloads.sh 已构建并安装静态 minic-workload 到现有专用 Rootfs，没有重做 Rootfs 或替 B 实现资源模块。

## 已执行验证

| 检查 | 状态 | 范围 |
|---|---|---|
| 标准 Debug CMake 配置/构建 | PASS | minic、B minic_cgroup/cgroup_probe、公共头均可链接 |
| V0 CTest | PASS | v0_regression 1/1 |
| 9 项生命周期/错误保留测试 | PASS（替身） | create/attach 失败无 release 事件；顺序为 attach→release→wait→snapshot→cleanup；快照/清理并发失败不覆盖主错误/负载状态 |
| JSON 往返 | PASS（替身） | full 实际 limits/swap0、cgroup=true、原始快照文本可解析 |
| Agent full 探测 | SKIP | clone errno=EPERM，未运行负载/创建 cgroup；不代表 VMware 原生能力不足 |
| 修订后原生 isolate/full/CPU/OOM | NOT_RUN | 需普通终端 root 专用委派执行 |
| 最终两机同候选/统一验收 | NOT_RUN | B 的统一 full runner 尚待衔接 |

测试前处于实现工作区，非 main 验收。提交后再给固定候选 SHA 与真实记录；不冒充已有原生 full PASS。

复现模拟测试：

```bash
c++ -std=c++17 -g -Wall -Wextra -Wpedantic -I. -Iinclude \
  docs/evidence/A/integration/control_test.cpp src/common/result_io.cpp \
  -o build/debug/integration-control-test
./build/debug/integration-control-test
```

## 真实 Ubuntu 下一步

B 的 with-delegation.sh 当前只允许非特权资源探针，不支持需要 root namespace/mount 的 full 命令。为继续独立验证，A 提供临时集成证据 harness native_full.py（不替代 B 的委派模块/统一 runner），仅允许在专用 minic-a2-*.scope 内以 root 执行，将当前管理进程移至 manager，给独占 jobs 启用 cpu/memory；不写系统根 cgroup。

普通终端执行：

```bash
sudo systemd-run --system --scope --unit="minic-a2-$(date +%s).scope" \
  --property='Delegate=cpu memory' --same-dir \
  python3 "$PWD/docs/evidence/A/integration/native_full.py"
```

覆盖真实 full smoke/限额读回、exit7、exec ENOENT、create 失败不放行、CPU（5秒有界负载）和内存（64MiB/swap0，负载96MiB）OOM；每例在删除后检查 cgroup 不存在，保存删除前快照，检查宿主状态未变。Agent 不代本人输入 sudo。最多约十几秒 CPU 测试，原始结果写独立 artifacts/integration/native-*。若环境失败保存 FAIL，不降级。

## 给 B 的具体依赖

B 的真实模块已取入且可链接，不再笼统等待实现。请 B 检查同一集成候选并把统一验收/委派入口扩展至 root Runtime（保留 manager 空父布局），提供 full 的真实 attach/config 失败不放行、中断及后代、CPU/OOM和三轮清理用例；测试替身不可替代。现有公共接口未发现必须变更的问题。B 不需改 A Runtime；若发现 cgroup/资源缺陷由 B 原模块修复，A 再取入。

A 接下来执行原生 full 验证/核心修复，复用本人已有机制观察，不增加文字答题门禁。B openSUSE 与 A Ubuntu 最终必须对同一实现/脚本候选测试。未自动合并，未开展扩展功能。
