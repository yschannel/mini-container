# S02 提交后复测摘要

被测实现 SHA：`46af9df64deaf85283ee11680d9639c0c69e69c7`；测试时工作区干净。

环境：Linux-7.0.0-38-generic-x86_64-with-glibc2.39

| CASE_ID | 状态 | 实际 |
|---|---|---|
| true | PASS | [0, 'exited'] |
| exit7 | PASS | [7, 'exited'] |
| missing | PASS | [127, 'startup_error'] |
| spaces | PASS | [0, 'exited'] |
| no-argv | PASS | [125, 'startup_error'] |
| bad-mode | PASS | [125, 'startup_error'] |
| full | PASS | [125, 'startup_error'] |
| payload125 | PASS | [125, 'exited'] |
| payload126 | PASS | [126, 'exited'] |
| payload127 | PASS | [127, 'exited'] |
| permission | PASS | [126, 'startup_error'] |
| signal | PASS | [143, 'signaled'] |
| stdin | PASS | [0, 'exited'] |
| interrupt2 | PASS | [137, {'schema_version': 1, 'run_id': '29-11420195560150', 'mode': 'v0', 'kind': 'signaled', 'host_pid': 30, 'exit_code': None, 'signal': 9, 'primary_error': None, 'snapshot_error': None, 'enabled': {'pid_namespace': False, 'uts_namespace': False, 'mount_namespace': False, 'rootfs': False, 'cgroup': False, 'limits': {'memory_bytes': None, 'swap_max_bytes': None, 'cpu_quota_us': None, 'cpu_period_us': 100000}}, 'cleanup': {'attempted': True, 'complete': True, 'errors': []}, 'snapshot': None, 'evidence_errors': []}] |
| interrupt15 | PASS | [137, {'schema_version': 1, 'run_id': '31-11420209251050', 'mode': 'v0', 'kind': 'signaled', 'host_pid': 32, 'exit_code': None, 'signal': 9, 'primary_error': None, 'snapshot_error': None, 'enabled': {'pid_namespace': False, 'uts_namespace': False, 'mount_namespace': False, 'rootfs': False, 'cgroup': False, 'limits': {'memory_bytes': None, 'swap_max_bytes': None, 'cpu_quota_us': None, 'cpu_period_us': 100000}}, 'cleanup': {'attempted': True, 'complete': True, 'errors': []}, 'snapshot': None, 'evidence_errors': []}] |
| repeat0 | PASS | [0, 'exited'] |
| repeat1 | PASS | [0, 'exited'] |
| repeat2 | PASS | [0, 'exited'] |
| repeat3 | PASS | [0, 'exited'] |
| repeat4 | PASS | [0, 'exited'] |
| repeat5 | PASS | [0, 'exited'] |
| repeat6 | PASS | [0, 'exited'] |
| repeat7 | PASS | [0, 'exited'] |
| repeat8 | PASS | [0, 'exited'] |
| repeat9 | PASS | [0, 'exited'] |

复现：`python3 docs/evidence/A/S02/verify.py`。逐例完整命令、时间和结果见本地 `artifacts/s02/verification.json`；输入命令见已提交的 verify.py。原始 JSON 和 stderr/stdout 保留本地，不以截图代替。

此摘要为实现提交之后的纯证据提交，不改变被测代码。CMake 源登记和本人 GDB 仍待完成。
