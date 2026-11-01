full Runtime 现在在 clone 子进程等待期间调用 B 的真实 CgroupSession：create 设置/读回限额，attach 成功后才放行；子进程隔离/exec，父进程 wait 后在删除前 snapshot，再 cleanup。创建/attach 失败不放行；主错误、快照和清理错误独立报告，JSON 保存实际限额和原始快照。

基于已合并 A-1 的 main 6088ccece75b450255c99ab269f059bdb2efe30e，集成 B 资源分支 d3ba16d。未改 B cgroup.cpp 或公共接口；CMake 仅将 minic 链接已有真实 cgroup 库。

验证：标准 Debug 构建、V0 CTest、9项明确标注的替身生命周期测试及 JSON 检查通过。A 在普通 Ubuntu 24.04/VMware 终端亲自执行 native_full.py，固定被测 SHA=d045d5939ffa315c00bbe716f39cc8cf0c16969a，7项 dirty=false、总退出0：smoke/exit7/missing/create-failure/cpu-baseline(isolate)/cpu-limited(full)/memory-oom(full) 均 PASS。CPU 读回20000/100000，5.073864秒墙钟内使用1.013068秒CPU、节流50次；内存64MiB/swap0，96MiB负载的独占叶子快照 oom_kill=1、signal9。快照在cleanup前，测试检查每轮进程/叶子已删除和宿主状态不变。

完整边界与精选原始计数见 docs/evidence/A/integration/native-verification.md、selected-snapshots.json、report.md。未重复 GDB/strace，不提交整个 artifacts。

待完成：本候选原生 full attach-failure、interruption及后代回收，以及最终 Ubuntu/openSUSE 同候选统一验收。替身/单模块/A-1结果不能替代。请 B 审查编排和错误回滚并协助统一验收；本PR不宣称完整双系统V1通过，不自动合并。若 B 资源分支有独立PR，请协调其合并顺序以免重复审查资源模块。
