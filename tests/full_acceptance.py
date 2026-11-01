#!/usr/bin/env python3
"""B-3 native full acceptance; run through scripts/with-delegation.sh --acceptance."""
import datetime
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import time

repo = Path(__file__).resolve().parents[1]
os.chdir(repo)
assert os.geteuid() == 0, 'needs root for clone/mount'
relative = next(line[3:] for line in Path('/proc/self/cgroup').read_text().splitlines() if line.startswith('0::'))
assert re.fullmatch(r'minic-b3-[0-9]+-[0-9]+\.scope', Path(relative).name), 'requires own fresh minic-b3 scope'
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
output=repo/'artifacts/b3'/datetime.datetime.now().strftime('native-%Y%m%d-%H%M%S')
output.mkdir(parents=True)
sha=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
dirty=bool(subprocess.check_output(['git','status','--porcelain'],text=True).strip())
rows=[]
rootfs=repo/'.local/isolation-rootfs'
hostname=os.uname().nodename
mounts=Path('/proc/self/mountinfo').read_text()

def run(case,argv,status,kind,mode='full',cgroup=jobs,step=None,binary='minic'):
    cmd=[str(repo/'build/debug'/binary),'run','--mode',mode,'--rootfs',str(rootfs),'--hostname','minic-full',
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
    if step: ok=ok and (result.get('primary_error') or {}).get('step')==step
    if mode=='full' and not step:
        files=(result.get('snapshot') or {}).get('files',{})
        ok=ok and result['enabled']['cgroup'] and bool(files)
        ok=ok and files.get('memory.max','').strip()=='67108864' and files.get('cpu.max','').strip()=='20000 100000'
        ok=ok and files.get('memory.swap.max','').strip()=='0'
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
run('exit125',['/bin/sh','-c','exit 125'],125,'exited')
run('exit127',['/bin/sh','-c','exit 127'],127,'exited')
run('missing',['/not-a-program'],127,'startup_error',step='exec')
# Configuration failure after clone must not execute the child payload.
marker='must-not-run-'+str(os.getpid())
run('create-failure',['/bin/sh','-c','echo ran >/tmp/'+marker],125,'startup_error',cgroup=rootfs,step='cgroup.create')
assert not (rootfs/'tmp'/marker).exists()
attach_marker='attach-must-not-run-'+str(os.getpid())
attach,_=run('attach-failure',['/bin/sh','-c','echo ran >/tmp/'+attach_marker],125,
             'startup_error',step='cgroup.attach',binary='minic_attach_rejection')
assert not (rootfs/'tmp'/attach_marker).exists()
assert not attach['enabled']['cgroup'] and attach['snapshot'] is not None
baseline,wall=run('cpu-baseline',['/bin/minic-workload','cpu','5'],0,'exited',mode='isolate')
cpu,wall=run('cpu-limited',['/bin/minic-workload','cpu','5'],0,'exited')
stats=dict(line.split() for line in cpu['snapshot']['files']['cpu.stat'].splitlines())
assert int(stats['nr_throttled'])>0 and int(stats['usage_usec']) < wall*1_000_000*.6, stats
memory,wall=run('memory-oom',['/bin/minic-workload','memory','96'],137,'signaled')
events=dict(line.split() for line in memory['snapshot']['files']['memory.events'].splitlines())
assert int(events['oom_kill'])>=1,events

# PID 1 launches a descendant and then waits. Send termination to the host CLI;
# the CLI must kill/reap namespace init, which also kills namespace descendants.
for sig in (signal.SIGINT, signal.SIGTERM):
    case='interrupt-'+sig.name.lower()
    evidence=output/case
    cmd=[str(repo/'build/debug/minic'),'run','--mode','full','--rootfs',str(rootfs),
         '--hostname','minic-full','--evidence-dir',str(evidence),'--cgroup-base',str(jobs),
         '--memory','67108864','--swap-max','0','--cpu-quota','20000','--cpu-period','100000',
         '--','/bin/sh','-c','sleep 30 & echo $! >/tmp/child-pid; wait']
    p=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,start_new_session=True)
    # Check the cgroup leaf, not a reused numeric PID or a timing-only sleep.
    deadline=time.monotonic()+5
    leaf=None
    while time.monotonic()<deadline:
        leaves=list(jobs.glob('*/'))
        if leaves:
            leaf=leaves[0]
            if len((leaf/'cgroup.procs').read_text().split())>=2: break
        time.sleep(.02)
    else:
        p.kill(); p.communicate(timeout=5)
        raise AssertionError(case+' never showed init and descendant in cgroup')
    members=(leaf/'cgroup.procs').read_text().split()
    p.send_signal(sig)
    try: out,err=p.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        os.killpg(p.pid,signal.SIGKILL); p.communicate(timeout=5)
        raise AssertionError(case+' did not terminate in five seconds')
    result=json.loads((evidence/'result.json').read_text())
    evidence.joinpath('stdout.log').write_text(out)
    evidence.joinpath('stderr.log').write_text(err)
    ok=(p.returncode!=0 and result['cleanup']['complete'] and result['snapshot'] is not None
        and not leaf.exists() and all(not Path('/proc/'+member).exists() for member in members))
    rows.append(dict(CASE_ID=case,command=cmd,sha=sha,dirty=dirty,signal=sig.name,
                     running_members=members,actual=p.returncode,result=result,
                     status='PASS' if ok else 'FAIL'))
    (output/'verification.json').write_text(json.dumps(rows,indent=2)+'\n')
    print(case,rows[-1]['status'],flush=True)
    assert ok,(case,err,result)
assert not list(jobs.glob('*/')), 'job cgroups remain'
jobs.rmdir()
print('PASS: native full cases; artifacts at',output)
