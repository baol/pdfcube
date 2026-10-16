"""One native headless context per test; explicit progress, never real-time waits."""
import os
from pathlib import Path
import subprocess
import sys
import time


class Frames:
    def __init__(self, driver, root, slo=.45, environment=None):
        self.started = time.perf_counter()
        self.slo = slo
        cache = os.environ.get("XDG_CACHE_HOME", str(Path(driver).resolve().parent/".pdfcube-test-cache"))
        self.process = subprocess.Popen(
            [str(Path(driver).resolve())], text=True, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env=dict(os.environ, HOME=str(root), XDG_CACHE_HOME=cache,
                     MESA_SHADER_CACHE_DIR=str(Path(cache)/"mesa"), LIBGL_ALWAYS_SOFTWARE="1",
                     **(environment or {})))
        assert self.process.stdout.readline().strip() == "READY", self.process.stderr.read()

    def render(self, *arguments):
        def quote(value):
            return '"' + str(value).replace('\\', '\\\\').replace('"', '\\"') + '"'
        self.process.stdin.write(" ".join(map(quote, arguments)) + "\n")
        self.process.stdin.flush()
        reply = self.process.stdout.readline().strip()
        assert reply == "OK", reply

    def close(self):
        self.process.stdin.close()
        self.process.stdin = None
        _, errors = self.process.communicate(timeout=10)
        assert self.process.returncode == 0, errors
        elapsed = time.perf_counter()-self.started
        if elapsed > self.slo:
            print(f"SLO miss: frame test took {elapsed:.3f}s (target {self.slo:.3f}s)",
                  file=sys.stderr)
