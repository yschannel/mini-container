#!/usr/bin/env python3
"""Temporary A integration evidence runner, not B's resource/delegation module.
Run only in a fresh minic-a2-*.scope with Delegate=cpu memory, as root.
"""
import datetime
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import time

repo = Path(__file__).resolve().parents[4]
os.chdir(repo)
assert os.geteuid() == 0, 'needs root for clone/mount'
relative = next(line[3:] for line in Path('/proc/self/cgroup').read_text().splitlines() if line.startswith('0::'))
assert re.fullmatch(r'minic-a2-[0-9]+\.scope', Path(relative).name), 'requires own fresh minic-a2 scope'
base = Path('/sys/fs/cgroup' + relative)
assert {'cpu','memory'} <= set((base/'cgroup.controllers').read_text().split())
manager = base/'manager'
manager.mkdir()
(manager/'cgroup.procs').write_text(str(os.getpid()))
assert not (base/'cgroup.procs').read_text().strip()
(base/'cgroup.subtree_control').write_text('+cpu +memory')
jobs=base/'jobs'
jobs.mkdir()
(jobs/'cgroup.subtree_control').write_text('+cpu +memory')
output=repo/'artifacts/integration'/datetime.datetime.now().strftime('native-%Y%m%d-%H%M%S')
output.mkdir(parents=True)
sha=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
dirty=bool(subprocess.check_output(['git','status','--porcelain'],text=True).strip())
rows=[]
rootfs=repo/'.local/isolation-rootfs'
hostname=os.uname().nodename
mounts=Path('/proc/self/mountinfo').read_text()

def run(case,argv,status,kind,mode='full',cgroup=jobs,step=None):
    cmd=[str(repo/'build/debug/minic'),'run','--mode',mode,'--rootfs',str(rootfs),'--hostname','minic-full',
         '--evidence-dir',str(output/case)]
    if mode=='full': cmd+=['--cgroup-base',str(cgroup),'--memory','67108864','--swap-max','0','--cpu-quota','20000','--cpu-period','100000']
    cmd+=['--',*argv]
    p=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,start_new_session=True)
    start=time.monotonic()
    try: out,err=p.communicate(timeout=20)
    except subprocess.TimeoutExpired:
        p.send_signal(signal.SIGTERM)
        try: out,err=p.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(p.pid,signal.SIGKILL)
            p.communicate(timeout=5)
        raise AssertionError('bounded command timed out: '+case)
    elapsed=time.monotonic()-start
    result=json.loads((output/case/'result.json').read_text())
    (output/case/'stdout.log').write_text(out)
    (output/case/'stderr.log').write_text(err)
    ok=p.returncode==status and result['kind']==kind and result['cleanup']['complete']
    if step: ok=ok and result['primary_error']['step']==step
    if mode=='full' and not step:
        ok=ok and result['enabled']['cgroup'] and result['snapshot'] is not None
        files=result['snapshot']['files']
        ok=ok and files['memory.max'].strip()=='67108864' and files['cpu.max'].strip()=='20000 100000'
        ok=ok and files['memory.swap.max'].strip()=='0'
    if result['snapshot']: ok=ok and not Path(result['snapshot']['path']).exists()
    if result['host_pid']: ok=ok and not Path(f"/proc/{result['host_pid']}").exists()
    ok=ok and os.uname().nodename==hostname and Path('/proc/self/mountinfo').read_text()==mounts
    rows.append(dict(CASE_ID=case,command=cmd,sha=sha,dirty=dirty,elapsed=elapsed,expected=[status,kind,step],
                     actual=p.returncode,result=result,status='PASS' if ok else 'FAIL'))
    (output/'verification.json').write_text(json.dumps(rows,indent=2)+'\n')
    print(case,rows[-1]['status'],flush=True)
    assert ok,(case,err,result)
    return result,elapsed

run('smoke',['/bin/sh','-c','set -e; test "$(hostname)" = minic-full; test "$$" = 1; test -c /dev/null; test -f /.minic-rootfs; ps'],0,'exited')
run('exit7',['/bin/sh','-c','exit 7'],7,'exited')
run('missing',['/not-a-program'],127,'startup_error',step='exec')
# Configuration failure after clone must not execute the child payload.
marker='must-not-run-'+str(os.getpid())
run('create-failure',['/bin/sh','-c','echo ran >/tmp/'+marker],125,'startup_error',cgroup=rootfs,step='cgroup.create')
assert not (rootfs/'tmp'/marker).exists()
baseline,wall=run('cpu-baseline',['/bin/minic-workload','cpu','5'],0,'exited',mode='isolate')
cpu,wall=run('cpu-limited',['/bin/minic-workload','cpu','5'],0,'exited')
stats=dict(line.split() for line in cpu['snapshot']['files']['cpu.stat'].splitlines())
assert int(stats['nr_throttled'])>0 and int(stats['usage_usec']) < wall*1_000_000*.6, stats
memory,wall=run('memory-oom',['/bin/minic-workload','memory','96'],137,'signaled')
events=dict(line.split() for line in memory['snapshot']['files']['memory.events'].splitlines())
assert int(events['oom_kill'])>=1,events
assert not list(jobs.glob('*/')), 'job cgroups remain'
jobs.rmdir()
print('PASS: full smoke/errors/CPU/OOM; artifacts at',output)
print('Native attach-failure/interruption and final two-system acceptance still require B unified runner.')
