#!/usr/bin/env python3
import hashlib
import os
import pathlib
import subprocess
import sys
import tempfile

xvfb, binary, fixture = map(pathlib.Path, sys.argv[1:4])

def snapshot(path, *extra, home=None):
    environment = os.environ.copy()
    if home is not None:
        environment["HOME"] = str(home)
    result = subprocess.run(
        [xvfb, "-a", binary, "--no-fullscreen", "--snapshot", path, *extra, fixture],
        text=True,
        capture_output=True,
        timeout=30,
        env=environment,
    )
    assert result.returncode == 0, result.stderr
    data = pathlib.Path(path).read_bytes()
    assert data.startswith(b"\x89PNG\r\n\x1a\n")
    return hashlib.sha256(data).hexdigest()

with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    first = snapshot(root / "first.png")
    repeated = snapshot(root / "repeated.png")
    assert first == repeated, "software-rendered snapshots must be deterministic"
    colored = snapshot(root / "colored.png", "--bgcolor", "0:0:1")
    assert colored != first, "background color must affect rendered output"
    (root / ".pdfcuberc").write_text("bgcolor=0:1:0\n")
    overridden = snapshot(
        root / "overridden.png", "--bgcolor", "0:0:1", home=root
    )
    assert overridden == colored, "command line options must override .pdfcuberc"
    transition = snapshot(
        root / "transition.png",
        "--program", "1:c",
        "--snapshot-progress", "0.5",
        "--top-color", "0:1:0",
    )
    assert transition not in (first, colored), "cube transition frame must be distinct"
    plain = snapshot(
        root / "plain.png",
        "--program", "1:p",
        "--snapshot-progress", "0.5",
    )
    assert plain == first, "plain programmed transitions must not render the cube"
    alternate_top = snapshot(
        root / "alternate-top.png",
        "--program", "1:c",
        "--snapshot-progress", "0.5",
        "--top-color", "1:0:0",
    )
    assert alternate_top != transition, "top color must affect the cube transition"


