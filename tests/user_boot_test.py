#!/usr/bin/env python3
"""Milestone 1: boot SableOS with a from-source static Linux ELF loaded as a
Multiboot2 module and confirm it executes in ring 3, exercises the Linux
syscall ABI, exits cleanly, and hands control back to the kernel shell."""
import shutil
import subprocess
import sys

BOOT_TIMEOUT = 20

if len(sys.argv) != 2:
    raise SystemExit("usage: user_boot_test.py ISO")
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
    output = result.stdout
except subprocess.TimeoutExpired as error:
    output = error.stdout or ""
    if isinstance(output, bytes):
        output = output.decode(errors="replace")

required = [
    "SableOS 0.1",                                  # kernel booted
    "[sableos] exec module 0: hello",               # loader launched the module
    "Hello from ring 3 (SableOS Linux ABI)",        # ring-3 code ran
    "user syscalls OK; exiting via exit_group(0)",  # syscall ABI worked
    "[sableos] exit status 0",                      # clean exit, kernel resumed
    "sable:/$",                                     # shell came back
]
forbidden = ["[user fault]", "[kernel fault]", "unimplemented syscall"]

missing = [text for text in required if text not in output]
present_bad = [text for text in forbidden if text in output]
if missing or present_bad:
    print(output)
    if missing:
        print(f"user boot test failed; missing: {missing}")
    if present_bad:
        print(f"user boot test failed; unexpected: {present_bad}")
    raise SystemExit(1)

print("Ring-3 user ELF boot test passed (Milestone 1)")
