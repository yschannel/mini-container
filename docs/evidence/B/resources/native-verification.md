# B-2 openSUSE resource probe

- Operator: B (user), in a normal openSUSE terminal; this record transcribes the terminal output supplied in chat. The agent did not enter the sudo password or run the privileged probe.
- System: openSUSE Tumbleweed 20261028, x86_64, kernel 7.3.0-1-default, systemd 261 (261.4).
- Development base: merged PR #3, main `6088ccece75b450255c99ab269f059bdb2efe30e`.
- Resource code/probe commit at the time of the run: `71dc77bc9a4e2cac9dff5cb83f78b4d2dc605095`. The terminal run preceded this commit; the cgroup implementation, original probe and delegation script content in this commit match the run. A static Rootfs workload and its build script were added afterward and separately built locally. The terminal run was on an uncommitted worktree, so it is **not** represented as an execution of a clean commit checkout. The later attach-failure probe change (positive nonexistent PID) needs its own run.
- Command: `./scripts/with-delegation.sh --probe all; echo $?`.
- Scope: `minic-b2-1793522239-54633.scope`, delegated base `/sys/fs/cgroup/system.slice/minic-b2-1793522239-54633.scope/jobs`.
- Shell exit: `0`; the scope path was absent in a subsequent read-only check.

| CASE_ID | Expected | Actual from terminal | Status |
|---|---|---|---|
| B2-SMOKE | Child in its own leaf, exits 0, snapshot before removal | Host PID 54656 in `minic-probe-54655`, exit 0, memory/cpu/events files printed | PASS |
| B2-ATTACH-FAIL | Invalid attach does not release blocked child | Original validation run: `invalid attach rejected=1 blocked child exit=80`; see kernel rejection rerun below | PASS |
| B2-CPU-BASELINE | About five seconds of unthrottled CPU | Wall 5.00042 s, `usage_usec=5000049`, `nr_throttled=0` | PASS |
| B2-CPU-LIMIT | Read back quota and observe throttling | `cpu.max=20000 100000`, wall 5.03794 s, `usage_usec=1014633`, `nr_throttled=51`, `throttled_usec=4022010` | PASS |
| B2-MEMORY | 64 MiB, swap 0, bounded 96 MiB write causes this leaf's OOM event | `memory.max=67108864`, `memory.swap.max=0`, child exit 137, `memory.events oom=1 oom_kill=1` | PASS |

The probe reads `cgroup.procs` before releasing the child, limit files after configuration, and `memory.events`/`cpu.stat` before cleanup. Its output is a resource module result, not a full Runtime/container acceptance result. B's personal interpretation of `memory.events` and `cpu.stat` is pending; do not mark that manual checkpoint complete on the agent's behalf.

## Kernel attach rejection rerun

- Tested clean commit: `a32188cf07de4345b0569cfe61f40699a59f96a3`.
- Operator: B (user), openSUSE normal terminal.
- Command: `./scripts/with-delegation.sh --probe attach-fail; echo $?`.
- Scope: `minic-b2-1793522524-58553.scope`; absent in subsequent read-only check.
- Actual: `invalid attach rejected=1 blocked child exit=80`, `RESULT=attach-fail PASS`, shell exit `0`.
- Status: PASS. The probe used positive nonexistent PID `INT_MAX`, so `attach()` reached the kernel write path. Closing the gate after rejection prevented the child from running the workload.
