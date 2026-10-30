# 给 B 的 S02 最小构建登记清单

A 遵守 CMake 所有权，未改 CMakeLists.txt。本分支当前标准 CMake 入口尚未登记 Runtime，因此 CLI 链接不能通过；不能合并当前版本。请 B 在本阶段分支提交以下最小变更，由 A 拉取后运行标准配置/构建及重测：

```cmake
add_executable(minic src/main_cli.cpp src/core/runtime.cpp src/common/result_io.cpp)
```

替换原有 `add_executable(minic src/main_cli.cpp)`，其他选项不变，不添加 cgroup 或 namespace。
本阶段无需第三方库，公共头 OBJECT 检查保持。A 的临时等价 Debug 编译命令：

```bash
mkdir -p build/s02
c++ -std=c++17 -g -O0 -Wall -Wextra -Wpedantic -Iinclude \
  src/main_cli.cpp src/core/runtime.cpp src/common/result_io.cpp -o build/s02/minic
python3 docs/evidence/A/S02/verify.py
```

B 完成登记后，先执行手册的 CMake 配置/构建；可把 `build/debug/minic` 复制到 `build/s02/minic` 后复用 A 的记录脚本。该脚本只是 A 的可复现阶段记录，不替代 S03 的 CTest/runner。
