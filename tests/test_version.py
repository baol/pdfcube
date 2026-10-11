#!/usr/bin/env python3
import os
import importlib.util
import pathlib
import shutil
import subprocess
import sys
import tempfile


sys.dont_write_bytecode = True
version_script = pathlib.Path(sys.argv[1]).resolve()
next_version_script = pathlib.Path(sys.argv[2]).resolve()

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

spec = importlib.util.spec_from_file_location("next_version", next_version_script)
next_version_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(next_version_module)

cases = (
    ("0.1.0", [], (0, 1, 0)),
    ("0.1.0", ["v0.1.0"], (0, 1, 1)),
    ("0.2.0", ["v0.1.9"], (0, 2, 0)),
    ("1.0.0", ["v0.9.12"], (1, 0, 0)),
    ("0.1.0", ["v2.3.4", "v2.4.0-rc1", "not-a-version"], (2, 3, 5)),
    ("2.3.10", ["v2.3.4"], (2, 3, 10)),
)
for minimum, tags, expected in cases:
    assert next_version_module.next_version(minimum, tags) == expected
