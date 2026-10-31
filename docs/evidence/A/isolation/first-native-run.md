# 首轮原生隔离验收与脚本修正

A 在普通 Ubuntu 终端执行 `sudo python3 docs/evidence/A/isolation/verify.py`，被测实现 SHA 为 b5fab92ee0207d8d2f037a56a9d475b415d46721，记录 dirty=false。原始证据保留在 artifacts/isolation/native，不覆盖。

verification.json 已保存缺 Rootfs、live、三轮含 exec 失败、strace 注入 mount EPERM 的六项预期返回；注入项是模拟故障。live 的 hostname/PID 输出及 namespace 链接检查已执行，但原脚本串联 test 后没有 set -e，不能仅凭整体退出 0 声称两个 marker 断言都成立；修正版强化后复验。

在 interrupt-SIGINT 发送信号前，脚本断言 `workload did not reach descendant wait`。该用例 result.json 为 exited/0、cleanup.complete=true，说明负载已提前正常退出，不是已验证中断回收。SIGTERM 用例未执行。全套不能记通过。

可能原因是最小 Rootfs 中没有 /dev/null，而 shell 非交互后台任务可能自行打开 /dev/null。原脚本在断言前没有保存该用例 stderr，所以当前不能将此原因写为已证实。

修正仅涉及 A 验收脚本：后台 sleep 显式 `<&0` 继承 Runtime 已连接的 stdin；等待失败也保存 stdout/stderr、JSON/FAIL 并尝试终止/回收自己的运行器；采样容忍 /proc 退出竞态；每次运行使用独立带时间目录；live 加 set -e，确保 marker 断言失败不会被后续 sleep 掩盖；注入标记立即落盘。不修改 Runtime，不重跑 S02 GDB。修正版尚待 A sudo 复验。

A 已成功推送隔离分支；PR 尚未创建，模块尚未最终验收。不合并或接入 full。
