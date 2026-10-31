# A 本人 isolate strace 观察

A 在普通 Ubuntu 终端亲手执行下列命令并回传输出，未重复 S02 GDB。

```bash
mkdir -p artifacts/isolation/manual
sudo strace -ff -o "$PWD/artifacts/isolation/manual/trace" \
  -e trace=clone,read,write,sendto,mount,sethostname,chroot,chdir,mknod,mknodat,execve,wait4,kill \
  ./build/debug/minic run --mode isolate \
  --rootfs "$PWD/.local/isolation-rootfs" --hostname minic-trace \
  --evidence-dir "$PWD/artifacts/isolation/manual" -- /bin/hostname
```

终端实际输出：

```text
isolate PID/UTS/Mount/Rootfs；无 cgroup，非完整 V1
minic-trace
```

A 再亲手筛选 trace.* 并回传。以下摘录省略地址及重复的 wait4=0，父 PID 42939、子宿主 PID 42940：

```text
# trace.42939（父）
clone(child_stack=..., flags=CLONE_NEWNS|CLONE_NEWUTS|CLONE_NEWPID|SIGCHLD) = 42940
sendto(5, "G", 1, MSG_NOSIGNAL, NULL, 0) = 1
wait4(42940, ..., WNOHANG, NULL) = 0
...
wait4(42940, [{WIFEXITED(s) && WEXITSTATUS(s) == 0}], WNOHANG, NULL) = 42940

# trace.42940（子）
read(7, "G", 1) = 1
mount(NULL, "/", NULL, MS_REC|MS_PRIVATE, NULL) = 0
sethostname("minic-trace", 11) = 0
chroot(".../.local/isolation-rootfs") = 0
chdir("/") = 0
mount("minic-dev", "/dev", "tmpfs", MS_NOSUID|MS_NOEXEC, "mode=0755,size=64k") = 0
mknodat(AT_FDCWD, "/dev/null", S_IFCHR|0666, makedev(0x1, 0x3)) = 0
mount("proc", "/proc", "proc", MS_NOSUID|MS_NODEV|MS_NOEXEC, NULL) = 0
execve("/bin/hostname", ["/bin/hostname"], ...) = 0
```

依据代码顺序与 trace 的控制字节，父 clone 后发送 G，子 read 到 G 后才准备隔离；private 挂载传播先于其他挂载；chroot 后 chdir /，随后建立私有设备/proc，再 exec。父的 wait4=0 是子尚未结束时的非阻塞轮询，最终返回子 PID 并确认退出值 0。两个文件分别保存进程内顺序；本次未开启跨文件统一时间戳，不将文本拼接当作完整时间线。

result.json：mode=isolate、kind=exited、host_pid=42940、exit_code=0，PID/UTS/Mount/Rootfs enabled=true，cgroup=false，cleanup.complete=true，无错误。此观察与 native-verification.md 的被测实现对应（本地 HEAD 66664782a71de911c9e5cd92e561e4c46e8ad1cb），没有重新构建或修改实现。

本人操作 PASS；上述机制解释由 Agent 根据 A 回传的实际 trace 整理，不冒写成 A 的逐字答题。完整原始日志保留 artifacts/isolation/manual，未新增文字确认门禁。
