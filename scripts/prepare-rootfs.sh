#!/usr/bin/env bash
set -eu
exec python3 - "$@" <<'PY'
import os
from pathlib import Path
import platform
import shutil
import struct
import subprocess
import sys

if len(sys.argv) != 3 or sys.argv[1] != '--output':
    sys.exit('usage: scripts/prepare-rootfs.sh --output ABSOLUTE_PATH_UNDER_PROJECT/.local')
target = Path(sys.argv[2])
project = Path.cwd().resolve()
if not target.is_absolute() or '..' in target.parts:
    sys.exit('target must be absolute without ..')
# Limit this teaching helper to the repository local assets area, protecting system directories.
allowed = project / '.local'
if target == allowed or allowed not in target.parents:
    sys.exit('target must be a dedicated child of ' + str(allowed))
for part in [target, *target.parents]:
    if part.is_symlink(): sys.exit('symlink in target path: ' + str(part))
if target.exists(): sys.exit('refusing existing target; no files were overwritten or removed')
busybox = shutil.which('busybox-static') or shutil.which('busybox')
if not busybox: sys.exit('BusyBox unavailable; install a static BusyBox package first')
busybox = str(Path(busybox).resolve())
header = Path(busybox).read_bytes()[:20]
if header[:4] != b'\x7fELF': sys.exit('BusyBox is not ELF')
machine = struct.unpack(('<' if header[5] == 1 else '>') + 'H', header[18:20])[0]
expected = {'x86_64': 62, 'aarch64': 183}.get(platform.machine())
if expected is None or machine != expected: sys.exit('BusyBox architecture mismatch or unsupported host architecture')
file_info = subprocess.check_output(['file', busybox], text=True)
programs = subprocess.check_output(['readelf', '-l', busybox], text=True)
dynamic = subprocess.check_output(['readelf', '-d', busybox], text=True)
print(file_info, end='')
print(dynamic, end='')
if 'INTERP' in programs or '(NEEDED)' in dynamic or 'statically linked' not in file_info:
    sys.exit('dynamic BusyBox is unsupported by this helper; loader/libraries must not be silently omitted')
print('Creating dedicated Rootfs:', target)
print('Writing bin/busybox, applet symlinks, proc/, dev/, tmp/, .minic-rootfs')
target.parent.mkdir(parents=True, exist_ok=True)
target.mkdir()  # Exclusive; no cleanup of unknown pre-existing paths.
for name in ('bin', 'proc', 'dev', 'tmp'): (target / name).mkdir()
(target / 'tmp').chmod(0o1777)
shutil.copyfile(busybox, target / 'bin/busybox')
(target / 'bin/busybox').chmod(0o755)
for name in ('sh', 'hostname', 'ps', 'ls', 'cat', 'echo', 'sleep', 'true', 'false', 'test', 'readlink', 'kill'):
    (target / 'bin' / name).symlink_to('busybox')
(target / '.minic-rootfs').write_text('minic-rootfs-v1\n')
print('Rootfs ready; no mount or cgroup changes performed')
PY
