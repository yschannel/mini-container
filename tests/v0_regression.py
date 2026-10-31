#!/usr/bin/env python3
"""Run the V0 CLI and check shell status against the persisted result."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="minic-v0-") as temp:
    base = Path(temp)

    def check(name, program, status, kind, step=None):
        evidence = base / name
        command = [str(binary), "run", "--mode", "v0", "--evidence-dir", str(evidence), "--", *program]
        completed = subprocess.run(command, text=True, capture_output=True, timeout=10)
        result = json.loads((evidence / "result.json").read_text())
        actual = (completed.returncode, result["kind"],
                  (result["primary_error"] or {}).get("step"))
        expected = (status, kind, step)
        assert actual == expected, f"{name}: {actual} != {expected}; stderr={completed.stderr}"
        assert result["schema_version"] == 1
        assert result["mode"] == "v0"
        return completed, result

    check("true", ["/bin/true"], 0, "exited")
    check("exit7", ["/bin/sh", "-c", "exit 7"], 7, "exited")
    check("missing", [str(base / "no-such-program")], 127, "startup_error", "exec")
    check("payload125", ["/bin/sh", "-c", "exit 125"], 125, "exited")
    check("payload127", ["/bin/sh", "-c", "exit 127"], 127, "exited")
    script = base / "not-executable"
    script.write_text("#!/bin/sh\nexit 0\n")
    script.chmod(0o644)
    check("permission", [str(script)], 126, "startup_error", "exec")
    output, _ = check("spaces", ["/bin/sh", "-c", 'test "$1" = "a b"', "sh", "a b"], 0, "exited")
    assert output.returncode == 0
    check("stdin", ["/bin/sh", "-c", "test ! -t 0 && test ! -s /dev/stdin"], 0, "exited")

    evidence = base / "interrupt"
    process = subprocess.Popen([str(binary), "run", "--mode", "v0", "--evidence-dir",
                                str(evidence), "--", "/bin/sleep", "30"],
                               stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    try:
        import time
        deadline = time.monotonic() + 5
        while not (evidence / "result.json").exists() and process.poll() is None:
            # Wait for the child PID to appear before interrupting the runner.
            children = Path(f"/proc/{process.pid}/task/{process.pid}/children")
            if children.exists() and children.read_text().strip():
                break
            if time.monotonic() > deadline:
                raise AssertionError("child did not start")
            time.sleep(0.02)
        os.kill(process.pid, 2)
        process.communicate(timeout=5)
        result = json.loads((evidence / "result.json").read_text())
        assert process.returncode == 137 and result["kind"] == "signaled", result
        assert result["cleanup"]["complete"] is True, result
    finally:
        if process.poll() is None:
            process.kill()
            process.communicate()

print("V0 CLI regression passed")
