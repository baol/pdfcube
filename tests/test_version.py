#!/usr/bin/env python3
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile


version_script = pathlib.Path(sys.argv[1]).resolve()

environment = os.environ.copy()
environment["PDFCUBE_VERSION"] = "9.8.7"
configured = subprocess.run(
    [sys.executable, version_script],
    check=True,
    capture_output=True,
    text=True,
    env=environment,
)
assert configured.stdout.strip() == "9.8.7"

with tempfile.TemporaryDirectory() as directory:
    archive_root = pathlib.Path(directory) / "pdfcube-7.6.5+4.gabcdef"
    tools = archive_root / "tools"
    tools.mkdir(parents=True)
    archived_script = tools / "version.py"
    shutil.copyfile(version_script, archived_script)
    environment.pop("PDFCUBE_VERSION", None)
    archived = subprocess.run(
        [sys.executable, archived_script],
        check=True,
        capture_output=True,
        text=True,
        env=environment,
    )
    assert archived.stdout.strip() == "7.6.5+4.gabcdef"
