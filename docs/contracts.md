# 公共契约初稿（S00，待 B 在 S01 审查及双方确认）

精确 C++17 声明以 include/minic/{config,result,runtime,cgroup}.hpp 为准。
所有声明尚未实现；调用 Runtime 或 cgroup 方法会缺少链接定义，不能当成功功能使用。

## 类型与调用

`Runtime::run(const RunConfig&) -> RunResult` 同步完成一次运行；调用方持有配置，Runtime 不保留配置引用。
`CgroupSession::create(const std::filesystem::path&, const std::string&, const ResourceLimits&) -> Outcome<CgroupSession>`；
`attach(pid_t) -> Status`；`snapshot() const -> Outcome<CgroupSnapshot>`；`cleanup() -> Status`。
`Outcome<T> = std::variant<T, Error>`，`Status = Outcome<std::monostate>`；monostate 为成功，Error 为失败。
Error 的 step 为稳定步骤名，errno_value 保存原始 errno（逻辑校验错误为 0），message 描述上下文。
预期的系统与配置错误经返回值报告，不用异常表达；内存分配等 C++ 异常由最终 CLI 边界捕获为 125。

create 只接受已准备好的委派基目录和单个安全路径分量 run_id，不操作系统根层级；目录必须独占创建，冲突即错误，不能复用或删除他人目录。
部分创建失败时自行回滚；回滚错误也写入 Error.message，不吞掉原始错误。
会话不可复制、可移动构造、不可移动赋值；父进程唯一持有，移动后源对象无资源。
cleanup 显式报告错误，成功后重复调用成功；失败保留所有权供重试；不杀无关进程。
snapshot 在删除之前调用；任何必需文件读取失败返回 Error，不能伪造空快照成功。
析构 noexcept 仅做兜底；无法报告的析构不能替代显式 cleanup。

RunConfig.mode 默认为 full。rootfs/cgroup_base/evidence_dir 使用绝对路径；argv 为逐项参数，禁止拼 shell 命令。
v0 只执行宿主程序；isolate 仅 PID/UTS/Mount、Rootfs；full 加本轮请求的全部 cgroup 限额，失败绝不降级。
v0/isolate 明确输出“非完整 V1”，拒绝与模式不符的资源参数，避免静默忽略。
内存与 swap 为 uint64 字节；memory 必须大于 0，swap 可为 0；缺省表示未请求，不等于无限额已验证。
CPU quota/period 为微秒，period 默认 100000，quota 必须正；不得用百分比或字节解释。
quota 可大于 period（多核额度）；支持的内核范围由 B 验证，超范围报告错误。
full 验收显式指定内存、CPU 与 swap 策略；swap 有请求而不支持即失败。v0 可省 Rootfs、hostname、cgroup_base；isolate 可省 cgroup_base。
所有模式要求非空 argv 与证据目录，isolate/full 要求专用有效 Rootfs、非空 hostname；full 要求已验证的委派目录。
stdin 为 /dev/null，stdout/stderr 继承终端，无交互代理。

## 结果、错误和 shell 状态

RunResult.kind 为 exited/signaled/startup_error/cleanup_error。
exit_code 仅在实际负载正常退出时有值（0–255）；signal 仅在信号终止时有值；host_pid 仅在成功创建子进程后有值。
primary_error、snapshot_error、cleanup.errors、evidence_errors 独立保存，后续错误不能覆盖首个主失败。
enabled 只写实际成功启用的隔离与限制；未请求/未设置的可选额度保持空。
cleanup.attempted 标记开始清理，complete 仅在所有本轮资源确认释放后为真。

| 情况 | kind | CLI 状态 |
|---|---|---|
| 负载正常退出，清理成功 | exited | 原退出值 0–255 |
| 负载被信号终止 | signaled | 128+signal |
| 校验/管道/创建/同步/cgroup/等待错误 | startup_error | 125 |
| exec 的 ENOENT/ENOTDIR（含缺 loader） | startup_error，step=exec | 127 |
| exec 的其他错误，如 EACCES/ENOEXEC | startup_error，step=exec | 126 |
| 负载成功但快照/清理/证据持久化失败 | cleanup_error | 125 |
| 负载非零或信号，且清理失败 | 保留 exited/signaled | 保留负载状态，另报清理错误；验收失败 |

snapshot/证据失败视为运行收尾失败，详细位置保存于对应字段；不将其编造为负载失败。
负载自身退出 125/126/127 仍是 exited，必须结合 JSON，不能只读 `$?`。
错误 step 约定：validate、pipe、fork、clone、cgroup.create、cgroup.attach、sync、mount.private、hostname、rootfs、proc、exec、wait、snapshot、cleanup、evidence。
S00 对未支持 CLI 请求返回 125 并输出骨架说明，不产生运行摘要。

## result.json（S02 起实现）

schema_version=1；字段：run_id、mode、kind、host_pid、exit_code、signal、primary_error、snapshot_error、enabled、cleanup、snapshot、evidence_errors。
optional 无值写 null；枚举用上述小写字符串；Error 为 {step, errno_value, message}；路径为字符串；整数不可写成浮点。
enabled 包含 pid_namespace/uts_namespace/mount_namespace/rootfs/cgroup 与 limits；limits 对应公共头四个同名字段。
cleanup 为 {attempted, complete, errors}；snapshot 为 {path, files}，files 存文件名到原始文本的映射，保留内核计数器单位。
必需快照文件：memory.events、memory.current、cpu.stat、cpu.max、memory.max、cgroup.events；memory.swap.max 可用时保存，请求 swap 时必须读取。
无法写 result.json 时 stderr 明确 evidence 错误且返回非零，不声称存在 JSON；日志只写 stderr。
测试证据还记录 CASE_ID、时间、系统、完整代码 SHA、命令、预期/实际、状态与证据路径，详见 workflow。

