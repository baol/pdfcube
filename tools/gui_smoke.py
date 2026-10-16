#!/usr/bin/env python3
"""One real GTK snapshot, not the multi-frame human-review gallery."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import time

from PIL import Image


def smoke(binary, fixture, cache=None):
    started = time.perf_counter()
    binary = Path(binary).resolve()
    cache = Path(cache).resolve() if cache else binary.parent / "gui-smoke-cache"
    cache.mkdir(parents=True, exist_ok=True)
    fonts = Path(__file__).resolve().parents[1] / "tests" / "fonts.conf"
    with tempfile.TemporaryDirectory() as directory:
        output = Path(directory) / "snapshot.png"
        subprocess.run([
            "xvfb-run", "-a", str(binary), "--no-fullscreen",
            "--snapshot", str(output), str(Path(fixture).resolve()),
        ], check=True, timeout=10, env=dict(
            os.environ, GDK_BACKEND="x11", LIBGL_ALWAYS_SOFTWARE="1",
            LP_NUM_THREADS="1", GTK_A11Y="none", HOME=directory,
            FONTCONFIG_FILE=str(fonts), XDG_CACHE_HOME=str(cache),
            MESA_SHADER_CACHE_DIR=str(cache / "mesa")))
        with Image.open(output) as image:
            assert image.format == "PNG", "snapshot is not PNG"
            image.load()
            assert min(image.size) > 0, "snapshot is empty"
            assert any(low != high for low, high in image.convert("RGB").getextrema()), \
                "snapshot contains no fixture content"
    elapsed = time.perf_counter()-started
    print(f"GUI snapshot smoke: {elapsed*1000:.1f} ms (SLO 500 ms)")
    if elapsed > .5:
        print("SLO MISS: GUI snapshot smoke exceeded 500 ms")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--cache", type=Path, help="Share the native tests' font/shader cache")
    args = parser.parse_args()
    smoke(args.binary, args.fixture, args.cache)
