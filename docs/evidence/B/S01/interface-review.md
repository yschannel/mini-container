# S01 公共接口审查（B）

基线 C00：`40040ab865ece11b1f7211ca37e01ac4c2979d0f`。

已逐项查看 `config.hpp`、`result.hpp`、`runtime.hpp`、`cgroup.hpp` 及 `docs/contracts.md`。`create(base, run_id, limits)`、`attach(pid_t)`、`snapshot() const`、`cleanup()` 的返回类型可表达成功与具体错误；`CgroupSession` 禁止复制和移动赋值，仅允许 noexcept 移动构造，符合父进程唯一持有会话的约定。内存/swap 单位为字节、CPU quota/period 为微秒。`RunResult` 分开保存主错误、快照错误、清理错误和证据错误；负载自身退出 125/126/127 与启动错误通过 `kind` 区分。

接口意见：当前签名足以开始 S02，无须 B 改动公共头。实现时需严格执行契约：`create` 部分失败必须回滚并报告，`cleanup` 失败时保留所有权供重试；`enabled.limits.cpu_period_us` 有非空默认值，序列化时不能仅凭该数值宣称已请求或启用 CPU 限额，应结合 `cpu_quota_us`。`snapshot()` 在 `cleanup()` 前读取必需文件；错误不能伪装空快照。`run_id` 只能是安全的单一路径分量。若 A 认为需要改签名，按 workflow 先提出变更说明、双方确认，由 A 维护公共头。

目录职责已核对：B 维护 CMake、测试与环境脚本；A 维护公共头、CLI 与 Runtime。S04/S05 并行时互不修改对方模块，C04 先合并，B 再同步主线并重测。

双方确认：B 审查完成；A 确认待完成。S02 不应在 A 确认及 S01 合并前开始。
