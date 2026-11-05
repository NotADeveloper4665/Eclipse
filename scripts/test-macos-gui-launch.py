#!/usr/bin/env python3
"""Launch the packaged GUI app and verify it survives real application startup."""
import pathlib
import subprocess
import sys
import time

app = pathlib.Path(sys.argv[1]).resolve()
if not (app / "Contents/Info.plist").is_file():
    raise SystemExit(f"App bundle missing Info.plist: {app}")
subprocess.run(["open", "-n", str(app)], check=True)
deadline = time.monotonic() + 25
pid = None
while time.monotonic() < deadline:
    result = subprocess.run(["pgrep", "-x", "Eclipse"], capture_output=True, text=True)
    if result.returncode == 0 and result.stdout.strip():
        pid = result.stdout.strip().splitlines()[0]
        break
    time.sleep(1)
if pid is None:
    raise SystemExit("Eclipse GUI process did not remain running after launch")
print(f"Eclipse GUI launch passed; process {pid} remained alive")
subprocess.run(["kill", "-TERM", pid], check=False)
