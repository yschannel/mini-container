#!/usr/bin/env bash
set -u

output=
if [[ $# == 2 && $1 == --output ]]; then
    output=$2
elif [[ $# != 0 ]]; then
    printf 'usage: %s [--output PATH]\n' "$0" >&2
    exit 2
fi

report() {
    local name=$1
    shift
    printf '\n[%s]\n' "$name"
    "$@" 2>&1 || printf 'unavailable: %s (exit %s)\n' "$name" "$?"
}

collect() {
    report os_release cat /etc/os-release
    report kernel uname -r
    report architecture uname -m
    report identity id
    report virtualization systemd-detect-virt
    report systemd systemctl --version
    for tool in git c++ cmake gdb strace file readelf busybox; do
        if command -v "$tool" >/dev/null 2>&1; then
            printf '%s: %s\n' "$tool" "$(command -v "$tool")"
        else
            printf '%s: MISSING\n' "$tool"
        fi
    done
    report cgroup2_mount findmnt -t cgroup2
    report self_cgroup cat /proc/self/cgroup
    report root_controllers cat /sys/fs/cgroup/cgroup.controllers
    report root_subtree_control cat /sys/fs/cgroup/cgroup.subtree_control
    local membership current_dir
    membership=$(sed -n 's/^0:://p' /proc/self/cgroup | head -n 1)
    current_dir="/sys/fs/cgroup${membership}"
    if [[ -d $current_dir ]]; then
        report current_controllers cat "$current_dir/cgroup.controllers"
        report current_subtree_control cat "$current_dir/cgroup.subtree_control"
    else
        printf 'current_cgroup_path=not_visible (%s)\n' "$current_dir"
    fi
    if command -v busybox-static >/dev/null 2>&1; then
        report busybox_static_file file "$(command -v busybox-static)"
        report busybox_static_dynamic readelf -d "$(command -v busybox-static)"
    elif command -v busybox >/dev/null 2>&1; then
        report busybox_file file "$(command -v busybox)"
        report busybox_dynamic readelf -d "$(command -v busybox)"
    fi
    printf '\n[capability_summary]\n'
    if command -v c++ >/dev/null && command -v cmake >/dev/null; then
        printf 'build_tools=present (build still requires verification)\n'
    else
        printf 'build_tools=missing\n'
    fi
    if findmnt -n -t cgroup2 >/dev/null 2>&1; then
        printf 'cgroup2=mounted\n'
    else
        printf 'cgroup2=not_mounted\n'
    fi
    if [[ -d $current_dir && -w $current_dir/cgroup.subtree_control ]]; then
        printf 'current_subtree_control=writable (delegation still unverified)\n'
    else
        printf 'current_subtree_control=not_writable_or_not_visible\n'
    fi
    printf 'v1_delegation=UNVERIFIED (requires writable delegated subtree and privileged test)\n'
}

if [[ -n $output ]]; then
    mkdir -p "$(dirname "$output")" || exit 1
    collect >"$output" || exit 1
else
    collect
fi
