#!/usr/bin/env python3
"""S02 A's reproducible checks; not the S03 runner/CTest suite."""
import datetime
import errno
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import time

root = Path(__file__).resolve().parents[4]
os.chdir(root)
binary = root / 'build/s02/minic'
base = root / 'artifacts/s02'
base.mkdir(parents=True, exist_ok=True)
sha = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
rows = []
def check(name, argv, status, kind, step=None, err=None, stdout=None, options=None):
    directory = base / name
    command = [str(binary), 'run', '--evidence-dir', str(directory), '--mode', 'v0']
    command += options or []
    command += ['--'] + argv
    p = subprocess.run(command, capture_output=True, text=True, timeout=10)
    result = json.loads((directory / 'result.json').read_text())
    assert p.returncode == status, (name, p.returncode, p.stderr)
    assert result['kind'] == kind, (name, result)
    if step:
        assert result['primary_error']['step'] == step
        assert result['primary_error']['errno_value'] == err
    if stdout is not None:
        assert p.stdout == stdout, (name, p.stdout)
    pid = result['host_pid']
    assert pid is None or not Path(f'/proc/{pid}').exists(), (name, 'child still exists')
    (directory / 'stdout.log').write_text(p.stdout)
    (directory / 'stderr.log').write_text(p.stderr)
    rows.append(dict(CASE_ID=name, command=command, time=datetime.datetime.now().astimezone().isoformat(),
                     system=platform.platform(), code_sha=sha, expected=[status,kind,step,err],
                     actual=[p.returncode,result['kind'],result['primary_error']], status='PASS', evidence=str(directory.relative_to(root))))
check('true', ['/bin/true'], 0, 'exited')
check('exit7', ['/bin/sh','-c','exit 7'], 7, 'exited')
check('missing', ['/definitely-not-exist'], 127, 'startup_error', 'exec', errno.ENOENT)
check('spaces', ['/usr/bin/printf','%s','a b;$(false)'], 0, 'exited', stdout='a b;$(false)')
check('no-argv', [], 125, 'startup_error', 'validate', 0)
check('bad-mode', ['/bin/true'], 125, 'startup_error', 'validate', 0, options=['--mode','wrong'])
check('full', ['/bin/true'], 125, 'startup_error', 'validate', 0, options=['--mode','full'])
for n in [125,126,127]:
    check(f'payload{n}', ['/bin/sh','-c',f'exit {n}'], n, 'exited')
program = base / 'no-permission'
program.write_text('#!/bin/sh\nexit 0\n')
program.chmod(0o600)
check('permission', [str(program)], 126, 'startup_error', 'exec', errno.EACCES)
check('signal', ['/bin/sh','-c','kill -TERM $$'], 143, 'signaled')
check('stdin', ['/bin/cat'], 0, 'exited', stdout='')
# Interrupt the runner after the direct child is alive, without terminal process-group assumptions.
for sig in (signal.SIGINT, signal.SIGTERM):
    name = f'interrupt{sig.value}'
    directory = base / name
    p = subprocess.Popen([str(binary),'run','--mode','v0','--evidence-dir',str(directory),'--','/bin/sleep','30'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    children = Path(f'/proc/{p.pid}/task/{p.pid}/children')
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline and not children.read_text().strip(): time.sleep(.01)
    assert children.read_text().strip()
    p.send_signal(sig)
    out, error = p.communicate(timeout=5)
    result = json.loads((directory / 'result.json').read_text())
    assert p.returncode == 137 and result['kind'] == 'signaled', (name,p.returncode,result)
    assert not Path(f"/proc/{result['host_pid']}").exists()
    rows.append(dict(CASE_ID=name, command=p.args, time=datetime.datetime.now().astimezone().isoformat(), system=platform.platform(), code_sha=sha,
                     expected='runner interrupted; direct child terminated and reaped', actual=[p.returncode,result], status='PASS', evidence=str(directory.relative_to(root))))
for i in range(10): check(f'repeat{i}', ['/bin/true'],0,'exited')
(base / 'verification.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2)+'\n')
print(f'{len(rows)} checks PASS; code_sha={sha}; artifacts/s02/verification.json')
