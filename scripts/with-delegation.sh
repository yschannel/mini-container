#!/usr/bin/env bash
set -euo pipefail

# Create one transient systemd scope for a single resource probe. The scope is
# delegated to the invoking user; all cgroup writes stay below that scope.
die() { printf 'with-delegation: %s\n' "$*" >&2; exit 1; }
has_controller() { [[ " $(<"$1/cgroup.controllers") " == *" $2 "* ]]; }
enabled_controller() { [[ " $(<"$1/cgroup.subtree_control") " == *" $2 "* ]]; }

if [[ ${1:-} == --inside ]]; then
    [[ $# == 5 ]] || die 'internal invocation requires scope unit, mode, uid, gid'
    unit=$2
    mode=$3
    user_uid=$4
    user_gid=$5
    [[ $unit =~ ^minic-b2-[0-9]+-[0-9]+\.scope$ ]] || die 'unexpected scope name'
    [[ $mode == smoke || $mode == attach-fail || $mode == baseline || $mode == cpu || $mode == memory || $mode == all ]] || die 'unknown probe mode'
    [[ $(id -u) == 0 ]] || die 'scope setup requires root'
    [[ $user_uid =~ ^[0-9]+$ && $user_uid != 0 && $user_gid =~ ^[0-9]+$ ]] || die 'invalid invoking user'
    getent passwd "$user_uid" >/dev/null || die 'invoking user is unknown'
    relative=$(sed -n 's/^0:://p' /proc/self/cgroup)
    [[ $relative == /* && ${relative##*/} == "$unit" ]] || die 'not inside expected scope'
    base=/sys/fs/cgroup$relative
    [[ -d $base ]] || die 'scope cgroup is missing'
    for controller in cpu memory; do
        has_controller "$base" "$controller" || die "$controller is not delegated to $base"
    done
    manager=$base/manager
    jobs=$base/jobs
    mkdir -- "$manager"
    printf '%s\n' "$$" > "$manager/cgroup.procs"
    [[ ! -s $base/cgroup.procs ]] || die 'scope root still has processes'
    printf '+cpu +memory\n' > "$base/cgroup.subtree_control"
    for controller in cpu memory; do
        enabled_controller "$base" "$controller" || die "could not enable $controller in scope"
    done
    mkdir -- "$jobs"
    printf '+cpu +memory\n' > "$jobs/cgroup.subtree_control"
    for controller in cpu memory; do
        enabled_controller "$jobs" "$controller" || die "could not enable $controller in jobs"
    done
    project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
    probe=$project_dir/build/debug/cgroup_probe
    [[ -x $probe ]] || die "build probe first: $probe"
    # Permit only the invoking user to create children under jobs and migrate
    # their own child from manager to a job leaf. The scope remains managed by
    # systemd and no ancestor cgroup is changed.
    chown -- "$user_uid:$user_gid" "$base/cgroup.procs" "$jobs"
    printf 'delegated_base=%s\n' "$jobs"
    status=0
    modes=("$mode")
    if [[ $mode == all ]]; then modes=(smoke attach-fail baseline cpu memory); fi
    for probe_mode in "${modes[@]}"; do
        printf '\nCASE=%s\n' "$probe_mode"
        if setpriv --reuid="$user_uid" --regid="$user_gid" --init-groups \
            "$probe" "$jobs" "$probe_mode"; then
            printf 'RESULT=%s PASS\n' "$probe_mode"
        else
            result=$?
            printf 'RESULT=%s FAIL exit=%s\n' "$probe_mode" "$result" >&2
            status=1
        fi
    done
    rmdir -- "$jobs" || die "probe left a cgroup below $jobs"
    exit "$status"
fi

[[ $# == 2 && ${1:-} == --probe ]] || die 'usage: ./scripts/with-delegation.sh --probe smoke|attach-fail|baseline|cpu|memory|all'
mode=$2
[[ $mode == smoke || $mode == attach-fail || $mode == baseline || $mode == cpu || $mode == memory || $mode == all ]] || die 'unknown probe mode'
[[ $(id -u) != 0 ]] || die 'invoke as normal user; this script calls sudo for systemd-run'
script=$(readlink -f -- "${BASH_SOURCE[0]}")
unit=minic-b2-$(date +%s)-$$.scope
exec sudo systemd-run --system --scope --unit="$unit" \
    --property='Delegate=cpu memory' \
    --same-dir "$script" --inside "$unit" "$mode" "$(id -u)" "$(id -g)"
