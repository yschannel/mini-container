# mini-container

Linux 课程教学型容器运行时，C++17 / CMake，前台、非交互。已实现基础执行、PID/UTS/Mount namespace、专用静态 BusyBox Rootfs、cgroup v2 CPU/内存限制，以及失败与中断回收。

A：yschannel / Ubuntu 24.04 / VMware；B：Blonisco / openSUSE Tumbleweed / VMware。2026-11-04 汇报，11-02 起冻结新增功能。

## 版本与已验证范围

| 标识 | 完整 SHA / 结论 |
|---|---|
| 材料整理基线 main，已合并 PR #6 | `25cf83acb7bfe080e12c1eb19d4318721f7fb109` |
| 两机实际被测实现及验收脚本 | `97800f0244b807466e7c516021f3c40b882abf45` |
| Ubuntu / openSUSE | 同一被测候选各12项原生验收 PASS，总退出码0；Debug 构建和 CTest 1/1 PASS |

上述两个 SHA 的 src/include/tests/scripts/CMakeLists.txt 内容一致；不能把合并提交或本次文档提交写成当场被测 SHA。Ubuntu 记录见 [A实机验收](docs/evidence/A/integration/ubuntu-97800f0.md)，openSUSE 见 [B复验记录](docs/evidence/B/B3/report.md) 中的候选97800f0小节；文档前面的 NOT_RUN/失败是历史观察，保留供追溯。

12项为 smoke、exit7、exit125、exit127、missing、create-failure、attach-failure、cpu-baseline、cpu-limited、memory-oom、interrupt-sigint、interrupt-sigterm。CPU baseline 是 isolate 模式；attach-failure 使用测试专用二进制诱发真实内核 ESRCH，不是普通生产运行自然发生的故障。

## 构建

前提：真实 Linux，C++17 编译器、CMake≥3.16、Make、Python3。Rootfs 准备另需静态 BusyBox、file、readelf；资源负载构建需支持 `cc -static` 的本机C工具链和静态库。特权验收另需 systemd、sudo，且 cpu/memory 控制器可沿祖先委派。工具/权限不足时报告具体阻碍，不修改启动参数或全局安全策略。

在仓库根目录执行：

```bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j2
ctest --test-dir build/debug --output-on-failure
```

CTest 1/1 是 V0 CLI 回归，不代表完整特权验收。默认构建还生成公共头编译检查、真实资源库/探针和 attach 拒绝测试二进制；不下载测试框架。`minic --version` 仍保留历史 S02 字样，实际版本以 Git SHA 为准。

## 准备 Rootfs

仅在目标尚不存在时执行：

```bash
./scripts/prepare-rootfs.sh --output "$PWD/.local/isolation-rootfs"
./scripts/build-rootfs-workloads.sh --rootfs "$PWD/.local/isolation-rootfs"
```

第一条验证 BusyBox 的 ELF 架构与静态链接，创建 bin、dev、proc、tmp 和 `.minic-rootfs` 标记。第二条安装经静态构建的 `/bin/minic-workload`。已有完整 Rootfs/负载就跳过，不删除未知目录来强行重建。脚本拒绝既有目标、危险软链接或系统路径；当前准备助手只接受项目 .local 下专用新目录，动态 BusyBox 不会自动补齐加载器和库。

运行时在子 Mount namespace 中给 /dev 挂载临时 tmpfs 并创建 null 字符设备，proc 也在子 namespace 内挂载；退出后释放挂载，Rootfs 文件资产保留。

## 三种模式与运行

| 模式 | 行为 | 权限/资源参数 |
|---|---|---|
| v0 | fork/exec 宿主程序 | 不做隔离和限额；普通用户运行 |
| isolate | clone PID/UTS/Mount、Rootfs、hostname、proc | 普通终端 sudo；拒绝资源参数，明确非完整V1 |
| full（RunConfig 默认） | isolate加本轮请求的全部 cgroup 限额 | 已准备的可写委派目录；显式指定正内存和CPU，失败不降级 |

V0 示例：

```bash
./build/debug/minic run --mode v0 --evidence-dir "$PWD/artifacts/manual-v0" -- /bin/true
```

隔离示例（宿主先查看 hostname，结束后再对照）：

```bash
hostname
sudo ./build/debug/minic run --mode isolate \
  --rootfs "$PWD/.local/isolation-rootfs" --hostname minic-report \
  --evidence-dir "$PWD/artifacts/manual-isolate" -- \
  /bin/sh -c 'hostname; echo "PID=$$"; ps; ls -a /'
hostname
```

