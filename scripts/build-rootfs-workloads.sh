#!/usr/bin/env bash
set -euo pipefail
die() { printf 'build-rootfs-workloads: %s\n' "$*" >&2; exit 1; }
[[ $# == 2 && ${1:-} == --rootfs ]] || die 'usage: ./scripts/build-rootfs-workloads.sh --rootfs ABSOLUTE_EXISTING_ROOTFS'
rootfs=$2
[[ $rootfs == /* && $rootfs != / ]] || die 'rootfs must be an absolute dedicated directory'
[[ -d $rootfs && ! -L $rootfs && -d $rootfs/bin && ! -L $rootfs/bin ]] || die 'rootfs and bin must be existing real directories'
rootfs=$(cd -- "$rootfs" && pwd -P)
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
project_dir=$(cd -- "$script_dir/.." && pwd -P)
[[ $rootfs == "$project_dir"/.local/* || $rootfs == "$project_dir"/rootfs/* || $rootfs == "$project_dir"/Rootfs/* ]] || die 'rootfs must be within this project'
source_file=$script_dir/../tests/workloads/resource_workload.c
target=$rootfs/bin/minic-workload
[[ ! -e $target && ! -L $target ]] || die "target already exists: $target"
temp=$(mktemp "$rootfs/bin/.minic-workload.XXXXXX")
trap 'rm -f -- "$temp"' EXIT
if ! cc -std=c11 -O2 -Wall -Wextra -Wpedantic -static "$source_file" -o "$temp"; then
    die 'static build unavailable; install the matching static libc toolchain or report the failure'
fi
chmod 0755 "$temp"
file "$temp"
if readelf -l "$temp" | grep -q INTERP; then die 'binary unexpectedly has an ELF interpreter'; fi
ln -- "$temp" "$target" || die "target appeared while building: $target"
rm -- "$temp"
printf 'installed=%s\n' "$target"