## 手册共同语义与资源边界


S00 创建可编译声明与文档，先固定以下语义及实际 C++ 签名，再开始实现；这里只定义语义，不提供代码实现。避免为占位而返回成功。

- `RunConfig`：阶段模式 `v0/isolate/full`、Rootfs 绝对路径、hostname、argv、可选内存字节数、swap 字节数、CPU quota/period（微秒）、cgroup 委派基目录、证据目录。`v0/isolate` 仅供阶段测试，运行时显式打印“非完整 V1”；默认最终验收使用 `full`。不得在 full 失败后自动降级。
- `RunResult`：`kind = exited/signaled/startup_error/cleanup_error`，独立保存退出值或信号、失败步骤、errno/消息、宿主 PID、run_id、实际启用的隔离/限额、清理结果。保留主失败和清理失败两类信息，不互相覆盖。运行摘要写 `result.json`，日志写 stderr。
- CLI：`minic run --mode v0|isolate|full --rootfs PATH --hostname NAME --cgroup-base PATH [--memory BYTES] [--swap-max BYTES] [--cpu-quota US --cpu-period US] --evidence-dir PATH -- PROGRAM ARGS...`。模式不需要的参数可省略；full 要求本轮指定的限额全部成功。`--cpu-period` 默认 100000 微秒，quota 必须为正；full 模式由验收配置明确给出内存和 CPU，不依赖隐含默认值。
- Runtime 对外 `run(config) -> RunResult`；负责验证、生成唯一 run_id、clone、同步、调用 cgroup、错误通信、等待、快照与回滚。非交互负载继承 stdout/stderr，stdin 连接 `/dev/null`；不实现终端代理。
- cgroup 抽象由 A 声明，B 实现：`create(base, run_id, limits)`、`attach(pid)`、`snapshot()`、`cleanup()` 的真实签名及错误类型写入契约；父进程唯一持有 move-only 会话。析构只作不抛异常兜底，显式 cleanup 必须返回可报告结果。模块不自行创建进程、不解析 CLI、不修改系统根层级。
- 错误与退出码：正常负载退出沿用 0–255；信号退出用 128+signal；运行器启动/配置失败用 125，exec 路径/格式等失败用 126/127（具体分类先写表），清理失败在负载成功时用 125。负载本身也可能返回这些值，因此必须结合 `result.kind` 和步骤判断，不能只看 shell `$?`。信号与清理失败同时存在时保留原终止原因并报告 cleanup 失败，验收仍未通过。
- exec 错误管道写端使用 close-on-exec；结构化错误记录有步骤和 errno。EOF 只是一个信号，必须结合子进程终止状态判定，不能把“早死但未写错误”自动算 exec 成功。检查短读写、EINTR、意外 EOF；控制端消失时子进程不执行工作负载。

### 4. 资源所有权与 V1 边界

- 父进程：clone 栈直到子进程回收；管道相应端；cgroup 会话；唯一 run_id；退出摘要。子进程：自己的管道端、namespace 内挂载与 cwd。所有 fd 有 RAII，exec 前关闭不需要的 fd。
- `clone` 采用独立回调栈，不用 CLONE_VM/CLONE_FILES/CLONE_THREAD；PID/UTS/Mount 模式必须在真实 Linux 运行，先将 mount propagation 设为 private，再修改挂载。父进程的 host namespace 不得被改变。
- 子进程等待放行后才配置 Rootfs 和 exec。full 模式父进程必须先创建、设置并加入 cgroup，再放行；失败终止并 wait，不能先运行再补限额。
- Rootfs 使用专用工作目录；chroot 后 chdir 到 `/`，在新 PID namespace 内挂载 proc。运行前关闭不需要的宿主目录 fd，不提供额外宿主目录绑定。只保证约定演示，不能宣称防逃逸。
- V1 允许负载直接为 namespace PID 1，不做 init 包装。只运行可信、有限、非守护化测试；容器后代处理依赖 PID namespace 生命周期，必须测试后代随 PID 1 终止被清除。PID 1 不保证响应普通 SIGTERM。CLI 收到 SIGINT/SIGTERM 时在正常控制流内终止并回收；可对自己的 namespace init 使用 SIGKILL 作为有界兜底，不能在信号处理器里执行复杂清理。
- 覆盖正常退出、准备失败、exec 失败、限额失败、CLI 中断；不承诺运行器被 SIGKILL、机器掉电后的自动恢复。记录手工诊断/清理方法，只操作确认属于本项目的资源。
- cgroup 使用 systemd 委派子树。管理进程在独立 manager 叶子，空父节点启用 cpu/memory，负载在各自叶子；先确认控制器沿父级可用。不将“有 root 权限”视为所有控制器都可用。
- 在清除 cgroup 前保存 memory.events、memory.current、cpu.stat、cpu.max、memory.max、可用时的 memory.swap.max 与 cgroup.events；OOM 已发生后再检查被删除路径是不成立的。snapshot 失败也要报告。
