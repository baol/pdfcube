#!/usr/bin/env python3
import pathlib
import subprocess
import sys
import tempfile

binary = pathlib.Path(sys.argv[1])
expected_version = sys.argv[2]

def run(*arguments, home=None):
    environment = None
    if home is not None:
        import os
        environment = os.environ.copy()
        environment["HOME"] = str(home)
    return subprocess.run([binary, *arguments], text=True, capture_output=True, env=environment)

help_result = run("--help")
assert help_result.returncode == 0, help_result.stderr
for option in ("--bgcolor", "--top-color", "--demo", "--program",
               "--performance-log", "--no-fullscreen"):
    assert option in help_result.stdout, option

version = run("--version")
assert version.returncode == 0
assert version.stdout == f"pdfcube {expected_version}\n"

short_help = run("-h")
assert short_help.returncode == 0
assert short_help.stdout == help_result.stdout

short_version = run("-v")
assert short_version.returncode == 0
assert short_version.stdout == version.stdout

for arguments in (
    ("-b", "0:0:0", "--help"),
    ("-t", "0.7:0.6:0.6", "--help"),
    ("-n", "--help"),
):
    short_option = run(*arguments)
    assert short_option.returncode == 0, (arguments, short_option.stderr)

short_input = run("-i", "unused.pdf", "--help")
assert short_input.returncode == 0, short_input.stderr

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


for program in ("inf:c", "nan:p", "1e100:c", "-1:c", "abc:c", "5:blind",
                "5", "5:c:", "5:c:bad:p", "5:c:3:", "", "5:c:1e100:p"):
    invalid = run("--program", program, "unused.pdf")
    assert invalid.returncode == 2, (program, invalid.returncode, invalid.stderr)
    assert "program" in invalid.stderr, invalid.stderr

for arguments in (("--bgcolor", "nan:0:0"), ("--snapshot-progress", "nan"),
                  ("--snapshot-progress", "inf")):
    invalid = run(*arguments)
    assert invalid.returncode == 2, (arguments, invalid.stderr)

with tempfile.TemporaryDirectory() as directory:
    home = pathlib.Path(directory)
    (home / ".pdfcuberc").write_text("unknown-setting=true\n")
    for argument in ("--help", "--version"):
        result = run(argument, home=home)
        assert result.returncode == 0, result.stderr

with tempfile.TemporaryDirectory() as directory:
    home = pathlib.Path(directory)
    for content in ("unknown-setting=true\n", "bgcolor=nan:0:0\n", "demo=invalid\n"):
        (home / ".pdfcuberc").write_text(content)
        invalid = run(home=home)
        assert invalid.returncode == 2, invalid.stderr
        assert "Configuration file parsing error" in invalid.stderr, invalid.stderr

for switch in ("--no-fullscreen", "-n", "--demo"):
    result = run(switch, "unused.pdf", "--help")
    assert result.returncode == 0, (switch, result.stderr)

assert run("--help", "--", "--demo").returncode == 0
