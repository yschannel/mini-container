# B-1 S02 收尾交接（2026-10-31）

开发基线：`7583f36f5a16b5d3f7b108d4e28accecfc8df9aa`（`origin/a/s02-v0-runtime`）。
系统：openSUSE，Linux `7.3.0-1-default`，x86_64；GCC 16.2.1，Python 3.13.16。

## 改动与审查

- 标准 CMake 登记 `runtime.cpp`、`result_io.cpp`，保留公共头编译检查。
- 新增 CTest V0 CLI 回归，核对 shell 状态与 `result.json` 的 `kind` / `primary_error.step`，覆盖成功、exit 7、exec ENOENT/EACCES、空格参数、负载自身退出 125/127、stdin 和中断回收。
- 审查 A Runtime 的错误管道、等待/中断、参数传递和 fd 所有权：当前覆盖路径未发现阻断合并的核心缺陷；V0 仍只适用于可信的前台非交互负载。

## 实际验证

| 项目 | 命令 | 结果 |
|---|---|---|
| Debug 配置与构建 | `cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug && cmake --build build/debug -j2` | PASS，标准链接成功 |
| CTest | `ctest --test-dir build/debug --output-on-failure` | PASS，1/1，含 9 项 CLI 检查 |
| A 的既有脚本 | `cp build/debug/minic build/s02/minic && python3 docs/evidence/A/S02/verify.py` | PASS，25/25；本地原始结果 `artifacts/s02/verification.json` |
| 帮助与版本 | `build/debug/minic --help`; `build/debug/minic --version` | PASS，显示 S02 V0 范围 |
| ASan 构建 | `cmake -S . -B build/asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS='-fsanitize=address -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address' && cmake --build build/asan -j2` | PASS |
| ASan 默认测试 | `ctest --test-dir build/asan --output-on-failure` | FAIL：当前环境 LeakSanitizer 初始化报 fatal error，`true` 用例的程序结果为 exited/0，但进程最终状态被 LSan 改为 1 |
| ASan 不含 leak 检测 | `ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/asan --output-on-failure` | PASS，1/1；仅覆盖 AddressSanitizer，未验证 leak 检测 |

代码审查/初次构建和自动测试由 Agent 在 B 的 openSUSE 工作区执行。B 本人随后在同一工作区、提交 `894c34d` 亲手运行 Debug CMake 配置/构建与 CTest（1/1 PASS），缺失程序用例（shell 127；`result.json` 为 `startup_error`、`primary_error.step=exec`、errno 2、清理完成），以及 `ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/asan --output-on-failure`（1/1 PASS）。B 提供了实际终端输出；该 ASan 运行未覆盖 leak 检测。B 的人工检查点据此完成。

A 的 Ubuntu 最短复验命令：

```bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j2
ctest --test-dir build/debug --output-on-failure
```

Ubuntu 结果与 A 对构建/测试改动的审查：NOT_RUN / 待 A。PR：`https://github.com/yschannel/mini-container/pull/2`；合并与 main SHA 待 A。本记录不把缺失的双机结果记为 PASS。
