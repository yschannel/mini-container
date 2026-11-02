# 11 月 4 日：A 单人汇报的 Ubuntu 短演示参考

总汇报 6 分钟，由 A 独立安排讲稿与操作；本页只提供可复用的短演示和资源证据。现场使用 Ubuntu，不运行 `./scripts/with-delegation.sh --acceptance` 的完整 12 项。B 不接话。已有两机原生验收与 B 彩排保留在下方，和现场新运行分开说明。

## 现场操作（建议占总时长约 60–90 秒）

前提：A 的 Ubuntu 已有本机 Debug 构建 `build/debug/minic`、Python 3；从仓库根目录执行。操作仅用现有 V0 命令，不需 sudo、委派、Rootfs 或临时安装依赖。V0 是 CLI/结果分类演示，**不代表现场运行了 full 隔离与资源限制**。

```bash
git rev-parse --short HEAD
demo_dir="$(mktemp -d /tmp/minic-talk-XXXXXX)"
./build/debug/minic run --mode v0 --evidence-dir "$demo_dir/exit7" -- /bin/sh -c 'exit 7'
echo "shell_exit=$?"
python3 -m json.tool "$demo_dir/exit7/result.json"
```

预期：shell 退出 7；`result.json` 的 `mode=v0`、`kind=exited`、`exit_code=7`。A 可借此说明 CLI 退出值和结构化结果分开保存。若需要一个启动错误对照，可只展示 [Ubuntu 12 项记录](evidence/A/integration/ubuntu-97800f0.md)中 `missing` 的 `startup_error/exec/ENOENT`，不增加现场故障命令。运行前检查本机二进制确由准备展示的提交构建；`git rev-parse` 只说明源码提交，不能单独证明二进制来源。

随后展示预先打开的 [Ubuntu 资源快照记录](evidence/A/integration/ubuntu-97800f0.md)，或 A 本机原始 `artifacts/b3/native-20261101-173747/verification.json`（若仍在）。该记录明确属于干净候选 `97800f0244b807466e7c516021f3c40b882abf45` 的历史实机运行：CPU `cpu.max=20000 100000`、墙钟 5.076948 秒、`usage_usec=1021486`、`nr_throttled=51`；OOM 为 `memory.max=67108864`、`memory.swap.max=0`、本轮叶子 `oom_kill=1`、signal 9。说明先采快照再删叶子，不能在删除后现场读原 cgroup 路径；也不能把历史快照说成刚才 V0 命令产生。

若时间足够，只指向同一记录中的 `attach-failure` 与中断回收结论：attach 拒绝由**测试构建**将 PID 换为 `INT_MAX`，真实内核返回 ESRCH，不是生产二进制自然故障；两种中断的运行期成员及叶子均由 runner 检查清理。CTest 是非特权 V0 回归，ASan 关闭泄漏检测后的通过记录不证明泄漏检测或特权资源路径正确。A 自行决定在统一 6 分钟讲稿中保留哪些内容。

## 现场失败时

V0 命令或投屏有问题时，保留真实错误和退出码，直接展示已提交的 [Ubuntu 原生记录](evidence/A/integration/ubuntu-97800f0.md)及 [openSUSE 对照记录](evidence/B/B3/report.md)，注明均为 2026-11-01 同候选历史验证，不冒充现场结果。不为恢复演示临时安装依赖，也不启动完整 12 项。A 若有自己的录屏，可以注明拍摄时间与来源。

## B 的历史彩排（供 A 判断稳定性，不安排 B 发言）

2026-11-02 B 在 openSUSE 普通终端、干净 `f2a08e6f71df86508634f7642da8d316060db37b` 上首次运行统一入口：`artifacts/b3/native-20261102-191644/verification.json` 为 8 PASS、`cpu-limited` 1 FAIL、后续 3 NOT_RUN。该项当时退出 0、限额读回 `20000 100000`、约 5.13 秒墙钟内 `usage_usec=1006722`、`nr_throttled=51`，清理标记完整；runner 将多条检查合为一个布尔值，日志未定位具体失败条件，不能改记 PASS。

随后两次原始汇总 `artifacts/b3/native-20261102-191712/verification.json`（SHA-256 `6a07ec1c37021c3995b14b06ac0018a16e9c32bfefe0e9c9de5b7e628a2c4f85`）与 `artifacts/b3/native-20261102-191728/verification.json`（SHA-256 `7c28678771cdcf3e5edd959c4d0d0721c5e506b6b76c561a20ef371fd1929da4`）均记录同一干净提交、12/12 PASS；第二次终端原文另确认 scope `minic-b3-1793618248-11747.scope`、总 `exit=0`。首次偶发 FAIL 原因仍未定位。这些是 B 的既有彩排，不作为 11 月 4 日 Ubuntu 现场演示结果。
