#!/usr/bin/env python3
import shutil
import subprocess
import sys

BOOT_TIMEOUT = 12

if len(sys.argv) != 2:
    raise SystemExit("usage: boot_test.py ISO")
if not shutil.which("qemu-system-x86_64"):
    raise SystemExit("qemu-system-x86_64 is required")

command = [
    "qemu-system-x86_64", "-cdrom", sys.argv[1], "-smp", "2", "-m", "128M",
    "-display", "none", "-serial", "stdio", "-no-reboot",
]
try:
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            timeout=BOOT_TIMEOUT, check=False)
except subprocess.TimeoutExpired as error:
    output = error.stdout or ""
    if isinstance(output, bytes):
        output = output.decode(errors="replace")
else:
    output = result.stdout

required = ["SableOS 0.1", "Ready. 2 CPU(s) discovered, 2 online", "sable:/$"]
missing = [text for text in required if text not in output]
if missing:
    print(output)
    raise SystemExit(f"boot test failed; missing: {missing}")
print("Functional boot test passed")