容器 hostname 应为 minic-report，shell 的容器PID为1，ps读取新proc，根目录有 `.minic-rootfs`；不要求ps只有一行。stdin 接 /dev/null，stdout/stderr 继承终端；参数以 argv 传递，CLI 不拼接 shell，示例中的 shell 是用户显式指定的负载。

full 的真实入口参数示意如下；CGROUP_BASE 必须是**本轮已核实的委派 jobs 目录**，不能照抄别人的 scope 路径。当前委派脚本不支持通用 `-- COMMAND`，普通终端验证应使用下一节已实现的 `--acceptance`。

```bash
: "${CGROUP_BASE:?先在本轮委派环境确认 jobs 绝对路径}"
sudo ./build/debug/minic run --mode full \
  --rootfs "$PWD/.local/isolation-rootfs" --hostname minic-full \
  --cgroup-base "$CGROUP_BASE" --memory 67108864 --swap-max 0 \
  --cpu-quota 20000 --cpu-period 100000 \
  --evidence-dir "$PWD/artifacts/manual-full" -- /bin/hostname
```

内存/swap单位为字节，CPU quota/period单位为微秒；20000/100000约为单核20%时间额度。period默认100000，CLI支持1000..1000000；swap0表示禁止该叶子的swap使用。full要求内存和CPU显式给出，swap按实际验收策略指定。不将有root权限、cgroup挂载可写或控制器存在当作委派已成功。

## 原生统一验收与资源探针

以下从**普通用户终端**启动，脚本内部调用sudo/systemd-run，不在外面再套sudo：

```bash
./scripts/with-delegation.sh --acceptance
```

它建立专用 `minic-b3-*.scope`，独立 manager 叶子和空 jobs 父节点，启用 cpu/memory，再运行12项测试。结果写 `artifacts/b3/native-*/verification.json`，各例保存 result.json、stdout/stderr。覆盖限额读回、CPU节流、64MiB/swap0下96MiB负载OOM、启动失败不放行、SIGINT/SIGTERM及后代回收。总退出非0或中途错误不能算通过。

独立资源模块入口：

```bash
./scripts/with-delegation.sh --probe all
```

这是B资源探针，不替代full验收。Agent沙箱的只读挂载、EPERM或no_new_privileges与虚拟机普通终端分别记录。已有合格验收无需因纯文档变化重跑；源码/脚本或演示有实质变化才复验相关项。

## 结果与错误

摘要写 evidence-dir/result.json，含 run_id、宿主PID、kind、exit_code/signal、主错误、实际启用功能、快照与清理结果。日志写stderr。正常负载保留0–255；信号用128+signal；启动/配置失败125；exec路径类错误127，其他exec错误126。负载自己也可退出125/127，必须看kind和step。收尾失败独立保存，不覆盖负载原因；无法写证据时非零并报告。

CPU/OOM依据删除前的 cpu.stat/memory.events 和精确配置读回，不能只看top或任意SIGKILL。完整语义见 [契约](docs/contracts.md)，代码顺序见 [架构](docs/architecture.md)。契约开头的“S00尚未实现”属于旧进度文字，当前实现与版本以本README、实际代码和最新台账为准，接口语义不变。

## 限制与汇报入口

只运行可信、有限、非守护化负载，允许负载直接作为namespace PID1；没有init包装，不保证PID1响应普通SIGTERM。CLI中断时在正常控制流终止并回收自己的init/后代。没有daemon、网络、Seccomp、TTY、镜像管理、User namespace或pivot_root；chroot不是完整防逃逸安全边界，也不承诺运行器SIGKILL、掉电后自动恢复。

历史V0的ASan在禁用LeakSanitizer后通过；LSan在当时环境初始化失败，**未证明泄漏检测通过**，也不能把V0工具结果扩展为完整V1内核资源验证。详见 [B的S02记录](docs/evidence/B/S02/report.md)。

[A单人汇报提纲](docs/report-outline.md) · [A完整讲解稿](docs/presentation-A.md) · [阶段台账](docs/stages.md) · [A的GDB](docs/evidence/A/S02/gdb-record.md) · [A的strace](docs/evidence/A/isolation/strace-record.md)。11月4日由A在Ubuntu独自完成6分钟汇报；B提供资源/工具材料、技术审校和台下备用支持，开发与实机操作归属保持真实。彩排/录像实际完成前保持待办。build、artifacts、Rootfs、主机私密配置不入Git。
