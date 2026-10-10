#!/usr/bin/env python3
import pathlib
import subprocess
import sys
import tempfile

binary = pathlib.Path(sys.argv[1])

def run(*arguments, home=None):
    environment = None
    if home is not None:
        import os
        environment = os.environ.copy()
        environment["HOME"] = str(home)
    return subprocess.run([binary, *arguments], text=True, capture_output=True, env=environment)

help_result = run("--help")
assert help_result.returncode == 0, help_result.stderr
for option in ("--bgcolor", "--top-color", "--demo", "--program", "--no-fullscreen"):
    assert option in help_result.stdout, option

version = run("--version")
assert version.returncode == 0
assert version.stdout == "pdfcube 0.1.0\n"

for arguments in (("--unknown",), ("--bgcolor", "1:2"), ("--top-color", "2:0:0")):
    invalid = run(*arguments)
    assert invalid.returncode == 2, (arguments, invalid.returncode, invalid.stderr)
    assert "Command line option parsing error" in invalid.stderr

with tempfile.TemporaryDirectory() as directory:
    home = pathlib.Path(directory)
    (home / ".pdfcuberc").write_text(
        "bgcolor=0.1:0.2:0.3\nno-fullscreen=true\n"
    )
    configured = run("--help", home=home)
    assert configured.returncode == 0

