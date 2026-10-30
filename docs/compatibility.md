# 环境兼容性记录

| 系统 | 被测代码 | 骨架构建 | 隔离/资源限制 | 备注 |
|---|---|---|---|---|
| openSUSE Tumbleweed 20261028, x86_64, kernel 7.2.11-1-default | S01 分支，SHA 见 S01 报告 | PASS | NOT_RUN | 当前 Agent 环境为 container-other；cgroup2 挂载只读，root 控制器有 cpu/memory，但当前 subtree 只启用 pids。委派待实机验证。 |
| Ubuntu 24.04.4, x86_64 | S01 `73bf10741497796e233e3cc3f729ee612f446439` | PASS（A 本人回传） | NOT_RUN | 配置、构建、帮助、版本符合骨架预期；公共头目标构建通过。 |

openSUSE 同时安装动态 `/usr/bin/busybox` 和静态 `/usr/bin/busybox-static`。构建 Rootfs 时应选经实际确认的静态版本；本阶段不制作 Rootfs。
