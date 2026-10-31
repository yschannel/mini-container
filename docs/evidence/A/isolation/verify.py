#!/usr/bin/env python3
"""A-1 privileged Linux checks. Writes only artifacts/isolation and its own host marker."""
import datetime
import errno
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

repo = Path(__file__).resolve().parents[4]
os.chdir(repo)
base = repo / 'artifacts/isolation/native'
base.mkdir(parents=True, exist_ok=True)
rootfs = repo / '.local/isolation-rootfs'
sha = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
dirty = bool(subprocess.check_output(['git', 'status', '--porcelain'], text=True).strip())
rows = []
if os.geteuid() != 0:
    sys.exit('Run in ordinary Ubuntu terminal: sudo python3 docs/evidence/A/isolation/verify.py')
marker = base / 'host-only-marker'
marker.write_text('host-only\n')
hostname = os.uname().nodename
mounts = Path('/proc/self/mountinfo').read_text()

def command(case, args, fs=rootfs, hostname='minic-a1'):
    return [str(repo/'build/debug/minic'), 'run', '--mode', 'isolate', '--rootfs', str(fs),
            '--hostname', hostname, '--evidence-dir', str(base/case), '--', *args]

def record(case, cmd, rc, data, expected, status='PASS'):
    rows.append(dict(CASE_ID=case, command=cmd, time=datetime.datetime.now().astimezone().isoformat(),
        sha=sha, dirty=dirty, system=os.uname().release, expected=expected, actual_exit=rc, result=data, status=status))
    (base/'verification.json').write_text(json.dumps(rows, indent=2)+'\n')

def run(case, args, rc, kind, step=None, fs=rootfs, prefix=None):
    cmd = (prefix or []) + command(case,args,fs)
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
    data = json.loads((base/case/'result.json').read_text())
    (base/case/'stdout.log').write_text(p.stdout)
    (base/case/'stderr.log').write_text(p.stderr)
    # Permission failures are environment blockers, not functional passes.
    error = data['primary_error']
    if error and error['step'] == 'clone' and error['errno_value'] in (errno.EPERM,errno.EACCES):
        record(case,cmd,p.returncode,data,'namespace creation available','SKIP')
        print('SKIP: clone permission denied; remaining privileged checks NOT_RUN', flush=True)
        sys.exit(2)
    ok = p.returncode == rc and data['kind'] == kind and (not step or error and error['step'] == step)
    record(case,cmd,p.returncode,data,[rc,kind,step], 'PASS' if ok else 'FAIL')
    assert ok, (case,p.stderr,data)
    if data['host_pid']:
        assert not Path(f"/proc/{data['host_pid']}").exists(), 'direct child remains'
    assert os.uname().nodename == hostname, 'host hostname changed'
    assert Path('/proc/self/mountinfo').read_text() == mounts, 'host mountinfo changed'
    return p,data

# Runtime validation does not require successful clone.
run('missing-rootfs',['/bin/true'],125,'startup_error','rootfs',fs=repo/'.local/no-such-isolation-rootfs')
script = f'hostname; echo PID=$$; ps; ls -a /; test -f /.minic-rootfs; test ! -e {marker}; sleep 3'
case = 'live'
cmd = command(case,['/bin/sh','-c',script])
p = subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
children = Path(f'/proc/{p.pid}/task/{p.pid}/children')
child = None
for _ in range(200):
    if p.poll() is not None: break
    ids = children.read_text().split()
    if ids: child = int(ids[0]); break
    time.sleep(.01)
if child:
    ns = {name: dict(host=os.readlink(f'/proc/self/ns/{name}'), child=os.readlink(f'/proc/{child}/ns/{name}')) for name in ('pid','uts','mnt')}
    assert all(x['host'] != x['child'] for x in ns.values()), ns
    (base/'live-namespaces.json').write_text(json.dumps(dict(host_pid=child,namespaces=ns),indent=2)+'\n')
out, err = p.communicate(timeout=10)
data = json.loads((base/case/'result.json').read_text())
(base/case/'stdout.log').write_text(out)
(base/case/'stderr.log').write_text(err)
error = data['primary_error']
if error and error['step'] == 'clone' and error['errno_value'] in (errno.EPERM,errno.EACCES):
    record(case,cmd,p.returncode,data,'namespace available','SKIP')
    print('SKIP: clone denied; native isolation NOT_RUN; see verification.json'); sys.exit(2)
ok = p.returncode == 0 and 'PID=1\n' in out and out.startswith('minic-a1\n')
record(case,cmd,p.returncode,data,'hostname=minic-a1; PID=1; rootfs marker; host marker absent', 'PASS' if ok else 'FAIL')
assert ok,(out,err,data)
assert child and not Path(f'/proc/{child}').exists()
assert os.uname().nodename == hostname and Path('/proc/self/mountinfo').read_text() == mounts
# Three additional rounds, including exec failure.
run('round1',['/bin/true'],0,'exited')
run('round2-missing-program',['/missing-program'],127,'startup_error','exec')
run('round3-exit7',['/bin/sh','-c','exit 7'],7,'exited')
# Simulated syscall failure tests rollback in real namespaces; not a native capability result.
if subprocess.run(['strace','-V'],capture_output=True).returncode == 0:
    run('injected-prepare-failure',['/bin/true'],125,'startup_error','mount.private',
        prefix=['strace','-f','-o',str(base/'injected.strace'),'-e','inject=mount:error=EPERM:when=1'])
    rows[-1]['simulation'] = 'strace fault injection of mount EPERM'
for sig in (signal.SIGINT,signal.SIGTERM):
    case = f'interrupt-{sig.name}'
    cmd = command(case,['/bin/sh','-c','sleep 30 & wait'])
    p = subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    direct = None
    descendants = []
    for _ in range(200):
        if p.poll() is not None: break
        ids = Path(f'/proc/{p.pid}/task/{p.pid}/children').read_text().split()
        if ids:
            direct = int(ids[0])
            nested = Path(f'/proc/{direct}/task/{direct}/children')
            if nested.exists(): descendants = [int(x) for x in nested.read_text().split()]
            if descendants: break
        time.sleep(.01)
    assert direct and descendants, 'workload did not reach descendant wait'
    p.send_signal(sig)
    out,err=p.communicate(timeout=5)
    data=json.loads((base/case/'result.json').read_text())
    ok = p.returncode == 137 and data['kind'] == 'signaled' and data['cleanup']['complete']
    assert not any(Path(f'/proc/{pid}').exists() for pid in [direct,*descendants]), 'namespace descendants remain'
    record(case,cmd,p.returncode,data,'SIGKILL namespace init, all descendants gone', 'PASS' if ok else 'FAIL')
    assert ok,(err,data)
assert os.uname().nodename == hostname and Path('/proc/self/mountinfo').read_text() == mounts
(base/'verification.json').write_text(json.dumps(rows,indent=2)+'\n')
print(f'{len(rows)} cases PASS; injected case marked simulation; {base}/verification.json')
