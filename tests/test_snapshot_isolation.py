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

# Normal interactive launches must also remain separate on the same session bus.
import signal
import time
with tempfile.TemporaryDirectory() as directory:
    processes = []
    logs = []
    try:
        for index, pdf in enumerate((first_pdf, second_pdf)):
            log = open(Path(directory) / f"launch-{index}.log", "w+")
            logs.append(log)
            processes.append(subprocess.Popen(
                [xvfb, "-a", binary, "--no-fullscreen", pdf],
                env=environment, stdout=log, stderr=log,
                start_new_session=True,
            ))
            time.sleep(1)
        time.sleep(1)
        for process, log in zip(processes, logs):
            if process.poll() is not None:
                log.seek(0)
                raise AssertionError(f"interactive launch exited {process.returncode}: {log.read()}")
    finally:
        for process in processes:
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            process.wait(timeout=10)
        for log in logs:
            log.close()
