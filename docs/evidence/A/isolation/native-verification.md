# A-1 Ubuntu 原生验收

被测实现 SHA：`66664782a71de911c9e5cd92e561e4c46e8ad1cb`。各项记录 dirty=false；A 在普通终端亲自 sudo 执行，不是 Agent namespace 探测。

环境：Ubuntu 24.04 / VMware，kernel 7.0.0-38-generic。

命令：`sudo python3 docs/evidence/A/isolation/verify.py`。

原始目录：`artifacts/isolation/native-20261031-162650/`；每例完整命令、时间、SHA、状态与 result.json 在 verification.json，各例 stdout/stderr 保留本地。

| CASE_ID | 状态 | CLI 状态 | 结果 |
|---|---|---|---|
| missing-rootfs | PASS | 125 | startup_error / rootfs / errno=2 |
| live | PASS | 0 | exited |
| round1 | PASS | 0 | exited |
| round2-missing-program | PASS | 127 | startup_error / exec / errno=2 |
| round3-exit7 | PASS | 7 | exited |
| injected-prepare-failure | PASS | 125 | startup_error / mount.private / errno=1；模拟故障注入 |
| interrupt-SIGINT | PASS | 137 | signaled |
| interrupt-SIGTERM | PASS | 137 | signaled |

live 输出显示 hostname=minic-a1、PID=1；proc 中显示 namespace 内 shell/ps，Rootfs 标记可见、宿主标记断言成立。新 /dev/null 字符设备和写入测试通过。

运行期宿主子进程 PID 为 42879；同一进程在容器内为 PID 1。namespace 对照：

```json
{
  "host_pid": 42879,
  "namespaces": {
    "pid": {
      "host": "pid:[4026531836]",
      "child": "pid:[4026532598]"
    },
    "uts": {
      "host": "uts:[4026531838]",
      "child": "uts:[4026532597]"
    },
    "mnt": {
      "host": "mnt:[4026531832]",
      "child": "mnt:[4026532596]"
    }
  }
}
```

三轮包含 exec 失败；每轮已检查直接子进程消失、宿主 hostname 和 /proc/self/mountinfo 不变。SIGINT/SIGTERM 用例先确认后代 sleep 存活，再中断 CLI，结果 137/signaled，直接子进程及采样到的后代均消失。

其中准备失败是 strace 注入 mount EPERM，仅证明该失败路径处理；其余隔离/中断项为真实运行。前两轮失败及修复见 first-native-run.md，不覆盖旧证据。

本次为 Ubuntu isolate 子集通过，不是 full/cgroup 或双系统 V1 通过。
