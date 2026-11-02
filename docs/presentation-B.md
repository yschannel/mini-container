# B-4 资源与工具讲解（约 90 秒）

基线：main `25cf83acb7bfe080e12c1eb19d4318721f7fb109` 包含 PR #6；两机完整原生结果针对相同的干净实现/脚本候选 `97800f0244b807466e7c516021f3c40b882abf45`，不是本讲稿提交。新版分工和判定见 [流程](workflow.md) 与 [加速流程](../V1_ACCELERATED_WORKFLOW.md)。

## 从委派到放行

`scripts/with-delegation.sh --acceptance` 由普通用户启动，经 sudo/systemd-run 建一次性 `Delegate=cpu memory` scope；`tests/full_acceptance.py` 核实所在 scope，把管理进程放入 `manager`，只在独占 `jobs` 下启用控制器和建叶子。它要求 cgroup v2、可委派的 cpu/memory、namespace/mount 权限及预备的静态 Rootfs；不是直接改系统根 cgroup。`src/core/runtime.cpp` 的子进程先阻塞；`src/core/cgroup.cpp` 校验委派基目录、创建叶子、写 `memory.max`、`memory.swap.max`、`cpu.max` 并逐项读回，然后把宿主 PID 写入 `cgroup.procs` 并核对成员；成功后父进程才放行负载。配置/attach 失败不放行。部分配置失败时 create 立即回滚叶子；attach 失败仍由父进程采样并清理会话。[B-3 审查](evidence/B/B3/report.md)、[A 集成说明](evidence/A/integration/report.md)。

## 读懂三个数与失败回收

`cpu.max = 20000 100000` 单位都是微秒：每 100000 微秒周期允许 20000 微秒 CPU 时间，单核持续繁忙时约 20% 单核额度；quota 可以大于 period，不能一概解释成进程总占用百分比。`cpu.stat` 的 `usage_usec` 是累计 CPU 时间，`nr_throttled` 是发生节流的周期数，`throttled_usec` 是累计节流时间；需结合墙钟和实际读回判断。Ubuntu 限额例墙钟 5.076948 秒、usage 1021486 微秒、51/51 个周期节流；openSUSE 的 usage 1010618、nr_throttled 50。数值不要求两机相同。[Ubuntu 12 项](evidence/A/integration/ubuntu-97800f0.md)、[openSUSE 12 项](evidence/B/B3/report.md)。

`memory.max=67108864`（64 MiB）、`memory.swap.max=0`，运行有界 96 MiB 写入。新叶子删除前快照的 `memory.events oom_kill=1`，配合负载 signal 9 / CLI 137，才支持本轮 OOM 判定；仅看 137 不能确定是 OOM。记录没有独立保存启动前计数快照，所以不称为两份快照的差值。`memory.events max` 表示越过 max 边界的次数，不等于杀死次数；`oom` 与 `oom_kill` 也不能混用。

运行结束先 wait，再 snapshot，最后 cleanup/rmdir；`result.json` 分开记录 `primary_error`、`snapshot_error`、`cleanup.errors`。负载自身 exit125/127 的 `kind=exited`，与启动错误 `kind=startup_error` 不同。12 项包括 create/attach 未放行、SIGINT/SIGTERM 后代消失和叶子清理。attach 拒绝由 `minic_attach_rejection` 测试构建把 attach PID 改成 `INT_MAX`，由内核返回 ESRCH；这不是生产程序自然故障。[测试构建](../CMakeLists.txt)、[runner](../tests/full_acceptance.py)、[资源实现](../src/core/cgroup.cpp)。

## 工具与两机边界

标准 Debug 命令是 `cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug`、`cmake --build build/debug -j2`、`ctest --test-dir build/debug --output-on-failure`；当前 CMake 注册的 CTest 是 `v0_regression` 1 项，特权 12 项须另用统一委派入口，不能说 CTest 覆盖它们。ASan 以 `-fsanitize=address -fno-omit-frame-pointer` 独立构建；openSUSE 默认测试因 LeakSanitizer 初始化 fatal error 失败，`ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/asan --output-on-failure` 1/1 通过，只证明已跑的非特权路径未报地址错误，**泄漏检测未验证**，也不覆盖内核 cgroup、namespace、挂载回收。不要把 ASan 二进制放进紧内存 OOM 演示。[B 工具记录](evidence/B/S02/report.md)、[构建定义](../CMakeLists.txt)。

Ubuntu 24.04/VMware 与 openSUSE Tumbleweed 20261028 的 systemd/内核和计数细节不同；两机各自原生构建、在同一候选跑统一 12 项且均 PASS。原始日志在各自本机 `artifacts/b3/native-*/verification.json`，提交的精选记录分别在上面两份文档；不把两机 PID、节流计数或 scope 路径当成固定常量。

## 给 A 的共同提纲短段

“B 负责资源和工程工具：systemd 将 cpu/memory 委派给本次 scope，父进程在放行子进程前创建 cgroup 叶子、写并读回 20ms/100ms CPU 额度与 64MiB/swap0，attach 宿主 PID；结束后先保存计数快照再删除叶子。双机同候选 12 项通过：CPU 有实际节流，OOM 有本轮叶子的 `oom_kill`，配置/测试构建 attach 拒绝不放行，中断后代已清理。CTest 是非特权 V0 回归；ASan 关闭泄漏检测后 1/1 通过，泄漏检测和特权资源路径不在其验证范围。”
