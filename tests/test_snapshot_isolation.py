#!/usr/bin/env python3
"""Two snapshot processes sharing one D-Bus session must remain independent."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile

xvfb, binary, first_pdf, second_pdf = map(lambda value: str(Path(value).resolve()), sys.argv[1:])
environment = dict(os.environ, GDK_BACKEND="x11", LIBGL_ALWAYS_SOFTWARE="1")

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    outputs = (root / "first.png", root / "second.png")
    processes = [
        subprocess.Popen(
            [xvfb, "-a", binary, "--no-fullscreen", "--snapshot", str(output), pdf],
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        for output, pdf in zip(outputs, (first_pdf, second_pdf))
    ]
    for process, output in zip(processes, outputs):
        stdout, stderr = process.communicate(timeout=30)
        assert process.returncode == 0, (
            f"snapshot exited {process.returncode}\n"
            f"stdout:\n{stdout.decode(errors='replace')}\n"
            f"stderr:\n{stderr.decode(errors='replace')}"
        )
        assert output.is_file(), (
            "successful snapshot process produced no PNG; GtkApplication "
            "probably forwarded activation to the concurrent process"
        )
    assert outputs[0].read_bytes() != outputs[1].read_bytes(), (
        "concurrent snapshots did not render their independent documents"
    )
